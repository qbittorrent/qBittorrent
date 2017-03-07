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

#include <QDebug>
#include <QDir>
#include <QHash>
#include <QRegExp>
#include <QRegularExpression>
#include <QString>
#include <QStringList>

#include "base/preferences.h"
#include "base/utils/fs.h"
#include "base/utils/string.h"
#include "rssfeed.h"
#include "rssarticle.h"
#include "rssdownloadrule.h"

using namespace Rss;

DownloadRule::DownloadRule()
    : m_enabled(false)
    , m_useRegex(false)
    , m_apstate(USE_GLOBAL)
    , m_ignoreDays(0)
    , m_cachedRegexes(new QHash<QString, QRegularExpression>)
{
}

DownloadRule::~DownloadRule()
{
    delete m_cachedRegexes;
}

QRegularExpression DownloadRule::cachedRegex(const QString &expression, bool isRegex) const
{
    // Use a cache of regexes so we don't have to continually recompile - big performance increase.
    // The cache is cleared whenever the regex/wildcard, must or must not contain fields or
    // episode filter are modified.
    Q_ASSERT(!expression.isEmpty());
    QRegularExpression regex((*m_cachedRegexes)[expression]);

    if (!regex.pattern().isEmpty())
        return regex;

    return (*m_cachedRegexes)[expression] = QRegularExpression(isRegex ? expression : Utils::String::wildcardToRegex(expression), QRegularExpression::CaseInsensitiveOption);
}

bool DownloadRule::matches(const QString &articleTitle, const QString &expression) const
{
    static QRegularExpression whitespace("\\s+");

    if (expression.isEmpty()) {
        // A regex of the form "expr|" will always match, so do the same for wildcards
        return true;
    }
    else if (m_useRegex) {
        QRegularExpression reg(cachedRegex(expression));
        return reg.match(articleTitle).hasMatch();
    }
    else {
        // Only match if every wildcard token (separated by spaces) is present in the article name.
        // Order of wildcard tokens is unimportant (if order is important, they should have used *).
        foreach (const QString &wildcard, expression.split(whitespace, QString::SplitBehavior::SkipEmptyParts)) {
            QRegularExpression reg(cachedRegex(wildcard, false));

            if (!reg.match(articleTitle).hasMatch())
                return false;
        }
    }

    return true;
}

