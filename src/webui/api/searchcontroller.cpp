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

#include "base/global.h"
#include "base/logger.h"
#include "base/search/searchhandler.h"
#include "base/utils/foreignapps.h"
#include "base/utils/random.h"
#include "base/utils/string.h"
#include "apierror.h"
#include "isessionmanager.h"

using SearchHandlerPtr = QSharedPointer<SearchHandler>;
using SearchHandlerDict = QMap<int, SearchHandlerPtr>;

namespace
{
    const QLatin1String ACTIVE_SEARCHES("activeSearches");
    const QLatin1String SEARCH_HANDLERS("searchHandlers");

    void removeActiveSearch(ISession *session, const int id)
    {
        auto activeSearches = session->getData<QSet<int>>(ACTIVE_SEARCHES);
        if (activeSearches.remove(id))
            session->setData(ACTIVE_SEARCHES, QVariant::fromValue(activeSearches));
    }

    /**
    * Returns the search categories in JSON format.
    *
    * The return value is an array of dictionaries.
    * The dictionary keys are:
    *   - "id"
    *   - "name"
    */
    QJsonArray getPluginCategories(QStringList categories)
    {
        QJsonArray categoriesInfo
        {QJsonObject {
            {QLatin1String("id"), "all"},
            {QLatin1String("name"), SearchPluginManager::categoryFullName("all")}
        }};

        categories.sort(Qt::CaseInsensitive);
        for (const QString &category : categories)
        {
            categoriesInfo << QJsonObject
            {
                {QLatin1String("id"), category},
                {QLatin1String("name"), SearchPluginManager::categoryFullName(category)}
            };
        }

        return categoriesInfo;
    }
}

void SearchController::startAction()
{
    requireParams({"pattern", "category", "plugins"});

    if (!Utils::ForeignApps::pythonInfo().isValid())
        throw APIError(APIErrorType::Conflict, tr("Python must be installed to use the Search Engine."));

    const QString pattern = params()["pattern"].trimmed();
    const QString category = params()["category"].trimmed();
    const QStringList plugins = params()["plugins"].split('|');

    QStringList pluginsToUse;
    if (plugins.size() == 1)
    {
        const QString pluginsLower = plugins[0].toLower();
        if (pluginsLower == "all")
            pluginsToUse = SearchPluginManager::instance()->allPlugins();
        else if ((pluginsLower == "enabled") || (pluginsLower == "multi"))
            pluginsToUse = SearchPluginManager::instance()->enabledPlugins();
        else
            pluginsToUse << plugins;
    }
    else
    {
        pluginsToUse << plugins;
    }

    ISession *const session = sessionManager()->session();
    auto activeSearches = session->getData<QSet<int>>(ACTIVE_SEARCHES);
    if (activeSearches.size() >= MAX_CONCURRENT_SEARCHES)
        throw APIError(APIErrorType::Conflict, tr("Unable to create more than %1 concurrent searches.").arg(MAX_CONCURRENT_SEARCHES));

    const auto id = generateSearchId();
    const SearchHandlerPtr searchHandler {SearchPluginManager::instance()->startSearch(pattern, category, pluginsToUse)};
    QObject::connect(searchHandler.data(), &SearchHandler::searchFinished, this, [session, id, this]() { searchFinished(session, id); });
    QObject::connect(searchHandler.data(), &SearchHandler::searchFailed, this, [session, id, this]() { searchFailed(session, id); });

    auto searchHandlers = session->getData<SearchHandlerDict>(SEARCH_HANDLERS);
    searchHandlers.insert(id, searchHandler);
    session->setData(SEARCH_HANDLERS, QVariant::fromValue(searchHandlers));

    activeSearches.insert(id);
    session->setData(ACTIVE_SEARCHES, QVariant::fromValue(activeSearches));

    const QJsonObject result = {{"id", id}};
    setResult(result);
}

void SearchController::stopAction()
{
    requireParams({"id"});

    const int id = params()["id"].toInt();
    ISession *const session = sessionManager()->session();

    const auto searchHandlers = session->getData<SearchHandlerDict>(SEARCH_HANDLERS);
    if (!searchHandlers.contains(id))
        throw APIError(APIErrorType::NotFound);

    const SearchHandlerPtr searchHandler = searchHandlers[id];

    if (searchHandler->isActive())
    {
        searchHandler->cancelSearch();
        removeActiveSearch(session, id);
    }
}

void SearchController::statusAction()
{
    const int id = params()["id"].toInt();

    const auto searchHandlers = sessionManager()->session()->getData<SearchHandlerDict>(SEARCH_HANDLERS);
    if ((id != 0) && !searchHandlers.contains(id))
        throw APIError(APIErrorType::NotFound);

    QJsonArray statusArray;
    const QList<int> searchIds {(id == 0) ? searchHandlers.keys() : QList<int> {id}};

    for (const int searchId : searchIds)
    {
        const SearchHandlerPtr searchHandler = searchHandlers[searchId];
        statusArray << QJsonObject
        {
            {"id", searchId},
            {"status", searchHandler->isActive() ? "Running" : "Stopped"},
            {"total", searchHandler->results().size()}
        };
    }

    setResult(statusArray);
}

