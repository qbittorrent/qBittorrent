/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015, 2018, 2021  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2006  Christophe Dumez <chris@qbittorrent.org>
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

#include <QtContainerFwd>
#include <QList>
#include <QObject>
#include <QSet>
#include <QString>

#include "searchresult.h"

class QThread;
class QTimer;

class SearchEngine;
class TorznabRSSParser;
struct TorznabRSSParsingResult;

namespace Net
{
    class DownloadHandler;
}

class SearchHandler final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(SearchHandler)

    friend class SearchEngine;

    SearchHandler(const QString &pattern, const QString &category, SearchEngine *manager);

public:
    ~SearchHandler() override;

    bool isActive() const;
    QString pattern() const;
    SearchEngine *manager() const;
    QList<SearchResult> results() const;

    void cancelSearch();

signals:
    void searchFinished(bool cancelled = false);
    void searchFailed();
    void newSearchResults(const QList<SearchResult> &results);

private:
    void handleParsingFinished(const TorznabRSSParsingResult &result);

    const QString m_pattern;
    const QString m_category;
    SearchEngine *m_manager;
    QThread *m_parsingThread;
    QTimer *m_searchTimeout;
    TorznabRSSParser *m_parser;
    int m_numOutstandingRequests = 0;
    bool m_searchCancelled = false;
    QList<SearchResult> m_results;
    QSet<Net::DownloadHandler *> m_downloadHandlers;
};
