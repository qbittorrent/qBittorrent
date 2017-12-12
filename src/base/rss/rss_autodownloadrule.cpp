/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2017  Vladimir Golovnev <glassez@yandex.ru>
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

#include <QDebug>
#include <QDir>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QRegExp>
#include <QRegularExpression>
#include <QSharedData>
#include <QString>
#include <QStringList>

#include "../preferences.h"
#include "../utils/fs.h"
#include "../utils/string.h"
#include "rss_feed.h"
#include "rss_article.h"

namespace
{
    boost::optional<bool> jsonValueToOptional(const QJsonValue &jsonVal)
    {
        if (jsonVal.isBool())
            return jsonVal.toBool();

        if (!jsonVal.isNull())
            qDebug() << Q_FUNC_INFO << "Incorrect value" << jsonVal.toVariant();

        return boost::none;
    }

    QJsonValue optionalToJsonValue(const boost::optional<bool> &opt)
    {
        if (!opt)
            return QJsonValue();
        return *opt;
    }

    boost::optional<bool> addPausedLegacyToOptional(int val)
    {
        switch (val) {
        case 1:  return true; // always
        case 2:  return false; // never
        default: return boost::none; // default
        }
    }

    int optionalToAddPausedLegacy(const boost::optional<bool> &opt)
    {
        if (opt) {
            if (*opt)
                return 1; // always
            else
                return 2; // never
        }
        return 0; // use default
    }
}

const QString Str_Name(QStringLiteral("name"));
const QString Str_Enabled(QStringLiteral("enabled"));
const QString Str_UseRegex(QStringLiteral("useRegex"));
const QString Str_MustContain(QStringLiteral("mustContain"));
const QString Str_MustNotContain(QStringLiteral("mustNotContain"));
const QString Str_EpisodeFilter(QStringLiteral("episodeFilter"));
const QString Str_AffectedFeeds(QStringLiteral("affectedFeeds"));
const QString Str_SavePath(QStringLiteral("savePath"));
const QString Str_AssignedCategory(QStringLiteral("assignedCategory"));
const QString Str_LastMatch(QStringLiteral("lastMatch"));
const QString Str_IgnoreDays(QStringLiteral("ignoreDays"));
const QString Str_AddPaused(QStringLiteral("addPaused"));

namespace RSS
{
    struct AutoDownloadRuleData: public QSharedData
    {
        QString name;
        bool enabled = true;

        QStringList mustContain;
        QStringList mustNotContain;
        QString episodeFilter;
        QStringList feedURLs;
        bool useRegex = false;
        int ignoreDays = 0;
        QDateTime lastMatch;

        QString savePath;
        QString category;
        boost::optional<bool> addPaused;

        mutable QHash<QString, QRegularExpression> cachedRegexes;

        bool operator==(const AutoDownloadRuleData &other) const
        {
            return (name == other.name)
                    && (enabled == other.enabled)
                    && (mustContain == other.mustContain)
                    && (mustNotContain == other.mustNotContain)
                    && (episodeFilter == other.episodeFilter)
                    && (feedURLs == other.feedURLs)
                    && (useRegex == other.useRegex)
                    && (ignoreDays == other.ignoreDays)
                    && (lastMatch == other.lastMatch)
                    && (savePath == other.savePath)
                    && (category == other.category)
                    && (addPaused == other.addPaused);
        }
    };
}

using namespace RSS;

AutoDownloadRule::AutoDownloadRule(const QString &name)
    : m_dataPtr(new AutoDownloadRuleData)
{
    setName(name);
}

AutoDownloadRule::AutoDownloadRule(const AutoDownloadRule &other)
    : m_dataPtr(other.m_dataPtr)
{
}

AutoDownloadRule::~AutoDownloadRule() {}

QRegularExpression AutoDownloadRule::cachedRegex(const QString &expression, bool isRegex) const
{
    // Use a cache of regexes so we don't have to continually recompile - big performance increase.
    // The cache is cleared whenever the regex/wildcard, must or must not contain fields or
    // episode filter are modified.
    Q_ASSERT(!expression.isEmpty());
    QRegularExpression regex(m_dataPtr->cachedRegexes[expression]);

    if (!regex.pattern().isEmpty())
        return regex;

    return m_dataPtr->cachedRegexes[expression] = QRegularExpression(isRegex ? expression : Utils::String::wildcardToRegex(expression), QRegularExpression::CaseInsensitiveOption);
}

