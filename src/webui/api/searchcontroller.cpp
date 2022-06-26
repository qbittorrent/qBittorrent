/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2018  Thomas Piccirello <thomas.piccirello@gmail.com>
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

#include "searchcontroller.h"

#include <limits>

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QSharedPointer>
#include <QVector>

#include "base/global.h"
#include "base/logger.h"
#include "base/search/searchhandler.h"
#include "base/utils/random.h"
#include "base/utils/string.h"
#include "apierror.h"
#include "isessionmanager.h"

namespace
{
    /**
    * Returns the search categories in JSON format.
    *
    * The return value is an array of dictionaries.
    * The dictionary keys are:
    *   - "id"
    *   - "name"
    */
    QJsonArray getIndexerCategories(QStringList categories)
    {
        QJsonArray categoriesInfo {
            QJsonObject {
                {u"id"_qs, u"all"_qs},
                {u"name"_qs, SearchEngine::categoryFullName(u"all"_qs)}
            }
        };

        categories.sort(Qt::CaseInsensitive);
        for (const QString &category : categories)
        {
            categoriesInfo.append(QJsonObject {
                {u"id"_qs, category},
                {u"name"_qs, SearchEngine::categoryFullName(category)}
            });
        }

        return categoriesInfo;
    }
}

void SearchController::startAction()
{
    requireParams({u"pattern"_qs, u"category"_qs});

    const QString pattern = params()[u"pattern"_qs].trimmed();
    const QString category = params()[u"category"_qs].trimmed();

    if (m_activeSearches.size() >= MAX_CONCURRENT_SEARCHES)
        throw APIError(APIErrorType::Conflict, tr("Unable to create more than %1 concurrent searches.").arg(MAX_CONCURRENT_SEARCHES));

    const auto id = generateSearchId();
    const std::shared_ptr<SearchHandler> searchHandler {SearchEngine::instance()->startSearch(pattern, category)};
    if (!searchHandler)
        throw APIError(APIErrorType::Conflict, tr("There are no enabled indexers."));

    QObject::connect(searchHandler.get(), &SearchHandler::searchFinished, this, [id, this]() { m_activeSearches.remove(id); });
    QObject::connect(searchHandler.get(), &SearchHandler::searchFailed, this, [id, this]() { m_activeSearches.remove(id); });

    m_searchHandlers.insert(id, searchHandler);
    m_activeSearches.insert(id);

    const QJsonObject result = {{u"id"_qs, id}};
    setResult(result);
}

void SearchController::stopAction()
{
    requireParams({u"id"_qs});

    const int id = params()[u"id"_qs].toInt();

    const auto iter = m_searchHandlers.find(id);
    if (iter == m_searchHandlers.end())
        throw APIError(APIErrorType::NotFound);

    const std::shared_ptr<SearchHandler> &searchHandler = iter.value();

    if (searchHandler->isActive())
    {
        searchHandler->cancelSearch();
        m_activeSearches.remove(id);
    }
}

void SearchController::statusAction()
{
    const int id = params()[u"id"_qs].toInt();

    if ((id != 0) && !m_searchHandlers.contains(id))
        throw APIError(APIErrorType::NotFound);

    QJsonArray statusArray;
    const QList<int> searchIds {(id == 0) ? m_searchHandlers.keys() : QList<int> {id}};

    for (const int searchId : searchIds)
    {
        const std::shared_ptr<SearchHandler> &searchHandler = m_searchHandlers[searchId];
        statusArray << QJsonObject
        {
            {u"id"_qs, searchId},
            {u"status"_qs, searchHandler->isActive() ? u"Running"_qs : u"Stopped"_qs},
            {u"total"_qs, searchHandler->results().size()}
        };
    }

    setResult(statusArray);
}

