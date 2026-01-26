/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  Thomas Piccirello <thomas@piccirello.com>
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

#include "searchjobmanager.h"

#include <limits>

#include <QDateTime>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QTimer>

#include "base/global.h"
#include "base/logger.h"
#include "base/preferences.h"
#include "base/profile.h"
#include "base/search/searchhandler.h"
#include "base/search/searchpluginmanager.h"
#include "base/utils/fs.h"
#include "base/utils/io.h"
#include "base/utils/random.h"

namespace
{
    const Path DATA_FOLDER {u"WebUI/SearchUI"_s};
    const QString SESSION_FILE_NAME {u"Session.json"_s};
    const int SESSION_FILE_MAX_SIZE = 1024 * 1024; // 1 MiB
    const int RESULTS_FILE_MAX_SIZE = 10 * 1024 * 1024; // 10 MiB
}

SearchJobManager::SearchJobManager(QObject *parent)
    : QObject(parent)
    , m_storeOpenedTabs(Preferences::instance()->storeOpenedSearchTabs())
    , m_storeOpenedTabsResults(Preferences::instance()->storeOpenedSearchTabResults())
{
    connect(Preferences::instance(), &Preferences::changed, this, &SearchJobManager::onPreferencesChanged);
    loadSession();
}

int SearchJobManager::startSearch(const QString &pattern, const QString &category, const QStringList &plugins)
{
    if (m_activeSearches.size() >= MAX_CONCURRENT_SEARCHES)
        return -1;

    const int id = generateSearchId();
    const std::shared_ptr<SearchHandler> searchHandler {SearchPluginManager::instance()->startSearch(pattern, category, plugins)};

    connect(searchHandler.get(), &SearchHandler::searchFinished, this, [this, id]
    {
        if (QTimer *timer = m_resultSaveTimers.take(id))
        {
            timer->stop();
            timer->deleteLater();
        }
        m_activeSearches.remove(id);
        saveSession();
        saveSearchResults(id);
    });
    connect(searchHandler.get(), &SearchHandler::searchFailed, this, [this, id]([[maybe_unused]] const QString &errorMessage)
    {
        if (QTimer *timer = m_resultSaveTimers.take(id))
        {
            timer->stop();
            timer->deleteLater();
        }
        m_activeSearches.remove(id);
        saveSession();
    });
    connect(searchHandler.get(), &SearchHandler::newSearchResults, this, [this, id]
    {
        scheduleSaveResults(id);
    });

    m_searchHandlers.insert(id, searchHandler);
    m_activeSearches.insert(id);
    m_searchOrder.append(id);

    saveSession();

    return id;
}

bool SearchJobManager::stopSearch(const int id)
{
    const auto iter = m_searchHandlers.constFind(id);
    if (iter == m_searchHandlers.cend())
        return false;

    const std::shared_ptr<SearchHandler> &searchHandler = iter.value();
    if (searchHandler->isActive())
    {
        searchHandler->cancelSearch();
        m_activeSearches.remove(id);
    }

    return true;
}

bool SearchJobManager::deleteSearch(const int id)
{
    bool found = false;

    if (const auto iter = m_searchHandlers.constFind(id); iter != m_searchHandlers.cend())
    {
        iter.value()->cancelSearch();
        m_activeSearches.remove(id);
        m_searchHandlers.erase(iter);
        found = true;
    }

    if (const auto iter = m_restoredSearches.constFind(id); iter != m_restoredSearches.cend())
    {
        m_restoredSearches.erase(iter);
        found = true;
    }

    if (!found)
        return false;

    if (QTimer *timer = m_resultSaveTimers.take(id))
    {
        timer->stop();
        timer->deleteLater();
    }

    m_searchOrder.removeOne(id);
    removeSearchResults(id);
    saveSession();

    return true;
}

std::shared_ptr<SearchHandler> SearchJobManager::getSearch(const int id) const
{
    return m_searchHandlers.value(id);
}

QList<int> SearchJobManager::allSearchIds() const
{
    return m_searchOrder;
}

int SearchJobManager::activeSearchCount() const
{
    return static_cast<int>(m_activeSearches.size());
}

bool SearchJobManager::hasSearch(const int id) const
{
    return m_searchHandlers.contains(id) || m_restoredSearches.contains(id);
}

QString SearchJobManager::getPattern(const int id) const
{
    if (const auto iter = m_searchHandlers.constFind(id); iter != m_searchHandlers.cend())
        return iter.value()->pattern();
    if (const auto iter = m_restoredSearches.constFind(id); iter != m_restoredSearches.cend())
        return iter.value().pattern;
    return {};
}

QList<SearchResult> SearchJobManager::getResults(const int id) const
{
    if (const auto iter = m_searchHandlers.constFind(id); iter != m_searchHandlers.cend())
        return iter.value()->results();
    if (const auto iter = m_restoredSearches.constFind(id); iter != m_restoredSearches.cend())
        return iter.value().results;
    return {};
}

