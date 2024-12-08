/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2017-2023  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2010  Christophe Dumez <chris@qbittorrent.org>
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
 */

#include "rss_autodownloadrule.h"

#include <algorithm>

#include <QDebug>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSharedData>
#include <QString>
#include <QStringList>

#include "base/global.h"
#include "base/path.h"
#include "base/preferences.h"
#include "base/utils/fs.h"
#include "base/utils/string.h"
#include "rss_article.h"
#include "rss_autodownloader.h"
#include "rss_feed.h"

namespace
{
    std::optional<bool> toOptionalBool(const QJsonValue &jsonVal)
    {
        if (jsonVal.isBool())
            return jsonVal.toBool();

        return std::nullopt;
    }

    QJsonValue toJsonValue(const std::optional<bool> boolValue)
    {
        return boolValue.has_value() ? *boolValue : QJsonValue {};
    }

    std::optional<bool> addPausedLegacyToOptionalBool(const int val)
    {
        switch (val)
        {
        case 1:
            return true; // always
        case 2:
            return false; // never
        default:
            return std::nullopt; // default
        }
    }

    int toAddPausedLegacy(const std::optional<bool> boolValue)
    {
        if (!boolValue.has_value())
            return 0; // default

        return (*boolValue ? 1 /* always */ : 2 /* never */);
    }

    std::optional<BitTorrent::TorrentContentLayout> jsonValueToContentLayout(const QJsonValue &jsonVal)
    {
        const QString str = jsonVal.toString();
        if (str.isEmpty())
            return std::nullopt;
        return Utils::String::toEnum(str, BitTorrent::TorrentContentLayout::Original);
    }

    QJsonValue contentLayoutToJsonValue(const std::optional<BitTorrent::TorrentContentLayout> contentLayout)
    {
        if (!contentLayout)
            return {};
        return Utils::String::fromEnum(*contentLayout);
    }
}

const QString S_NAME = u"name"_s;
const QString S_ENABLED = u"enabled"_s;
const QString S_PRIORITY = u"priority"_s;
const QString S_USE_REGEX = u"useRegex"_s;
const QString S_MUST_CONTAIN = u"mustContain"_s;
const QString S_MUST_NOT_CONTAIN = u"mustNotContain"_s;
const QString S_EPISODE_FILTER = u"episodeFilter"_s;
const QString S_AFFECTED_FEEDS = u"affectedFeeds"_s;
const QString S_LAST_MATCH = u"lastMatch"_s;
const QString S_IGNORE_DAYS = u"ignoreDays"_s;
const QString S_SMART_FILTER = u"smartFilter"_s;
const QString S_PREVIOUSLY_MATCHED = u"previouslyMatchedEpisodes"_s;

const QString S_SAVE_PATH = u"savePath"_s;
const QString S_ASSIGNED_CATEGORY = u"assignedCategory"_s;
const QString S_ADD_PAUSED = u"addPaused"_s;
const QString S_CONTENT_LAYOUT = u"torrentContentLayout"_s;

const QString S_TORRENT_PARAMS = u"torrentParams"_s;

namespace RSS
{
    struct AutoDownloadRuleData : public QSharedData
    {
        QString name;
        bool enabled = true;
        int priority = 0;

        QStringList mustContain;
        QStringList mustNotContain;
        QString episodeFilter;
        QStringList feedURLs;
        bool useRegex = false;
        int ignoreDays = 0;
        QDateTime lastMatch;

        BitTorrent::AddTorrentParams addTorrentParams;

        bool smartFilter = false;
        QStringList previouslyMatchedEpisodes;

        mutable QStringList lastComputedEpisodes;
        mutable QHash<QString, QRegularExpression> cachedRegexes;

        friend bool operator==(const AutoDownloadRuleData &left, const AutoDownloadRuleData &right)
        {
            return (left.name == right.name)
                    && (left.enabled == right.enabled)
                    && (left.priority == right.priority)
                    && (left.mustContain == right.mustContain)
                    && (left.mustNotContain == right.mustNotContain)
                    && (left.episodeFilter == right.episodeFilter)
                    && (left.feedURLs == right.feedURLs)
                    && (left.useRegex == right.useRegex)
                    && (left.ignoreDays == right.ignoreDays)
                    && (left.lastMatch == right.lastMatch)
                    && (left.smartFilter == right.smartFilter)
                    && (left.addTorrentParams == right.addTorrentParams);
        }
    };

