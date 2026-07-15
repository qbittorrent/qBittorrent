/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2018, 2026  Thomas Piccirello <thomas@piccirello.com>
 * Copyright (C) 2024  Vladimir Golovnev <glassez@yandex.ru>
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
const Path DATA_FOLDER = Path(u"WebUI/Search"_s);
const QString SESSION_FILE_NAME = u"Session.json"_s;
const int SESSION_FILE_MAX_SIZE = 1024 * 1024; // 1 MiB
const int RESULTS_FILE_MAX_SIZE = 10 * 1024 * 1024; // 10 MiB
const int MAX_CONCURRENT_SEARCHES = 5;

const QString KEY_JOBS = u"Jobs"_s;
const QString KEY_JOB_ID = u"ID"_s;
const QString KEY_JOB_SEARCH_PATTERN = u"SearchPattern"_s;
const QString KEY_JOB_CATEGORY = u"Category"_s;
const QString KEY_JOB_PLUGINS = u"Plugins"_s;

const QString KEY_RESULT_FILE_NAME = u"FileName"_s;
const QString KEY_RESULT_FILE_URL = u"FileURL"_s;
const QString KEY_RESULT_FILE_SIZE = u"FileSize"_s;
const QString KEY_RESULT_SEEDERS_COUNT = u"SeedersCount"_s;
const QString KEY_RESULT_LEECHERS_COUNT = u"LeechersCount"_s;
const QString KEY_RESULT_ENGINE_NAME = u"EngineName"_s;
const QString KEY_RESULT_SITE_URL = u"SiteURL"_s;
const QString KEY_RESULT_DESCR_LINK = u"DescrLink"_s;
const QString KEY_RESULT_PUB_DATE = u"PubDate"_s;
}

SearchJobManager::SearchJobManager(QObject *parent)
    : QObject(parent)
    , m_storeSearchJobs {Preferences::instance()->storeSearchJobs()}
    , m_storeSearchJobResults {Preferences::instance()->storeSearchJobResults()}
{
    connect(Preferences::instance(), &Preferences::changed, this, &SearchJobManager::onPreferencesChanged);
    loadSession();
}

nonstd::expected<int, QString> SearchJobManager::startSearch(const QString &pattern, const QString &category, const QStringList &plugins)
{
    if (m_searchHandlers.size() >= MAX_CONCURRENT_SEARCHES)
        return nonstd::make_unexpected(tr("Unable to create more than %1 concurrent searches.").arg(MAX_CONCURRENT_SEARCHES));

    const int id = generateSearchId();
    const std::shared_ptr<SearchHandler> searchHandler = std::shared_ptr<SearchHandler>(SearchPluginManager::instance()->startSearch(pattern, category, plugins));

    connect(searchHandler.get(), &SearchHandler::searchFinished, this, [this, id]
    {
        const auto jobIter = m_jobs.find(id);
        if (jobIter == m_jobs.end())
            return;

        jobIter->active = false;
        m_searchHandlers.remove(id);
        saveSearchResults(id);
    });
    connect(searchHandler.get(), &SearchHandler::searchFailed, this, [this, id]([[maybe_unused]] const QString &errorMessage)
    {
        const auto jobIter = m_jobs.find(id);
        if (jobIter == m_jobs.end())
            return;

        jobIter->active = false;
        m_searchHandlers.remove(id);
        saveSearchResults(id);
    });
    connect(searchHandler.get(), &SearchHandler::newSearchResults, this, [this, id, handler = searchHandler.get()]
    {
        const auto jobIter = m_jobs.find(id);
        if (jobIter == m_jobs.end())
            return;

        jobIter->results = handler->results();
    });

    m_searchHandlers.insert(id, searchHandler);
    m_jobs.insert(id, {.id = id, .pattern = pattern, .category = category, .plugins = plugins, .active = true, .results = {}});
    m_searchOrder.append(id);

    saveSession();

    return id;
}

bool SearchJobManager::stopSearch(const int id)
{
    const std::shared_ptr<SearchHandler> searchHandler = m_searchHandlers.value(id);
    if (!searchHandler)
        return false;

    if (searchHandler->isActive())
        searchHandler->cancelSearch();

    return true;
}

bool SearchJobManager::deleteSearch(const int id)
{
    if (!m_jobs.remove(id))
        return false;

    if (const std::shared_ptr<SearchHandler> searchHandler = m_searchHandlers.take(id))
        searchHandler->cancelSearch();

    m_searchOrder.removeOne(id);
    removeSearchResults(id);
    saveSession();

    return true;
}

std::optional<SearchJobManager::JobInfo> SearchJobManager::getJobInfo(const int id) const
{
    const auto iter = m_jobs.constFind(id);
    if (iter == m_jobs.cend())
        return std::nullopt;

    return *iter;
}

QList<SearchJobManager::JobInfo> SearchJobManager::getAllJobInfos() const
{
    QList<JobInfo> result;
    result.reserve(m_searchOrder.size());
    for (const int id : asConst(m_searchOrder))
    {
        const auto info = getJobInfo(id);
        Q_ASSERT(info);
        if (info) [[likely]]
            result.append(*info);
    }
    return result;
}

int SearchJobManager::generateSearchId() const
{
    while (true)
    {
        const int id = Utils::Random::rand(1, std::numeric_limits<int>::max());
        if (!m_jobs.contains(id))
            return id;
    }
}