void SearchController::resultsAction()
{
    requireParams({"id"});

    const int id = params()["id"].toInt();
    int limit = params()["limit"].toInt();
    int offset = params()["offset"].toInt();

    const auto searchHandlers = sessionManager()->session()->getData<SearchHandlerDict>(SEARCH_HANDLERS);
    if (!searchHandlers.contains(id))
        throw APIError(APIErrorType::NotFound);

    const SearchHandlerPtr searchHandler = searchHandlers[id];
    const QList<SearchResult> searchResults = searchHandler->results();
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
    requireParams({"id"});

    const int id = params()["id"].toInt();
    ISession *const session = sessionManager()->session();

    auto searchHandlers = session->getData<SearchHandlerDict>(SEARCH_HANDLERS);
    if (!searchHandlers.contains(id))
        throw APIError(APIErrorType::NotFound);

    const SearchHandlerPtr searchHandler = searchHandlers[id];
    searchHandler->cancelSearch();
    searchHandlers.remove(id);
    session->setData(SEARCH_HANDLERS, QVariant::fromValue(searchHandlers));

    removeActiveSearch(session, id);
}

void SearchController::pluginsAction()
{
    const QStringList allPlugins = SearchPluginManager::instance()->allPlugins();
    setResult(getPluginsInfo(allPlugins));
}

void SearchController::installPluginAction()
{
    requireParams({"sources"});

    const QStringList sources = params()["sources"].split('|');
    for (const QString &source : sources)
        SearchPluginManager::instance()->installPlugin(source);
}

void SearchController::uninstallPluginAction()
{
    requireParams({"names"});

    const QStringList names = params()["names"].split('|');
    for (const QString &name : names)
        SearchPluginManager::instance()->uninstallPlugin(name.trimmed());
}

void SearchController::enablePluginAction()
{
    requireParams({"names", "enable"});

    const QStringList names = params()["names"].split('|');
    const bool enable = Utils::String::parseBool(params()["enable"].trimmed()).value_or(false);

    for (const QString &name : names)
        SearchPluginManager::instance()->enablePlugin(name.trimmed(), enable);
}

void SearchController::updatePluginsAction()
{
    SearchPluginManager *const pluginManager = SearchPluginManager::instance();

    connect(pluginManager, &SearchPluginManager::checkForUpdatesFinished, this, &SearchController::checkForUpdatesFinished);
    connect(pluginManager, &SearchPluginManager::checkForUpdatesFailed, this, &SearchController::checkForUpdatesFailed);
    pluginManager->checkForUpdates();
}

void SearchController::checkForUpdatesFinished(const QHash<QString, PluginVersion> &updateInfo)
{
    if (updateInfo.isEmpty())
    {
        LogMsg(tr("All plugins are already up to date."), Log::INFO);
        return;
    }

    LogMsg(tr("Updating %1 plugins").arg(updateInfo.size()), Log::INFO);

    SearchPluginManager *const pluginManager = SearchPluginManager::instance();
    for (const QString &pluginName : asConst(updateInfo.keys()))
    {
        LogMsg(tr("Updating plugin %1").arg(pluginName), Log::INFO);
        pluginManager->updatePlugin(pluginName);
    }
}

void SearchController::checkForUpdatesFailed(const QString &reason)
{
    LogMsg(tr("Failed to check for plugin updates: %1").arg(reason), Log::INFO);
}

void SearchController::searchFinished(ISession *session, const int id)
{
    removeActiveSearch(session, id);
}

void SearchController::searchFailed(ISession *session, const int id)
{
    removeActiveSearch(session, id);
}

int SearchController::generateSearchId() const
{
    const auto searchHandlers = sessionManager()->session()->getData<SearchHandlerDict>(SEARCH_HANDLERS);

    while (true)
    {
        const int id = Utils::Random::rand(1, std::numeric_limits<int>::max());
        if (!searchHandlers.contains(id))
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
QJsonObject SearchController::getResults(const QList<SearchResult> &searchResults, const bool isSearchActive, const int totalResults) const
{
    QJsonArray searchResultsArray;
    for (const SearchResult &searchResult : searchResults)
    {
        searchResultsArray << QJsonObject
        {
            {"fileName", searchResult.fileName},
            {"fileUrl", searchResult.fileUrl},
            {"fileSize", searchResult.fileSize},
            {"nbSeeders", searchResult.nbSeeders},
            {"nbLeechers", searchResult.nbLeechers},
            {"siteUrl", searchResult.siteUrl},
            {"descrLink", searchResult.descrLink}
        };
    }

    const QJsonObject result =
    {
        {"status", isSearchActive ? "Running" : "Stopped"},
        {"results", searchResultsArray},
        {"total", totalResults}
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
QJsonArray SearchController::getPluginsInfo(const QStringList &plugins) const
{
    QJsonArray pluginsArray;

    for (const QString &plugin : plugins)
    {
        const PluginInfo *const pluginInfo = SearchPluginManager::instance()->pluginInfo(plugin);

        pluginsArray << QJsonObject
        {
            {"name", pluginInfo->name},
            {"version", QString(pluginInfo->version)},
            {"fullName", pluginInfo->fullName},
            {"url", pluginInfo->url},
            {"supportedCategories", getPluginCategories(pluginInfo->supportedCategories)},
            {"enabled", pluginInfo->enabled}
        };
    }

    return pluginsArray;
}