    bool operator==(const AutoDownloadRule &left, const AutoDownloadRule &right)
    {
        return (left.m_dataPtr == right.m_dataPtr) // optimization
                || (*(left.m_dataPtr) == *(right.m_dataPtr));
    }
}

using namespace RSS;

QString computeEpisodeName(const QString &article)
{
    const QRegularExpression episodeRegex = AutoDownloader::instance()->smartEpisodeRegex();
    const QRegularExpressionMatch match = episodeRegex.match(article);

    // See if we can extract an season/episode number or date from the title
    if (!match.hasMatch())
        return {};

    QStringList ret;
    for (int i = 1; i <= match.lastCapturedIndex(); ++i)
    {
        const QString cap = match.captured(i);

        if (cap.isEmpty())
            continue;

        bool isInt = false;
        const int x = cap.toInt(&isInt);

        ret.append(isInt ? QString::number(x) : cap);
    }
    return ret.join(u'x');
}

AutoDownloadRule::AutoDownloadRule(const QString &name)
    : m_dataPtr(new AutoDownloadRuleData)
{
    setName(name);
}

AutoDownloadRule::AutoDownloadRule(const AutoDownloadRule &other) = default;

AutoDownloadRule::~AutoDownloadRule() = default;

QRegularExpression AutoDownloadRule::cachedRegex(const QString &expression, const bool isRegex) const
{
    // Use a cache of regexes so we don't have to continually recompile - big performance increase.
    // The cache is cleared whenever the regex/wildcard, must or must not contain fields or
    // episode filter are modified.
    Q_ASSERT(!expression.isEmpty());

    QRegularExpression &regex = m_dataPtr->cachedRegexes[expression];
    if (regex.pattern().isEmpty())
    {
        const QString pattern = (isRegex ? expression : Utils::String::wildcardToRegexPattern(expression));
        regex = QRegularExpression {pattern, QRegularExpression::CaseInsensitiveOption};
    }

    return regex;
}

bool AutoDownloadRule::matchesExpression(const QString &articleTitle, const QString &expression) const
{
    const QRegularExpression whitespace {u"\\s+"_s};

    if (expression.isEmpty())
    {
        // A regex of the form "expr|" will always match, so do the same for wildcards
        return true;
    }

    if (m_dataPtr->useRegex)
    {
        const QRegularExpression reg(cachedRegex(expression));
        return reg.match(articleTitle).hasMatch();
    }

    // Only match if every wildcard token (separated by spaces) is present in the article name.
    // Order of wildcard tokens is unimportant (if order is important, they should have used *).
    const QStringList wildcards {expression.split(whitespace, Qt::SkipEmptyParts)};
    for (const QString &wildcard : wildcards)
    {
        const QRegularExpression reg {cachedRegex(wildcard, false)};
        if (!reg.match(articleTitle).hasMatch())
            return false;
    }

    return true;
}

bool AutoDownloadRule::matchesMustContainExpression(const QString &articleTitle) const
{
    if (m_dataPtr->mustContain.empty())
        return true;

    // Each expression is either a regex, or a set of wildcards separated by whitespace.
    // Accept if any complete expression matches.
    return std::any_of(m_dataPtr->mustContain.cbegin(), m_dataPtr->mustContain.cend(), [this, &articleTitle](const QString &expression)
    {
        // A regex of the form "expr|" will always match, so do the same for wildcards
        return matchesExpression(articleTitle, expression);
    });
}

bool AutoDownloadRule::matchesMustNotContainExpression(const QString &articleTitle) const
{
    if (m_dataPtr->mustNotContain.empty())
        return true;

    // Each expression is either a regex, or a set of wildcards separated by whitespace.
    // Reject if any complete expression matches.
    return std::none_of(m_dataPtr->mustNotContain.cbegin(), m_dataPtr->mustNotContain.cend(), [this, &articleTitle](const QString &expression)
    {
        // A regex of the form "expr|" will always match, so do the same for wildcards
        return matchesExpression(articleTitle, expression);
    });
}

