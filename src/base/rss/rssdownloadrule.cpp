/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2010  Christophe Dumez
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * In addition, as a special exception, the copyright holders give permission to
 * link this program with the OpenSSL project's "OpenSSL" library (or with
 * modified versions of it that use the same license as the "OpenSSL" library),
 * and distribute the linked executables. You must obey the GNU General Public
 * License in all respects for all of the code used other than "OpenSSL".  If you
 * modify file(s), you may extend this exception to your version of the file(s),
 * but you are not obligated to do so. If you do not wish to do so, delete this
 * exception statement from your version.
 *
 * Contact : chris@qbittorrent.org
 */

#include "rssdownloadrule.h"
#include "rssdownloadrule_p.h"

#include <QDebug>
#include <QDir>
#include <QHash>
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <QSharedData>

#include "base/preferences.h"
#include "base/utils/fs.h"
#include "base/utils/string.h"
#include "rssfeed.h"
#include "rssarticle.h"

using namespace Rss;

DownloadRule::DownloadRule(const QString &name)
    : d(new DownloadRuleData)
{
    setName(name);
}

DownloadRule::DownloadRule()
    : d(nullptr)
{
}

DownloadRule::~DownloadRule() {}

bool DownloadRule::matches(const QString &articleTitle, const QString &expression) const
{
    static const QRegularExpression whitespace("\\s+");

    if (expression.isEmpty()) {
        // A regex of the form "expr|" will always match, so do the same for wildcards
        return true;
    }
    else if (d->useRegex()) {
        QRegularExpression reg(d->cachedRegex(expression));
        return reg.match(articleTitle).hasMatch();
    }
    else {
        // Only match if every wildcard token (separated by spaces) is present in the article name.
        // Order of wildcard tokens is unimportant (if order is important, they should have used *).
        foreach (const QString &wildcard, expression.split(whitespace, QString::SplitBehavior::SkipEmptyParts)) {
            QRegularExpression reg(d->cachedRegex(wildcard, false));

            if (!reg.match(articleTitle).hasMatch())
                return false;
        }
    }

    return true;
}

