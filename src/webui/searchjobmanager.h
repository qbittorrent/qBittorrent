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

#pragma once

#include <memory>

#include <QHash>
#include <QObject>
#include <QSet>

class QTimer;

#include "base/path.h"
#include "base/search/searchhandler.h"

class SearchJobManager final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(SearchJobManager)

public:
    static constexpr int MAX_CONCURRENT_SEARCHES = 5;

    explicit SearchJobManager(QObject *parent = nullptr);

    int startSearch(const QString &pattern, const QString &category, const QStringList &plugins);
    bool stopSearch(int id);
    bool deleteSearch(int id);

    std::shared_ptr<SearchHandler> getSearch(int id) const;
    QList<int> allSearchIds() const;
    int activeSearchCount() const;

    // Methods that work with both live and restored searches
    bool hasSearch(int id) const;
    QString getPattern(int id) const;
    QList<SearchResult> getResults(int id) const;
    qsizetype getResultsCount(int id) const;
    bool isActive(int id) const;

private slots:
    void onPreferencesChanged();

private:
    struct RestoredSearch
    {
        QString pattern;
        QList<SearchResult> results;
    };

    int generateSearchId() const;
    void loadSession();
    void saveSession() const;
    void saveSearchResults(int searchId) const;
    void scheduleSaveResults(int searchId);
    void removeSearchResults(int searchId) const;
    void removeAllResultFiles() const;
    void removeAllData() const;
    Path makeDataFilePath(const QString &fileName) const;

    bool m_storeOpenedTabs = false;
    bool m_storeOpenedTabsResults = false;

    QHash<int, std::shared_ptr<SearchHandler>> m_searchHandlers;
    QSet<int> m_activeSearches;
    QHash<int, RestoredSearch> m_restoredSearches;
    QList<int> m_searchOrder;  // Tracks insertion order (oldest first)
    QHash<int, QTimer*> m_resultSaveTimers;
};