bool AutoDownloadRule::matchesEpisodeFilterExpression(const QString &articleTitle) const
{
    // Reset the lastComputedEpisode, we don't want to leak it between matches
    m_dataPtr->lastComputedEpisodes.clear();

    if (m_dataPtr->episodeFilter.isEmpty())
        return true;

    const QRegularExpression filterRegex {cachedRegex(u"(^\\d{1,4})x(.*;$)"_s)};
    const QRegularExpressionMatch matcher {filterRegex.match(m_dataPtr->episodeFilter)};
    if (!matcher.hasMatch())
        return false;

    const QString season {matcher.captured(1)};
    const QStringList episodes {matcher.captured(2).split(u';')};
    const int seasonOurs {season.toInt()};

    for (QString episode : episodes)
    {
        if (episode.isEmpty())
            continue;

        // We need to trim leading zeroes, but if it's all zeros then we want episode zero.
        while ((episode.size() > 1) && episode.startsWith(u'0'))
            episode = episode.right(episode.size() - 1);

        if (episode.indexOf(u'-') != -1)
        { // Range detected
            const QString partialPattern1 {u"\\bs0?(\\d{1,4})[ -_\\.]?e(0?\\d{1,4})(?:\\D|\\b)"_s};
            const QString partialPattern2 {u"\\b(\\d{1,4})x(0?\\d{1,4})(?:\\D|\\b)"_s};

            // Extract partial match from article and compare as digits
            QRegularExpressionMatch matcher = cachedRegex(partialPattern1).match(articleTitle);
            bool matched = matcher.hasMatch();

            if (!matched)
            {
                matcher = cachedRegex(partialPattern2).match(articleTitle);
                matched = matcher.hasMatch();
            }

            if (matched)
            {
                const int seasonTheirs {matcher.captured(1).toInt()};
                const int episodeTheirs {matcher.captured(2).toInt()};

                if (episode.endsWith(u'-'))
                { // Infinite range
                    const int episodeOurs {QStringView(episode).left(episode.size() - 1).toInt()};
                    if (((seasonTheirs == seasonOurs) && (episodeTheirs >= episodeOurs)) || (seasonTheirs > seasonOurs))
                        return true;
                }
                else
                { // Normal range
                    const QStringList range {episode.split(u'-')};
                    Q_ASSERT(range.size() == 2);
                    if (range.first().toInt() > range.last().toInt())
                        continue; // Ignore this subrule completely

                    const int episodeOursFirst {range.first().toInt()};
                    const int episodeOursLast {range.last().toInt()};
                    if ((seasonTheirs == seasonOurs) && ((episodeOursFirst <= episodeTheirs) && (episodeOursLast >= episodeTheirs)))
                        return true;
                }
            }
        }
        else
        { // Single number
            const QString expStr {u"\\b(?:s0?%1[ -_\\.]?e0?%2|%1x0?%2)(?:\\D|\\b)"_s.arg(season, episode)};
            if (cachedRegex(expStr).match(articleTitle).hasMatch())
                return true;
        }
    }

    return false;
}

bool AutoDownloadRule::matchesSmartEpisodeFilter(const QString &articleTitle) const
{
    if (!useSmartFilter())
        return true;

    const QString episodeStr = computeEpisodeName(articleTitle);
    if (episodeStr.isEmpty())
        return true;

    // See if this episode has been downloaded before
    const bool previouslyMatched = m_dataPtr->previouslyMatchedEpisodes.contains(episodeStr);
    if (previouslyMatched)
    {
        if (!AutoDownloader::instance()->downloadRepacks())
            return false;

        // Now see if we've downloaded this particular repack/proper combination
        const bool isRepack = articleTitle.contains(u"REPACK", Qt::CaseInsensitive);
        const bool isProper = articleTitle.contains(u"PROPER", Qt::CaseInsensitive);

        if (!isRepack && !isProper)
            return false;

        const QString fullEpisodeStr = u"%1%2%3"_s.arg(episodeStr,
                                                        isRepack ? u"-REPACK" : u"",
                                                        isProper ? u"-PROPER" : u"");
        const bool previouslyMatchedFull = m_dataPtr->previouslyMatchedEpisodes.contains(fullEpisodeStr);
        if (previouslyMatchedFull)
            return false;

        m_dataPtr->lastComputedEpisodes.append(fullEpisodeStr);

        // If this is a REPACK and PROPER download, add the individual entries to the list
        // so we don't download those
        if (isRepack && isProper)
        {
            m_dataPtr->lastComputedEpisodes.append(episodeStr + u"-REPACK");
            m_dataPtr->lastComputedEpisodes.append(episodeStr + u"-PROPER");
        }

        return true;
    }

    m_dataPtr->lastComputedEpisodes.append(episodeStr);
    return true;
}