Path SearchJobManager::makeDataFilePath(const QString &fileName) const
{
    return specialFolderLocation(SpecialFolder::Data) / DATA_FOLDER / Path(fileName);
}

Path SearchJobManager::makeResultsFilePath(const int searchId) const
{
    return makeDataFilePath(QString::number(searchId).rightJustified(4, u'0') + u".json"_s);
}

void SearchJobManager::loadSession()
{
    if (!m_storeSearchJobs)
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
    const QJsonArray jobsArray = jsonObj[KEY_JOBS].toArray();

    for (const QJsonValue &jobValue : jobsArray)
    {
        const QJsonObject jobObj = jobValue.toObject();
        const int jobId = jobObj[KEY_JOB_ID].toInt();
        const QString searchPattern = jobObj[KEY_JOB_SEARCH_PATTERN].toString();

        if ((jobId <= 0) || searchPattern.isEmpty())
            continue;

        JobInfo jobInfo;
        jobInfo.id = jobId;
        jobInfo.pattern = searchPattern;
        jobInfo.category = jobObj[KEY_JOB_CATEGORY].toString();
        for (const QJsonValue &pluginValue : jobObj[KEY_JOB_PLUGINS].toArray())
            jobInfo.plugins.append(pluginValue.toString());

        if (m_storeSearchJobResults)
        {
            const Path resultsFilePath = makeResultsFilePath(jobId);
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
                        const SearchResult result {
                            .fileName = resultObj[KEY_RESULT_FILE_NAME].toString(),
                            .fileUrl = resultObj[KEY_RESULT_FILE_URL].toString(),
                            .fileSize = resultObj[KEY_RESULT_FILE_SIZE].toInteger(),
                            .nbSeeders = resultObj[KEY_RESULT_SEEDERS_COUNT].toInteger(),
                            .nbLeechers = resultObj[KEY_RESULT_LEECHERS_COUNT].toInteger(),
                            .engineName = resultObj[KEY_RESULT_ENGINE_NAME].toString(),
                            .siteUrl = resultObj[KEY_RESULT_SITE_URL].toString(),
                            .descrLink = resultObj[KEY_RESULT_DESCR_LINK].toString(),
                            .pubDate = QDateTime::fromSecsSinceEpoch(resultObj[KEY_RESULT_PUB_DATE].toInteger())
                        };
                        jobInfo.results.append(result);
                    }
                }
            }
        }

        m_jobs.insert(jobId, jobInfo);
        m_searchOrder.append(jobId);
    }
}

void SearchJobManager::saveSession() const
{
    if (!m_storeSearchJobs)
    {
        removeAllData();
        return;
    }

    QJsonArray jobsArray;

    // Save searches in chronological order (oldest first)
    for (const int searchId : asConst(m_searchOrder))
    {
        const auto jobInfo = getJobInfo(searchId);
        Q_ASSERT(jobInfo);
        if (!jobInfo) [[unlikely]]
            continue;

        jobsArray.append(QJsonObject {
            {KEY_JOB_ID, searchId},
            {KEY_JOB_SEARCH_PATTERN, jobInfo->pattern},
            {KEY_JOB_CATEGORY, jobInfo->category},
            {KEY_JOB_PLUGINS, QJsonArray::fromStringList(jobInfo->plugins)}
        });
    }

    const QJsonObject sessionObj {
        {KEY_JOBS, jobsArray}
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

void SearchJobManager::saveSearchResults(const int searchId) const
{
    if (!m_storeSearchJobs || !m_storeSearchJobResults)
        return;

    const auto jobInfo = getJobInfo(searchId);
    if (!jobInfo)
        return;

    QJsonArray resultsArray;
    for (const SearchResult &result : jobInfo->results)
    {
        resultsArray.append(QJsonObject {
            {KEY_RESULT_FILE_NAME, result.fileName},
            {KEY_RESULT_FILE_URL, result.fileUrl},
            {KEY_RESULT_FILE_SIZE, result.fileSize},
            {KEY_RESULT_SEEDERS_COUNT, result.nbSeeders},
            {KEY_RESULT_LEECHERS_COUNT, result.nbLeechers},
            {KEY_RESULT_ENGINE_NAME, result.engineName},
            {KEY_RESULT_SITE_URL, result.siteUrl},
            {KEY_RESULT_DESCR_LINK, result.descrLink},
            {KEY_RESULT_PUB_DATE, result.pubDate.toSecsSinceEpoch()}
        });
    }

    const Path resultsFilePath = makeResultsFilePath(searchId);

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
    const Path resultsFilePath = makeResultsFilePath(searchId);
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

    const bool storeSearchJobs = pref->storeSearchJobs();
    if (storeSearchJobs != m_storeSearchJobs)
    {
        m_storeSearchJobs = storeSearchJobs;
        saveSession();
    }

    const bool storeSearchJobResults = pref->storeSearchJobResults();
    if (storeSearchJobResults != m_storeSearchJobResults)
    {
        m_storeSearchJobResults = storeSearchJobResults;
        if (m_storeSearchJobResults && m_storeSearchJobs)
        {
            // Save results for all completed searches
            for (const JobInfo &job : asConst(m_jobs))
            {
                if (!job.active)
                    saveSearchResults(job.id);
            }
        }
        else
        {
            removeAllResultFiles();
        }
    }
}