qsizetype SearchJobManager::getResultsCount(const int id) const
{
    if (const auto iter = m_searchHandlers.constFind(id); iter != m_searchHandlers.cend())
        return iter.value()->results().size();
    if (const auto iter = m_restoredSearches.constFind(id); iter != m_restoredSearches.cend())
        return iter.value().results.size();
    return 0;
}

bool SearchJobManager::isActive(const int id) const
{
    return m_activeSearches.contains(id);
}

int SearchJobManager::generateSearchId() const
{
    while (true)
    {
        const int id = Utils::Random::rand(1, std::numeric_limits<int>::max());
        if (!m_searchHandlers.contains(id) && !m_restoredSearches.contains(id))
            return id;
    }
}

Path SearchJobManager::makeDataFilePath(const QString &fileName) const
{
    return specialFolderLocation(SpecialFolder::Data) / DATA_FOLDER / Path(fileName);
}

void SearchJobManager::loadSession()
{
    const auto *pref = Preferences::instance();
    if (!pref->storeOpenedSearchTabs())
        return;

    const Path sessionFilePath = makeDataFilePath(SESSION_FILE_NAME);
    const auto readResult = Utils::IO::readFile(sessionFilePath, SESSION_FILE_MAX_SIZE);
    if (!readResult)
    {
        if (readResult.error().status != Utils::IO::ReadError::NotExist)
        {
            LogMsg(tr("Failed to load WebUI search session. File: \"%1\". Error: \"%2\"")
                    .arg(sessionFilePath.toString(), readResult.error().message), Log::WARNING);
        }
        return;
    }

    QJsonParseError jsonError;
    const QJsonDocument jsonDoc = QJsonDocument::fromJson(readResult.value(), &jsonError);
    if (jsonError.error != QJsonParseError::NoError)
    {
        LogMsg(tr("Failed to parse WebUI search session. File: \"%1\". Error: \"%2\"")
                .arg(sessionFilePath.toString(), jsonError.errorString()), Log::WARNING);
        return;
    }

    const QJsonObject jsonObj = jsonDoc.object();
    const QJsonArray tabsArray = jsonObj[u"Tabs"_s].toArray();

    const bool loadResults = pref->storeOpenedSearchTabResults();

    for (const QJsonValue &tabValue : tabsArray)
    {
        const QJsonObject tabObj = tabValue.toObject();
        const int tabId = tabObj[u"ID"_s].toInt();
        const QString searchPattern = tabObj[u"SearchPattern"_s].toString();

        if ((tabId <= 0) || searchPattern.isEmpty())
            continue;

        RestoredSearch restoredSearch;
        restoredSearch.pattern = searchPattern;

        if (loadResults)
        {
            const Path resultsFilePath = makeDataFilePath(QString::number(tabId) + u".json"_s);
            const auto resultsReadResult = Utils::IO::readFile(resultsFilePath, RESULTS_FILE_MAX_SIZE);
            if (resultsReadResult)
            {
                QJsonParseError resultsJsonError;
                const QJsonDocument resultsDoc = QJsonDocument::fromJson(resultsReadResult.value(), &resultsJsonError);
                if ((resultsJsonError.error == QJsonParseError::NoError) && resultsDoc.isArray())
                {
                    const QJsonArray resultsArray = resultsDoc.array();
                    for (const QJsonValue &resultValue : resultsArray)
                    {
                        const QJsonObject resultObj = resultValue.toObject();
                        SearchResult result;
                        result.fileName = resultObj[u"FileName"_s].toString();
                        result.fileUrl = resultObj[u"FileURL"_s].toString();
                        result.fileSize = resultObj[u"FileSize"_s].toInteger();
                        result.nbSeeders = resultObj[u"SeedersCount"_s].toInteger();
                        result.nbLeechers = resultObj[u"LeechersCount"_s].toInteger();
                        result.engineName = resultObj[u"EngineName"_s].toString();
                        result.siteUrl = resultObj[u"SiteURL"_s].toString();
                        result.descrLink = resultObj[u"DescrLink"_s].toString();
                        result.pubDate = QDateTime::fromSecsSinceEpoch(resultObj[u"PubDate"_s].toInteger());
                        restoredSearch.results.append(result);
                    }
                }
            }
        }

        m_restoredSearches.insert(tabId, restoredSearch);
        m_searchOrder.append(tabId);
    }
}