bool AutoDownloadRule::matches(const QString &articleTitle, const QString &expression) const
{
    static QRegularExpression whitespace("\\s+");

    if (expression.isEmpty()) {
        // A regex of the form "expr|" will always match, so do the same for wildcards
        return true;
    }
    else if (m_dataPtr->useRegex) {
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

bool AutoDownloadRule::matches(const QString &articleTitle) const
{
    if (!m_dataPtr->mustContain.empty()) {
        bool logged = false;
        bool foundMustContain = false;

        // Each expression is either a regex, or a set of wildcards separated by whitespace.
        // Accept if any complete expression matches.
        foreach (const QString &expression, m_dataPtr->mustContain) {
            if (!logged) {
//                qDebug() << "Checking matching" << (m_dataPtr->useRegex ? "regex:" : "wildcard expressions:") << m_dataPtr->mustContain.join("|");
                logged = true;
            }

            // A regex of the form "expr|" will always match, so do the same for wildcards
            foundMustContain = matches(articleTitle, expression);

            if (foundMustContain) {
//                qDebug() << "Found matching" << (m_dataPtr->useRegex ? "regex:" : "wildcard expression:") << expression;
                break;
            }
        }

        if (!foundMustContain)
            return false;
    }

    if (!m_dataPtr->mustNotContain.empty()) {
        bool logged = false;

        // Each expression is either a regex, or a set of wildcards separated by whitespace.
        // Reject if any complete expression matches.
        foreach (const QString &expression, m_dataPtr->mustNotContain) {
            if (!logged) {
//                qDebug() << "Checking not matching" << (m_dataPtr->useRegex ? "regex:" : "wildcard expressions:") << m_dataPtr->mustNotContain.join("|");
                logged = true;
            }

            // A regex of the form "expr|" will always match, so do the same for wildcards
            if (matches(articleTitle, expression)) {
//                qDebug() << "Found not matching" << (m_dataPtr->useRegex ? "regex:" : "wildcard expression:") << expression;
                return false;
            }
        }
    }

    if (!m_dataPtr->episodeFilter.isEmpty()) {
//        qDebug() << "Checking episode filter:" << m_dataPtr->episodeFilter;
        QRegularExpression f(cachedRegex("(^\\d{1,4})x(.*;$)"));
        QRegularExpressionMatch matcher = f.match(m_dataPtr->episodeFilter);
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
//                            qDebug() << "Matched episode:" << ep;
//                            qDebug() << "Matched article:" << articleTitle;
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
//                            qDebug() << "Matched episode:" << ep;
//                            qDebug() << "Matched article:" << articleTitle;
                            return true;
                        }
                    }
                }
            }
            else { // Single number
                QString expStr("\\b(?:s0?" + s + "[ -_\\.]?" + "e0?" + ep + "|" + s + "x" + "0?" + ep + ")(?:\\D|\\b)");
                QRegularExpression reg(cachedRegex(expStr));
                if (reg.match(articleTitle).hasMatch()) {
//                    qDebug() << "Matched episode:" << ep;
//                    qDebug() << "Matched article:" << articleTitle;
                    return true;
                }
            }
        }

        return false;
    }

//    qDebug() << "Matched article:" << articleTitle;
    return true;
}

AutoDownloadRule &AutoDownloadRule::operator=(const AutoDownloadRule &other)
{
    m_dataPtr = other.m_dataPtr;
    return *this;
}

bool AutoDownloadRule::operator==(const AutoDownloadRule &other) const
{
    return (m_dataPtr == other.m_dataPtr) // optimization
            || (*m_dataPtr == *other.m_dataPtr);
}

bool AutoDownloadRule::operator!=(const AutoDownloadRule &other) const
{
    return !operator==(other);
}

QJsonObject AutoDownloadRule::toJsonObject() const
{
    return {{Str_Enabled, isEnabled()}
        , {Str_UseRegex, useRegex()}
        , {Str_MustContain, mustContain()}
        , {Str_MustNotContain, mustNotContain()}
        , {Str_EpisodeFilter, episodeFilter()}
        , {Str_AffectedFeeds, QJsonArray::fromStringList(feedURLs())}
        , {Str_SavePath, savePath()}
        , {Str_AssignedCategory, assignedCategory()}
        , {Str_LastMatch, lastMatch().toString(Qt::RFC2822Date)}
        , {Str_IgnoreDays, ignoreDays()}
        , {Str_AddPaused, optionalToJsonValue(addPaused())}};
}