bool DownloadRule::matches(const QString &articleTitle) const
{
    if (!d->mustContain().empty()) {
        bool logged = false;
        bool foundMustContain = false;

        // Each expression is either a regex, or a set of wildcards separated by whitespace.
        // Accept if any complete expression matches.
        foreach (const QString &expression, d->mustContain()) {
            if (!logged) {
                qDebug() << "Checking matching" << (d->useRegex() ? "regex:" : "wildcard expressions:") << mustContain();
                logged = true;
            }

            // A regex of the form "expr|" will always match, so do the same for wildcards
            foundMustContain = matches(articleTitle, expression);

            if (foundMustContain) {
                qDebug() << "Found matching" << (d->useRegex() ? "regex:" : "wildcard expression:") << expression;
                break;
            }
        }

        if (!foundMustContain)
            return false;
    }

    if (!d->mustNotContain().empty()) {
        bool logged = false;

        // Each expression is either a regex, or a set of wildcards separated by whitespace.
        // Reject if any complete expression matches.
        foreach (const QString &expression, d->mustNotContain()) {
            if (!logged) {
                qDebug() << "Checking not matching" << (d->useRegex() ? "regex:" : "wildcard expressions:") << mustNotContain();
                logged = true;
            }

            // A regex of the form "expr|" will always match, so do the same for wildcards
            if (matches(articleTitle, expression)) {
                qDebug() << "Found not matching" << (d->useRegex() ? "regex:" : "wildcard expression:") << expression;
                return false;
            }
        }
    }

    if (!d->episodeFilter().isEmpty()) {
        qDebug() << "Checking episode filter:" << d->episodeFilter();
        QRegularExpression f(d->cachedRegex("(^\\d{1,4})x(.+;$)"));
        QRegularExpressionMatch matcher = f.match(d->episodeFilter());
        bool matched = matcher.hasMatch();

        if (!matched)
            return false;

        QString s = matcher.captured(1);
        QStringList eps = matcher.captured(2).split(";");
        int sOurs = s.toInt();

        foreach (QString ep, eps) {
            if (ep.isEmpty())
                continue;

            // We need to trim leading zeroes, but if it's all zeros then we want episode zero.
            while (ep.size() > 1 && ep.startsWith("0"))
                ep = ep.right(ep.size() - 1);

            if (ep.indexOf('-') != -1) { // Range detected
                QString partialPattern1 = "\\bs0?(\\d{1,4})[ -_\\.]?e(0?\\d{1,4})(?:\\D|\\b)";
                QString partialPattern2 = "\\b(\\d{1,4})x(0?\\d{1,4})(?:\\D|\\b)";
                QRegularExpression reg(d->cachedRegex(partialPattern1));

                if (ep.endsWith('-')) { // Infinite range
                    int epOurs = ep.left(ep.size() - 1).toInt();

                    // Extract partial match from article and compare as digits
                    matcher = reg.match(articleTitle);
                    matched = matcher.hasMatch();

                    if (!matched) {
                        reg = QRegularExpression(d->cachedRegex(partialPattern2));
                        matcher = reg.match(articleTitle);
                        matched = matcher.hasMatch();
                    }

                    if (matched) {
                        int sTheirs = matcher.captured(1).toInt();
                        int epTheirs = matcher.captured(2).toInt();
                        if (((sTheirs == sOurs) && (epTheirs >= epOurs)) || (sTheirs > sOurs)) {
                            qDebug() << "Matched episode:";
                            qDebug() << "    Filter: " << s << "x" << ep;
                            qDebug() << "    Article:" << sTheirs << "x" << epTheirs;
                            qDebug() << "Matched article:" << articleTitle;
                            return true;
                        }
                    }
                }
                else { // Normal range
                    QStringList range = ep.split('-');
                    Q_ASSERT(range.size() == 2);
                    if (range.first().toInt() > range.last().toInt())
                        continue; // Ignore this subrule completely

                    int epOursFirst = range.first().toInt();
                    int epOursLast = range.last().toInt();

                    // Extract partial match from article and compare as digits
                    matcher = reg.match(articleTitle);
                    matched = matcher.hasMatch();

                    if (!matched) {
                        reg = QRegularExpression(d->cachedRegex(partialPattern2));
                        matcher = reg.match(articleTitle);
                        matched = matcher.hasMatch();
                    }

                    if (matched) {
                        int sTheirs = matcher.captured(1).toInt();
                        int epTheirs = matcher.captured(2).toInt();
                        if ((sTheirs == sOurs) && ((epOursFirst <= epTheirs) && (epOursLast >= epTheirs))) {
                            qDebug() << "Matched episode:";
                            qDebug() << "    Filter: " << s << "x" << ep;
                            qDebug() << "    Article:" << sTheirs << "x" << epTheirs;
                            qDebug() << "Matched article:" << articleTitle;
                            return true;
                        }
                    }
                }
            }
            else { // Single number
                QString expStr("\\b(?:s0?" + s + "[ -_\\.]?" + "e0?" + ep + "|" + s + "x" + "0?" + ep + ")(?:\\D|\\b)");
                QRegularExpression reg(d->cachedRegex(expStr));
                if (reg.match(articleTitle).hasMatch()) {
                    qDebug() << "Matched episode:" << s << "x" << ep;
                    qDebug() << "Matched article:" << articleTitle;
                    return true;
                }
            }
        }

        return false;
    }

    qDebug() << "Matched article:" << articleTitle;
    return true;
}

void DownloadRule::setMustContain(const QString &tokens)
{
    d->setMustContain(tokens);
}

void DownloadRule::setMustNotContain(const QString &tokens)
{
    d->setMustNotContain(tokens);
}

QStringList DownloadRule::rssFeeds() const
{
    return d->rssFeeds;
}

void DownloadRule::setRssFeeds(const QStringList &rssFeeds)
{
    d->rssFeeds = rssFeeds;
}

QString DownloadRule::name() const
{
    return d->name;
}