void SearchJobManager::saveSession() const
{
    const auto *pref = Preferences::instance();
    if (!pref->storeOpenedSearchTabs())
    {
        removeAllData();
        return;
    }

    QJsonArray tabsArray;

    // Save searches in chronological order (oldest first)
    for (const int searchId : m_searchOrder)
    {
        const QString pattern = getPattern(searchId);
        if (pattern.isEmpty())
            continue;

        tabsArray.append(QJsonObject {
            {u"ID"_s, searchId},
            {u"SearchPattern"_s, pattern}
        });
    }

    const QJsonObject sessionObj {
        {u"Tabs"_s, tabsArray}
    };

    const Path sessionFilePath = makeDataFilePath(SESSION_FILE_NAME);

    // Ensure directory exists
    Utils::Fs::mkpath(sessionFilePath.parentPath());

    const auto saveResult = Utils::IO::saveToFile(sessionFilePath, QJsonDocument(sessionObj).toJson());
    if (!saveResult)
    {
        LogMsg(tr("Failed to save WebUI search session. File: \"%1\". Error: \"%2\"")
                .arg(sessionFilePath.toString(), saveResult.error()), Log::WARNING);
    }
}

void SearchJobManager::scheduleSaveResults(const int searchId)
{
    if (!m_storeOpenedTabsResults)
        return;

    if (m_resultSaveTimers.contains(searchId))
        return;  // Timer already running, don't restart

    auto *timer = new QTimer(this);
    timer->setSingleShot(true);
    timer->setInterval(30000);  // 30 seconds
    connect(timer, &QTimer::timeout, this, [this, searchId, timer]
    {
        m_resultSaveTimers.remove(searchId);
        timer->deleteLater();
        saveSearchResults(searchId);
    });
    m_resultSaveTimers.insert(searchId, timer);
    timer->start();
}

void SearchJobManager::saveSearchResults(const int searchId) const
{
    const auto *pref = Preferences::instance();
    if (!pref->storeOpenedSearchTabs() || !pref->storeOpenedSearchTabResults())
        return;

    const auto iter = m_searchHandlers.constFind(searchId);
    if (iter == m_searchHandlers.cend())
        return;

    const std::shared_ptr<SearchHandler> &handler = iter.value();
    const QList<SearchResult> &results = handler->results();

    QJsonArray resultsArray;
    for (const SearchResult &result : results)
    {
        resultsArray.append(QJsonObject {
            {u"FileName"_s, result.fileName},
            {u"FileURL"_s, result.fileUrl},
            {u"FileSize"_s, result.fileSize},
            {u"SeedersCount"_s, result.nbSeeders},
            {u"LeechersCount"_s, result.nbLeechers},
            {u"EngineName"_s, result.engineName},
            {u"SiteURL"_s, result.siteUrl},
            {u"DescrLink"_s, result.descrLink},
            {u"PubDate"_s, result.pubDate.toSecsSinceEpoch()}
        });
    }

    const Path resultsFilePath = makeDataFilePath(QString::number(searchId) + u".json"_s);

    // Ensure directory exists
    Utils::Fs::mkpath(resultsFilePath.parentPath());

    const auto saveResult = Utils::IO::saveToFile(resultsFilePath, QJsonDocument(resultsArray).toJson());
    if (!saveResult)
    {
        LogMsg(tr("Failed to save WebUI search results. File: \"%1\". Error: \"%2\"")
                .arg(resultsFilePath.toString(), saveResult.error()), Log::WARNING);
    }
}

void SearchJobManager::removeSearchResults(const int searchId) const
{
    const Path resultsFilePath = makeDataFilePath(QString::number(searchId) + u".json"_s);
    Utils::Fs::removeFile(resultsFilePath);
}

void SearchJobManager::removeAllData() const
{
    const Path dataFolderPath = specialFolderLocation(SpecialFolder::Data) / DATA_FOLDER;
    Utils::Fs::removeDirRecursively(dataFolderPath);
}

void SearchJobManager::removeAllResultFiles() const
{
    const Path dataFolderPath = specialFolderLocation(SpecialFolder::Data) / DATA_FOLDER;
    // Remove all .json files except Session.json
    const QStringList entries = QDir(dataFolderPath.data()).entryList({u"*.json"_s}, QDir::Files);
    for (const QString &entry : entries)
    {
        if (entry != SESSION_FILE_NAME)
            Utils::Fs::removeFile(dataFolderPath / Path(entry));
    }
}

void SearchJobManager::onPreferencesChanged()
{
    const auto *pref = Preferences::instance();

    const bool storeOpenedTabs = pref->storeOpenedSearchTabs();
    if (storeOpenedTabs != m_storeOpenedTabs)
    {
        m_storeOpenedTabs = storeOpenedTabs;
        if (m_storeOpenedTabs)
            saveSession();
        else
            removeAllData();
    }

    const bool storeOpenedTabsResults = pref->storeOpenedSearchTabResults();
    if (storeOpenedTabsResults != m_storeOpenedTabsResults)
    {
        m_storeOpenedTabsResults = storeOpenedTabsResults;
        if (m_storeOpenedTabsResults)
        {
            // Save results for all current searches
            for (auto it = m_searchHandlers.cbegin(); it != m_searchHandlers.cend(); ++it)
            {
                if (!it.value()->isActive())
                    saveSearchResults(it.key());
            }
        }
        else
        {
            removeAllResultFiles();
        }
    }
}