AutoDownloadRule AutoDownloadRule::fromJsonObject(const QJsonObject &jsonObj, const QString &name)
{
    AutoDownloadRule rule(name.isEmpty() ? jsonObj.value(Str_Name).toString() : name);

    rule.setUseRegex(jsonObj.value(Str_UseRegex).toBool(false));
    rule.setMustContain(jsonObj.value(Str_MustContain).toString());
    rule.setMustNotContain(jsonObj.value(Str_MustNotContain).toString());
    rule.setEpisodeFilter(jsonObj.value(Str_EpisodeFilter).toString());
    rule.setEnabled(jsonObj.value(Str_Enabled).toBool(true));
    rule.setSavePath(jsonObj.value(Str_SavePath).toString());
    rule.setCategory(jsonObj.value(Str_AssignedCategory).toString());
    rule.setAddPaused(jsonValueToOptional(jsonObj.value(Str_AddPaused)));
    rule.setLastMatch(QDateTime::fromString(jsonObj.value(Str_LastMatch).toString(), Qt::RFC2822Date));
    rule.setIgnoreDays(jsonObj.value(Str_IgnoreDays).toInt());

    const QJsonValue feedsVal = jsonObj.value(Str_AffectedFeeds);
    QStringList feedURLs;
    if (feedsVal.isString())
        feedURLs << feedsVal.toString();
    else foreach (const QJsonValue &urlVal, feedsVal.toArray())
        feedURLs << urlVal.toString();
    rule.setFeedURLs(feedURLs);

    return rule;
}

QVariantHash AutoDownloadRule::toLegacyDict() const
{
    return {{"name", name()},
        {"must_contain", mustContain()},
        {"must_not_contain", mustNotContain()},
        {"save_path", savePath()},
        {"affected_feeds", feedURLs()},
        {"enabled", isEnabled()},
        {"category_assigned", assignedCategory()},
        {"use_regex", useRegex()},
        {"add_paused", optionalToAddPausedLegacy(addPaused())},
        {"episode_filter", episodeFilter()},
        {"last_match", lastMatch()},
        {"ignore_days", ignoreDays()}};
}

AutoDownloadRule AutoDownloadRule::fromLegacyDict(const QVariantHash &dict)
{
    AutoDownloadRule rule(dict.value("name").toString());

    rule.setUseRegex(dict.value("use_regex", false).toBool());
    rule.setMustContain(dict.value("must_contain").toString());
    rule.setMustNotContain(dict.value("must_not_contain").toString());
    rule.setEpisodeFilter(dict.value("episode_filter").toString());
    rule.setFeedURLs(dict.value("affected_feeds").toStringList());
    rule.setEnabled(dict.value("enabled", false).toBool());
    rule.setSavePath(dict.value("save_path").toString());
    rule.setCategory(dict.value("category_assigned").toString());
    rule.setAddPaused(addPausedLegacyToOptional(dict.value("add_paused").toInt()));
    rule.setLastMatch(dict.value("last_match").toDateTime());
    rule.setIgnoreDays(dict.value("ignore_days").toInt());

    return rule;
}

void AutoDownloadRule::setMustContain(const QString &tokens)
{
    m_dataPtr->cachedRegexes.clear();

    if (m_dataPtr->useRegex)
        m_dataPtr->mustContain = QStringList() << tokens;
    else
        m_dataPtr->mustContain = tokens.split("|");

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
        m_dataPtr->mustNotContain = tokens.split("|");

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

QString AutoDownloadRule::savePath() const
{
    return m_dataPtr->savePath;
}

void AutoDownloadRule::setSavePath(const QString &savePath)
{
    m_dataPtr->savePath = Utils::Fs::fromNativePath(savePath);
}

boost::optional<bool> AutoDownloadRule::addPaused() const
{
    return m_dataPtr->addPaused;
}

void AutoDownloadRule::setAddPaused(const boost::optional<bool> &addPaused)
{
    m_dataPtr->addPaused = addPaused;
}

QString AutoDownloadRule::assignedCategory() const
{
    return m_dataPtr->category;
}

void AutoDownloadRule::setCategory(const QString &category)
{
    m_dataPtr->category = category;
}

bool AutoDownloadRule::isEnabled() const
{
    return m_dataPtr->enabled;
}

void AutoDownloadRule::setEnabled(bool enable)
{
    m_dataPtr->enabled = enable;
}

QDateTime AutoDownloadRule::lastMatch() const
{
    return m_dataPtr->lastMatch;
}

void AutoDownloadRule::setLastMatch(const QDateTime &lastMatch)
{
    m_dataPtr->lastMatch = lastMatch;
}

void AutoDownloadRule::setIgnoreDays(int d)
{
    m_dataPtr->ignoreDays = d;
}

int AutoDownloadRule::ignoreDays() const
{
    return m_dataPtr->ignoreDays;
}

QString AutoDownloadRule::mustContain() const
{
    return m_dataPtr->mustContain.join("|");
}

QString AutoDownloadRule::mustNotContain() const
{
    return m_dataPtr->mustNotContain.join("|");
}

bool AutoDownloadRule::useRegex() const
{
    return m_dataPtr->useRegex;
}

void AutoDownloadRule::setUseRegex(bool enabled)
{
    m_dataPtr->useRegex = enabled;
    m_dataPtr->cachedRegexes.clear();
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