bool AutoDownloadRule::matches(const QVariantHash &articleData) const
{
    const QDateTime articleDate {articleData[Article::KeyDate].toDateTime()};
    if (ignoreDays() > 0)
    {
        if (lastMatch().isValid() && (articleDate < lastMatch().addDays(ignoreDays())))
            return false;
    }

    const QString articleTitle {articleData[Article::KeyTitle].toString()};
    if (!matchesMustContainExpression(articleTitle))
        return false;
    if (!matchesMustNotContainExpression(articleTitle))
        return false;
    if (!matchesEpisodeFilterExpression(articleTitle))
        return false;
    if (!matchesSmartEpisodeFilter(articleTitle))
        return false;

    return true;
}

bool AutoDownloadRule::accepts(const QVariantHash &articleData)
{
    if (!matches(articleData))
        return false;

    setLastMatch(articleData[Article::KeyDate].toDateTime());

    // If there's a matched episode string, add that to the previously matched list
    if (!m_dataPtr->lastComputedEpisodes.isEmpty())
    {
        m_dataPtr->previouslyMatchedEpisodes.append(m_dataPtr->lastComputedEpisodes);
        m_dataPtr->lastComputedEpisodes.clear();
    }

    return true;
}

AutoDownloadRule &AutoDownloadRule::operator=(const AutoDownloadRule &other)
{
    if (this != &other)
    {
        m_dataPtr = other.m_dataPtr;
    }
    return *this;
}

QJsonObject AutoDownloadRule::toJsonObject() const
{
    const BitTorrent::AddTorrentParams &addTorrentParams = m_dataPtr->addTorrentParams;

    return {{S_ENABLED, isEnabled()}
        , {S_PRIORITY, priority()}
        , {S_USE_REGEX, useRegex()}
        , {S_MUST_CONTAIN, mustContain()}
        , {S_MUST_NOT_CONTAIN, mustNotContain()}
        , {S_EPISODE_FILTER, episodeFilter()}
        , {S_AFFECTED_FEEDS, QJsonArray::fromStringList(feedURLs())}
        , {S_LAST_MATCH, lastMatch().toString(Qt::RFC2822Date)}
        , {S_IGNORE_DAYS, ignoreDays()}
        , {S_SMART_FILTER, useSmartFilter()}
        , {S_PREVIOUSLY_MATCHED, QJsonArray::fromStringList(previouslyMatchedEpisodes())}

        // TODO: The following code is deprecated. Replace with the commented one after several releases in 4.6.x.
        // === BEGIN DEPRECATED CODE === //
        , {S_ADD_PAUSED, toJsonValue(addTorrentParams.addStopped)}
        , {S_CONTENT_LAYOUT, contentLayoutToJsonValue(addTorrentParams.contentLayout)}
        , {S_SAVE_PATH, addTorrentParams.savePath.toString()}
        , {S_ASSIGNED_CATEGORY, addTorrentParams.category}
        // === END DEPRECATED CODE === //

        , {S_TORRENT_PARAMS, BitTorrent::serializeAddTorrentParams(addTorrentParams)}
    };
}