void SearchController::resultsAction()
{
    requireParams({u"id"_qs});

    const int id = params()[u"id"_qs].toInt();
    int limit = params()[u"limit"_qs].toInt();
    int offset = params()[u"offset"_qs].toInt();

    const auto iter = m_searchHandlers.find(id);
    if (iter == m_searchHandlers.end())
        throw APIError(APIErrorType::NotFound);

    const std::shared_ptr<SearchHandler> &searchHandler = iter.value();
    const QVector<SearchResult> searchResults = searchHandler->results();
    const int size = searchResults.size();

    if (offset > size)
        throw APIError(APIErrorType::Conflict, tr("Offset is out of range"));

    // normalize values
    if (offset < 0)
        offset = size + offset;
    if (offset < 0)  // check again
        throw APIError(APIErrorType::Conflict, tr("Offset is out of range"));
    if (limit <= 0)
        limit = -1;

    if ((limit > 0) || (offset > 0))
        setResult(getResults(searchResults.mid(offset, limit), searchHandler->isActive(), size));
    else
        setResult(getResults(searchResults, searchHandler->isActive(), size));
}

void SearchController::deleteAction()
{
    requireParams({u"id"_qs});

    const int id = params()[u"id"_qs].toInt();

    const auto iter = m_searchHandlers.find(id);
    if (iter == m_searchHandlers.end())
        throw APIError(APIErrorType::NotFound);

    const std::shared_ptr<SearchHandler> &searchHandler = iter.value();
    searchHandler->cancelSearch();
    m_activeSearches.remove(id);
    m_searchHandlers.erase(iter);
}

void SearchController::pluginsAction()
{
    setResult(getIndexersInfo());
}

void SearchController::installPluginAction()
{
}

void SearchController::uninstallPluginAction()
{
}

void SearchController::enablePluginAction()
{
    requireParams({u"names"_qs, u"enable"_qs});

    const QStringList names = params()[u"names"_qs].split(u'|');
    const bool enable = Utils::String::parseBool(params()[u"enable"_qs].trimmed()).value_or(false);

    for (const QString &name : names)
        SearchEngine::instance()->enableIndexer(name.trimmed(), enable);
}

void SearchController::updatePluginsAction()
{
}

int SearchController::generateSearchId() const
{
    while (true)
    {
        const int id = Utils::Random::rand(1, std::numeric_limits<int>::max());
        if (!m_searchHandlers.contains(id))
            return id;
    }
}

/**
 * Returns the search results in JSON format.
 *
 * The return value is an object with a status and an array of dictionaries.
 * The dictionary keys are:
 *   - "fileName"
 *   - "fileUrl"
 *   - "fileSize"
 *   - "nbSeeders"
 *   - "nbLeechers"
 *   - "siteUrl"
 *   - "descrLink"
 */
QJsonObject SearchController::getResults(const QVector<SearchResult> &searchResults, const bool isSearchActive, const int totalResults) const
{
    QJsonArray searchResultsArray;
    for (const SearchResult &searchResult : searchResults)
    {
        searchResultsArray.append(QJsonObject {
            {u"fileName"_qs, searchResult.fileName},
            {u"fileUrl"_qs, searchResult.fileUrl},
            {u"fileSize"_qs, searchResult.fileSize},
            {u"nbSeeders"_qs, searchResult.numSeeders},
            {u"nbLeechers"_qs, searchResult.numLeechers},
            {u"siteUrl"_qs, searchResult.indexerName},
            {u"descrLink"_qs, searchResult.descrLink}
        });
    }

    const QJsonObject result {
        {u"status"_qs, isSearchActive ? u"Running"_qs : u"Stopped"_qs},
        {u"results"_qs, searchResultsArray},
        {u"total"_qs, totalResults}
    };

    return result;
}

/**
 * Returns the search plugins in JSON format.
 *
 * The return value is an array of dictionaries.
 * The dictionary keys are:
 *   - "name"
 *   - "version"
 *   - "fullName"
 *   - "url"
 *   - "supportedCategories"
 *   - "iconPath"
 *   - "enabled"
 */
QJsonArray SearchController::getIndexersInfo() const
{
    QJsonArray indexersArray;

    const auto indexers = SearchEngine::instance()->indexers();
    for (auto it = indexers.cbegin(); it != indexers.cend(); ++it)
    {
        const QString indexerName = it.key();
        const IndexerInfo indexerInfo = it.value();

        indexersArray.append(QJsonObject {
            {u"name"_qs, indexerName},
            {u"version"_qs, {}},
            {u"fullName"_qs, indexerName},
            {u"url"_qs, indexerInfo.options.url},
            {u"supportedCategories"_qs, getIndexerCategories(SearchEngine::instance()->supportedCategories())},
            {u"enabled"_qs, indexerInfo.enabled}
        });
    }

    return indexersArray;
}