bool DownloadRule::matches(const QString &articleTitle) const
{
    if (!m_mustContain.empty()) {
        bool logged = false;
        bool foundMustContain = false;

        // Each expression is either a regex, or a set of wildcards separated by whitespace.
        // Accept if any complete expression matches.
        foreach (const QString &expression, m_mustContain) {
            if (!logged) {
                qDebug() << "Checking matching" << (m_useRegex ? "regex:" : "wildcard expressions:") << m_mustContain.join("|");
                logged = true;
            }

            // A regex of the form "expr|" will always match, so do the same for wildcards
            foundMustContain = matches(articleTitle, expression);

            if (foundMustContain) {
                qDebug() << "Found matching" << (m_useRegex ? "regex:" : "wildcard expression:") << expression;
                break;
            }
        }

        if (!foundMustContain)
            return false;
    }

    if (!m_mustNotContain.empty()) {
        bool logged = false;

        // Each expression is either a regex, or a set of wildcards separated by whitespace.
        // Reject if any complete expression matches.
        foreach (const QString &expression, m_mustNotContain) {
            if (!logged) {
                qDebug() << "Checking not matching" << (m_useRegex ? "regex:" : "wildcard expressions:") << m_mustNotContain.join("|");
                logged = true;
            }

            // A regex of the form "expr|" will always match, so do the same for wildcards
            if (matches(articleTitle, expression)) {
                qDebug() << "Found not matching" << (m_useRegex ? "regex:" : "wildcard expression:") << expression;
                return false;
            }
        }
    }

    if (!m_episodeFilter.isEmpty()) {
        qDebug() << "Checking episode filter:" << m_episodeFilter;
        QRegularExpression f(cachedRegex("(^\\d{1,4})x(.*;$)"));
        QRegularExpressionMatch matcher = f.match(m_episodeFilter);
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
                QRegularExpression reg(cachedRegex(partialPattern1));

                if (ep.endsWith('-')) { // Infinite range
                    int epOurs = ep.left(ep.size() - 1).toInt();

                    // Extract partial match from article and compare as digits
                    matcher = reg.match(articleTitle);
                    matched = matcher.hasMatch();

                    if (!matched) {
                        reg = QRegularExpression(cachedRegex(partialPattern2));
                        matcher = reg.match(articleTitle);
                        matched = matcher.hasMatch();
                    }

                    if (matched) {
                        int sTheirs = matcher.captured(1).toInt();
                        int epTheirs = matcher.captured(2).toInt();
                        if (((sTheirs == sOurs) && (epTheirs >= epOurs)) || (sTheirs > sOurs)) {
                            qDebug() << "Matched episode:" << ep;
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
                        reg = QRegularExpression(cachedRegex(partialPattern2));
                        matcher = reg.match(articleTitle);
                        matched = matcher.hasMatch();
                    }

                    if (matched) {
                        int sTheirs = matcher.captured(1).toInt();
                        int epTheirs = matcher.captured(2).toInt();
                        if ((sTheirs == sOurs) && ((epOursFirst <= epTheirs) && (epOursLast >= epTheirs))) {
                            qDebug() << "Matched episode:" << ep;
                            qDebug() << "Matched article:" << articleTitle;
                            return true;
                        }
                    }
                }
            }
            else { // Single number
                QString expStr("\\b(?:s0?" + s + "[ -_\\.]?" + "e0?" + ep + "|" + s + "x" + "0?" + ep + ")(?:\\D|\\b)");
                QRegularExpression reg(cachedRegex(expStr));
                if (reg.match(articleTitle).hasMatch()) {
                    qDebug() << "Matched episode:" << ep;
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
    m_cachedRegexes->clear();

    if (m_useRegex)
        m_mustContain = QStringList() << tokens;
    else
        m_mustContain = tokens.split("|");

    // Check for single empty string - if so, no condition
    if ((m_mustContain.size() == 1) && m_mustContain[0].isEmpty())
        m_mustContain.clear();
}

void DownloadRule::setMustNotContain(const QString &tokens)
{
    m_cachedRegexes->clear();

    if (m_useRegex)
        m_mustNotContain = QStringList() << tokens;
    else
        m_mustNotContain = tokens.split("|");

    // Check for single empty string - if so, no condition
    if ((m_mustNotContain.size() == 1) && m_mustNotContain[0].isEmpty())
        m_mustNotContain.clear();
}

QStringList DownloadRule::rssFeeds() const
{
    return m_rssFeeds;
}

void DownloadRule::setRssFeeds(const QStringList &rssFeeds)
{
    m_rssFeeds = rssFeeds;
}

QString DownloadRule::name() const
{
    return m_name;
}

void DownloadRule::setName(const QString &name)
{
    m_name = name;
}

QString DownloadRule::savePath() const
{
    return m_savePath;
}

DownloadRulePtr DownloadRule::fromVariantHash(const QVariantHash &ruleHash)
{
    DownloadRulePtr rule(new DownloadRule);
    rule->setName(ruleHash.value("name").toString());
    rule->setUseRegex(ruleHash.value("use_regex", false).toBool());
    rule->setMustContain(ruleHash.value("must_contain").toString());
    rule->setMustNotContain(ruleHash.value("must_not_contain").toString());
    rule->setEpisodeFilter(ruleHash.value("episode_filter").toString());
    rule->setRssFeeds(ruleHash.value("affected_feeds").toStringList());
    rule->setEnabled(ruleHash.value("enabled", false).toBool());
    rule->setSavePath(ruleHash.value("save_path").toString());
    rule->setCategory(ruleHash.value("category_assigned").toString());
    rule->setAddPaused(AddPausedState(ruleHash.value("add_paused").toUInt()));
    rule->setLastMatch(ruleHash.value("last_match").toDateTime());
    rule->setIgnoreDays(ruleHash.value("ignore_days").toInt());
    return rule;
}

QVariantHash DownloadRule::toVariantHash() const
{
    QVariantHash hash;
    hash["name"] = m_name;
    hash["must_contain"] = m_mustContain.join("|");
    hash["must_not_contain"] = m_mustNotContain.join("|");
    hash["save_path"] = m_savePath;
    hash["affected_feeds"] = m_rssFeeds;
    hash["enabled"] = m_enabled;
    hash["category_assigned"] = m_category;
    hash["use_regex"] = m_useRegex;
    hash["add_paused"] = m_apstate;
    hash["episode_filter"] = m_episodeFilter;
    hash["last_match"] = m_lastMatch;
    hash["ignore_days"] = m_ignoreDays;
    return hash;
}

bool DownloadRule::operator==(const DownloadRule &other) const
{
    return m_name == other.name();
}

void DownloadRule::setSavePath(const QString &savePath)
{
    m_savePath = Utils::Fs::fromNativePath(savePath);
}

DownloadRule::AddPausedState DownloadRule::addPaused() const
{
    return m_apstate;
}

void DownloadRule::setAddPaused(const DownloadRule::AddPausedState &aps)
{
    m_apstate = aps;
}

QString DownloadRule::category() const
{
    return m_category;
}

void DownloadRule::setCategory(const QString &category)
{
    m_category = category;
}

bool DownloadRule::isEnabled() const
{
    return m_enabled;
}

void DownloadRule::setEnabled(bool enable)
{
    m_enabled = enable;
}

void DownloadRule::setLastMatch(const QDateTime &d)
{
    m_lastMatch = d;
}

QDateTime DownloadRule::lastMatch() const
{
    return m_lastMatch;
}

void DownloadRule::setIgnoreDays(int d)
{
    m_ignoreDays = d;
}

int DownloadRule::ignoreDays() const
{
    return m_ignoreDays;
}

QString DownloadRule::mustContain() const
{
    return m_mustContain.join("|");
}

QString DownloadRule::mustNotContain() const
{
    return m_mustNotContain.join("|");
}

bool DownloadRule::useRegex() const
{
    return m_useRegex;
}

void DownloadRule::setUseRegex(bool enabled)
{
    m_useRegex = enabled;
    m_cachedRegexes->clear();
}

QString DownloadRule::episodeFilter() const
{
    return m_episodeFilter;
}

void DownloadRule::setEpisodeFilter(const QString &e)
{
    m_episodeFilter = e;
    m_cachedRegexes->clear();
}

QStringList DownloadRule::findMatchingArticles(const FeedPtr &feed) const
{
    QStringList ret;
    const ArticleHash &feedArticles = feed->articleHash();

    ArticleHash::ConstIterator artIt = feedArticles.begin();
    ArticleHash::ConstIterator artItend = feedArticles.end();
    for (; artIt != artItend; ++artIt) {
        const QString title = artIt.value()->title();
        qDebug() << "Matching article:" << title;
        if (matches(title))
            ret << title;
    }
    return ret;
}