void DownloadRule::setName(const QString &name)
{
    d->name = name;
}

QString DownloadRule::savePath() const
{
    return d->savePath;
}

DownloadRule DownloadRule::fromVariantHash(const QVariantHash &ruleHash)
{
    DownloadRule rule(ruleHash.value("name").toString());
    rule.setUseRegex(ruleHash.value("use_regex", false).toBool());
    rule.setMustContain(ruleHash.value("must_contain").toString());
    rule.setMustNotContain(ruleHash.value("must_not_contain").toString());
    rule.setEpisodeFilter(ruleHash.value("episode_filter").toString());
    rule.setRssFeeds(ruleHash.value("affected_feeds").toStringList());
    rule.setEnabled(ruleHash.value("enabled", false).toBool());
    rule.setSavePath(ruleHash.value("save_path").toString());
    rule.setCategory(ruleHash.value("category_assigned").toString());
    rule.setAddPaused(AddPausedState(ruleHash.value("add_paused").toUInt()));
    rule.setLastMatch(ruleHash.value("last_match").toDateTime());
    rule.setIgnoreDays(ruleHash.value("ignore_days").toInt());
    return rule;
}

QVariantHash DownloadRule::toVariantHash() const
{
    QVariantHash hash;
    hash["name"] = name();
    hash["must_contain"] = mustContain();
    hash["must_not_contain"] = mustNotContain();
    hash["save_path"] = savePath();
    hash["affected_feeds"] = rssFeeds();
    hash["enabled"] = isEnabled();
    hash["category_assigned"] = category();
    hash["use_regex"] = useRegex();
    hash["add_paused"] = addPaused();
    hash["episode_filter"] = episodeFilter();
    hash["last_match"] = lastMatch();
    hash["ignore_days"] = ignoreDays();
    return hash;
}

bool DownloadRule::operator==(const DownloadRule &other) const
{
    if (d && !other)
        return false;

    return d && name() == other.name();
}

DownloadRule::operator bool() const
{
    return d;
}

void DownloadRule::setSavePath(const QString &savePath)
{
    d->savePath = Utils::Fs::fromNativePath(savePath);
}

DownloadRule::AddPausedState DownloadRule::addPaused() const
{
    return d->apstate;
}

void DownloadRule::setAddPaused(const DownloadRule::AddPausedState &aps)
{
    d->apstate = aps;
}

QString DownloadRule::category() const
{
    return d->category;
}

void DownloadRule::setCategory(const QString &category)
{
    d->category = category;
}

bool DownloadRule::isEnabled() const
{
    return d->enabled;
}

void DownloadRule::setEnabled(bool enable)
{
    d->enabled = enable;
}

void DownloadRule::setLastMatch(const QDateTime &date)
{
    d->lastMatch = date;
}

QDateTime DownloadRule::lastMatch() const
{
    return d->lastMatch;
}

void DownloadRule::setIgnoreDays(int days)
{
    d->ignoreDays = days;
}

int DownloadRule::ignoreDays() const
{
    return d->ignoreDays;
}

QString DownloadRule::mustContain() const
{
    return d->mustContain().join("|");
}

QString DownloadRule::mustNotContain() const
{
    return d->mustNotContain().join("|");
}

bool DownloadRule::useRegex() const
{
    return d->useRegex();
}

void DownloadRule::setUseRegex(bool enabled)
{
    d->setUseRegex(enabled);
}

QString DownloadRule::episodeFilter() const
{
    return d->episodeFilter();
}

void DownloadRule::setEpisodeFilter(const QString &e)
{
    d->setEpisodeFilter(e);
}

QStringList DownloadRule::findMatchingArticles(const FeedPtr &feed) const
{
    QStringList ret;
    const ArticleHash &feedArticles = feed->articleHash();

    foreach (const ArticlePtr &article, feedArticles) {
        const QString title = article->title();
        qDebug() << "Matching article:" << title;
        if (matches(title))
            ret << title;
    }
    return ret;
}

bool DownloadRule::isSame(const DownloadRule &other) const
{
    return d == other.d;
}