AutoDownloadRule AutoDownloadRule::fromJsonObject(const QJsonObject &jsonObj, const QString &name)
{
    AutoDownloadRule rule {(name.isEmpty() ? jsonObj.value(S_NAME).toString() : name)};

    rule.setEnabled(jsonObj.value(S_ENABLED).toBool(true));
    rule.setPriority(jsonObj.value(S_PRIORITY).toInt(0));

    rule.setUseRegex(jsonObj.value(S_USE_REGEX).toBool(false));
    rule.setMustContain(jsonObj.value(S_MUST_CONTAIN).toString());
    rule.setMustNotContain(jsonObj.value(S_MUST_NOT_CONTAIN).toString());
    rule.setEpisodeFilter(jsonObj.value(S_EPISODE_FILTER).toString());
    rule.setLastMatch(QDateTime::fromString(jsonObj.value(S_LAST_MATCH).toString(), Qt::RFC2822Date));
    rule.setIgnoreDays(jsonObj.value(S_IGNORE_DAYS).toInt());
    rule.setUseSmartFilter(jsonObj.value(S_SMART_FILTER).toBool(false));

    const QJsonValue feedsVal = jsonObj.value(S_AFFECTED_FEEDS);
    QStringList feedURLs;
    if (feedsVal.isString())
        feedURLs << feedsVal.toString();
    else for (const QJsonValue &urlVal : asConst(feedsVal.toArray()))
        feedURLs << urlVal.toString();
    rule.setFeedURLs(feedURLs);

    const QJsonValue previouslyMatchedVal = jsonObj.value(S_PREVIOUSLY_MATCHED);
    QStringList previouslyMatched;
    if (previouslyMatchedVal.isString())
    {
        previouslyMatched << previouslyMatchedVal.toString();
    }
    else
    {
        for (const QJsonValue &val : asConst(previouslyMatchedVal.toArray()))
            previouslyMatched << val.toString();
    }
    rule.setPreviouslyMatchedEpisodes(previouslyMatched);

    // TODO: The following code is deprecated. Replace with the commented one after several releases in 4.6.x.
    // === BEGIN DEPRECATED CODE === //
    BitTorrent::AddTorrentParams addTorrentParams;
    if (auto it = jsonObj.find(S_TORRENT_PARAMS); it != jsonObj.end())
    {
        addTorrentParams = BitTorrent::parseAddTorrentParams(it->toObject());
    }
    else
    {
        addTorrentParams.savePath = Path(jsonObj.value(S_SAVE_PATH).toString());
        addTorrentParams.category = jsonObj.value(S_ASSIGNED_CATEGORY).toString();
        addTorrentParams.addStopped = toOptionalBool(jsonObj.value(S_ADD_PAUSED));
        if (!addTorrentParams.savePath.isEmpty())
            addTorrentParams.useAutoTMM = false;

        if (jsonObj.contains(S_CONTENT_LAYOUT))
        {
            addTorrentParams.contentLayout = jsonValueToContentLayout(jsonObj.value(S_CONTENT_LAYOUT));
        }
        else
        {
            const std::optional<bool> createSubfolder = toOptionalBool(jsonObj.value(u"createSubfolder"));
            std::optional<BitTorrent::TorrentContentLayout> contentLayout;
            if (createSubfolder.has_value())
            {
                contentLayout = (*createSubfolder
                        ? BitTorrent::TorrentContentLayout::Original
                        : BitTorrent::TorrentContentLayout::NoSubfolder);
            }

            addTorrentParams.contentLayout = contentLayout;
        }
    }
    rule.setAddTorrentParams(addTorrentParams);
    // === END DEPRECATED CODE === //
    // === BEGIN REPLACEMENT CODE === //
    //    rule.setAddTorrentParams(BitTorrent::parseAddTorrentParams(jsonObj.value(S_TORRENT_PARAMS).object()));
    // === END REPLACEMENT CODE === //

    return rule;
}

QVariantHash AutoDownloadRule::toLegacyDict() const
{
    const BitTorrent::AddTorrentParams &addTorrentParams = m_dataPtr->addTorrentParams;

    return {{u"name"_s, name()},
        {u"must_contain"_s, mustContain()},
        {u"must_not_contain"_s, mustNotContain()},
        {u"save_path"_s, addTorrentParams.savePath.toString()},
        {u"affected_feeds"_s, feedURLs()},
        {u"enabled"_s, isEnabled()},
        {u"category_assigned"_s, addTorrentParams.category},
        {u"use_regex"_s, useRegex()},
        {u"add_paused"_s, toAddPausedLegacy(addTorrentParams.addStopped)},
        {u"episode_filter"_s, episodeFilter()},
        {u"last_match"_s, lastMatch()},
        {u"ignore_days"_s, ignoreDays()}};
}

AutoDownloadRule AutoDownloadRule::fromLegacyDict(const QVariantHash &dict)
{
    BitTorrent::AddTorrentParams addTorrentParams;
    addTorrentParams.savePath = Path(dict.value(u"save_path"_s).toString());
    addTorrentParams.category = dict.value(u"category_assigned"_s).toString();
    addTorrentParams.addStopped = addPausedLegacyToOptionalBool(dict.value(u"add_paused"_s).toInt());
    if (!addTorrentParams.savePath.isEmpty())
        addTorrentParams.useAutoTMM = false;

    AutoDownloadRule rule {dict.value(u"name"_s).toString()};

    rule.setUseRegex(dict.value(u"use_regex"_s, false).toBool());
    rule.setMustContain(dict.value(u"must_contain"_s).toString());
    rule.setMustNotContain(dict.value(u"must_not_contain"_s).toString());
    rule.setEpisodeFilter(dict.value(u"episode_filter"_s).toString());
    rule.setFeedURLs(dict.value(u"affected_feeds"_s).toStringList());
    rule.setEnabled(dict.value(u"enabled"_s, false).toBool());
    rule.setLastMatch(dict.value(u"last_match"_s).toDateTime());
    rule.setIgnoreDays(dict.value(u"ignore_days"_s).toInt());
    rule.setAddTorrentParams(addTorrentParams);

    return rule;
}

