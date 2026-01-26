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

#include <QString>
#include <QStringList>

#include "base/global.h"
#include "base/search/searchhandler.h"
#include "base/search/searchpluginmanager.h"
#include "base/utils/random.h"

SearchJobManager::SearchJobManager(QObject *parent)
    : QObject(parent)
{
}

int SearchJobManager::startSearch(const QString &pattern, const QString &category, const QStringList &plugins)
{
    if (m_activeSearches.size() >= MAX_CONCURRENT_SEARCHES)
        return -1;

    const int id = generateSearchId();
    const std::shared_ptr<SearchHandler> searchHandler {SearchPluginManager::instance()->startSearch(pattern, category, plugins)};

    connect(searchHandler.get(), &SearchHandler::searchFinished, this, [this, id] { m_activeSearches.remove(id); });
    connect(searchHandler.get(), &SearchHandler::searchFailed, this, [this, id]([[maybe_unused]] const QString &errorMessage) { m_activeSearches.remove(id); });

    m_searchHandlers.insert(id, searchHandler);
    m_activeSearches.insert(id);
    m_searchOrder.append(id);

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
    const auto iter = m_searchHandlers.constFind(id);
    if (iter == m_searchHandlers.cend())
        return false;

    iter.value()->cancelSearch();
    m_activeSearches.remove(id);
    m_searchHandlers.erase(iter);
    m_searchOrder.removeOne(id);

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

int SearchJobManager::generateSearchId() const
{
    while (true)
    {
        const int id = Utils::Random::rand(1, std::numeric_limits<int>::max());
        if (!m_searchHandlers.contains(id))
            return id;
    }
}
