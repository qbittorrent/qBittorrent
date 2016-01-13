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

#include <QRegExp>
#include <QDebug>
#include <QDir>

#include "base/preferences.h"
#include "base/utils/fs.h"
#include "rssfeed.h"
#include "rssarticle.h"
#include "rssdownloadrule.h"

using namespace Rss;

DownloadRule::DownloadRule()
    : m_enabled(false)
    , m_useRegex(false)
    , m_apstate(USE_GLOBAL)
    , m_ignoreDays(0)
{
}

bool DownloadRule::matches(const QString &articleTitle) const
{
    foreach (const QString &token, m_mustContain) {
        if (!token.isEmpty()) {
            QRegExp reg(token, Qt::CaseInsensitive, m_useRegex ? QRegExp::RegExp : QRegExp::Wildcard);
            if (reg.indexIn(articleTitle) < 0)
                return false;
        }
    }
    qDebug("Checking not matching tokens");
    // Checking not matching
    foreach (const QString &token, m_mustNotContain) {
        if (!token.isEmpty()) {
            QRegExp reg(token, Qt::CaseInsensitive, m_useRegex ? QRegExp::RegExp : QRegExp::Wildcard);
            if (reg.indexIn(articleTitle) > -1)
                return false;
        }
    }
    if (!m_episodeFilter.isEmpty()) {
        qDebug("Checking episode filter");
        QRegExp f("(^\\d{1,4})x(.*;$)");
        int pos = f.indexIn(m_episodeFilter);
        if (pos < 0)
            return false;

        QString s = f.cap(1);
        QStringList eps = f.cap(2).split(";");
        QString expStr;
        expStr += "s0?" + s + "[ -_\\.]?" + "e0?";

        foreach (const QString &ep, eps) {
            if (ep.isEmpty())
                continue;

            if (ep.indexOf('-') != -1) { // Range detected
                QString partialPattern = "s0?" + s + "[ -_\\.]?" + "e(0?\\d{1,4})";
                QRegExp reg(partialPattern, Qt::CaseInsensitive);

                if (ep.endsWith('-')) { // Infinite range
                    int epOurs = ep.left(ep.size() - 1).toInt();

                    // Extract partial match from article and compare as digits
                    pos = reg.indexIn(articleTitle);
                    if (pos != -1) {
                        int epTheirs = reg.cap(1).toInt();
                        if (epTheirs >= epOurs)
                            return true;
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
                    pos = reg.indexIn(articleTitle);
                    if (pos != -1) {
                        int epTheirs = reg.cap(1).toInt();
                        if (epOursFirst <= epTheirs && epOursLast >= epTheirs)
                            return true;
                    }
                }
            }
            else { // Single number
                QRegExp reg(expStr + ep + "\\D", Qt::CaseInsensitive);
                if (reg.indexIn(articleTitle) != -1)
                    return true;
            }
        }
        return false;
    }
    return true;
}

void DownloadRule::setMustContain(const QString &tokens)
{
    if (m_useRegex)
        m_mustContain = QStringList() << tokens;
    else
        m_mustContain = tokens.split(" ");
}

void DownloadRule::setMustNotContain(const QString &tokens)
{
    if (m_useRegex)
        m_mustNotContain = QStringList() << tokens;
    else
        m_mustNotContain = tokens.split("|");
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
    rule->setLabel(ruleHash.value("label_assigned").toString());
    rule->setAddPaused(AddPausedState(ruleHash.value("add_paused").toUInt()));
    rule->setLastMatch(ruleHash.value("last_match").toDateTime());
    rule->setIgnoreDays(ruleHash.value("ignore_days").toInt());
    return rule;
}

QVariantHash DownloadRule::toVariantHash() const
{
    QVariantHash hash;
    hash["name"] = m_name;
    hash["must_contain"] = m_mustContain.join(" ");
    hash["must_not_contain"] = m_mustNotContain.join("|");
    hash["save_path"] = m_savePath;
    hash["affected_feeds"] = m_rssFeeds;
    hash["enabled"] = m_enabled;
    hash["label_assigned"] = m_label;
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
    if (!savePath.isEmpty() && (QDir(savePath) != QDir(Preferences::instance()->getSavePath())))
        m_savePath = Utils::Fs::fromNativePath(savePath);
    else
        m_savePath = QString();
}

DownloadRule::AddPausedState DownloadRule::addPaused() const
{
    return m_apstate;
}

void DownloadRule::setAddPaused(const DownloadRule::AddPausedState &aps)
{
    m_apstate = aps;
}

QString DownloadRule::label() const
{
    return m_label;
}

void DownloadRule::setLabel(const QString &label)
{
    m_label = label;
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
    return m_mustContain.join(" ");
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
}

QString DownloadRule::episodeFilter() const
{
    return m_episodeFilter;
}

void DownloadRule::setEpisodeFilter(const QString &e)
{
    m_episodeFilter = e;
}

QStringList DownloadRule::findMatchingArticles(const FeedPtr &feed) const
{
    QStringList ret;
    const ArticleHash &feedArticles = feed->articleHash();

    ArticleHash::ConstIterator artIt = feedArticles.begin();
    ArticleHash::ConstIterator artItend = feedArticles.end();
    for ( ; artIt != artItend ; ++artIt) {
        const QString title = artIt.value()->title();
        if (matches(title))
            ret << title;
    }
    return ret;
}