void AutoDownloadRule::setMustContain(const QString &tokens)
{
    m_dataPtr->cachedRegexes.clear();

    if (m_dataPtr->useRegex)
        m_dataPtr->mustContain = QStringList() << tokens;
    else
        m_dataPtr->mustContain = tokens.split(u'|');

    // Check for single empty string - if so, no condition
    if ((m_dataPtr->mustContain.size() == 1) && m_dataPtr->mustContain[0].isEmpty())
        m_dataPtr->mustContain.clear();
}

void AutoDownloadRule::setMustNotContain(const QString &tokens)
{
    m_dataPtr->cachedRegexes.clear();

    if (m_dataPtr->useRegex)
        m_dataPtr->mustNotContain = QStringList() << tokens;
    else
        m_dataPtr->mustNotContain = tokens.split(u'|');

    // Check for single empty string - if so, no condition
    if ((m_dataPtr->mustNotContain.size() == 1) && m_dataPtr->mustNotContain[0].isEmpty())
        m_dataPtr->mustNotContain.clear();
}

QStringList AutoDownloadRule::feedURLs() const
{
    return m_dataPtr->feedURLs;
}

void AutoDownloadRule::setFeedURLs(const QStringList &urls)
{
    m_dataPtr->feedURLs = urls;
}

QString AutoDownloadRule::name() const
{
    return m_dataPtr->name;
}

void AutoDownloadRule::setName(const QString &name)
{
    m_dataPtr->name = name;
}

BitTorrent::AddTorrentParams AutoDownloadRule::addTorrentParams() const
{
    return m_dataPtr->addTorrentParams;
}

void AutoDownloadRule::setAddTorrentParams(BitTorrent::AddTorrentParams addTorrentParams)
{
    m_dataPtr->addTorrentParams = std::move(addTorrentParams);
}

bool AutoDownloadRule::isEnabled() const
{
    return m_dataPtr->enabled;
}

void AutoDownloadRule::setEnabled(const bool enable)
{
    m_dataPtr->enabled = enable;
}

int AutoDownloadRule::priority() const
{
    return m_dataPtr->priority;
}

void AutoDownloadRule::setPriority(const int value)
{
    m_dataPtr->priority = value;
}

QDateTime AutoDownloadRule::lastMatch() const
{
    return m_dataPtr->lastMatch;
}

void AutoDownloadRule::setLastMatch(const QDateTime &lastMatch)
{
    m_dataPtr->lastMatch = lastMatch;
}

void AutoDownloadRule::setIgnoreDays(const int d)
{
    m_dataPtr->ignoreDays = d;
}

int AutoDownloadRule::ignoreDays() const
{
    return m_dataPtr->ignoreDays;
}

QString AutoDownloadRule::mustContain() const
{
    return m_dataPtr->mustContain.join(u'|');
}

QString AutoDownloadRule::mustNotContain() const
{
    return m_dataPtr->mustNotContain.join(u'|');
}

bool AutoDownloadRule::useSmartFilter() const
{
    return m_dataPtr->smartFilter;
}

void AutoDownloadRule::setUseSmartFilter(const bool enabled)
{
    m_dataPtr->smartFilter = enabled;
}

bool AutoDownloadRule::useRegex() const
{
    return m_dataPtr->useRegex;
}

void AutoDownloadRule::setUseRegex(const bool enabled)
{
    m_dataPtr->useRegex = enabled;
    m_dataPtr->cachedRegexes.clear();
}

QStringList AutoDownloadRule::previouslyMatchedEpisodes() const
{
    return m_dataPtr->previouslyMatchedEpisodes;
}

void AutoDownloadRule::setPreviouslyMatchedEpisodes(const QStringList &previouslyMatchedEpisodes)
{
    m_dataPtr->previouslyMatchedEpisodes = previouslyMatchedEpisodes;
}

QString AutoDownloadRule::episodeFilter() const
{
    return m_dataPtr->episodeFilter;
}

void AutoDownloadRule::setEpisodeFilter(const QString &e)
{
    m_dataPtr->episodeFilter = e;
    m_dataPtr->cachedRegexes.clear();
}
