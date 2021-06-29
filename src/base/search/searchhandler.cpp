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

#include "searchhandler.h"

#include <QThread>
#include <QTimer>
#include <QVector>

#include "base/global.h"
#include "base/net/downloadmanager.h"
#include "searchengine.h"
#include "torznabrssparser.h"

namespace
{
    QString makeQueryString(const QHash<QString, QString> &params)
    {
        QStringList encodedParams;
        for (auto it = params.cbegin(); it != params.cend(); ++it)
            encodedParams.append(it.key() + QLatin1Char('=') + QString::fromLatin1(it.value().toUtf8().toPercentEncoding()));

        return encodedParams.join(QLatin1Char('&'));
    }
}

SearchHandler::SearchHandler(const QString &pattern, const QString &category, SearchEngine *manager)
    : QObject {manager}
    , m_pattern {pattern}
    , m_category {category}
    , m_manager {manager}
    , m_parsingThread {new QThread(this)}
    , m_searchTimeout {new QTimer(this)}
    , m_parser {new TorznabRSSParser}
{
    connect(m_parser, &TorznabRSSParser::finished, this, &SearchHandler::handleParsingFinished);
    m_parser->moveToThread(m_parsingThread);
    connect(m_parsingThread, &QThread::finished, m_parser, &QObject::deleteLater);
    m_parsingThread->start();

    const QHash<QString, QStringList> supportedCategories
    {
        {QLatin1String("all"), {}},
        {QLatin1String("anime"), {QLatin1String("5070")}},
        {QLatin1String("books"), {QLatin1String("8000")}},
        {QLatin1String("games"), {QLatin1String("1000"), QLatin1String("4000")}},
        {QLatin1String("movies"), {QLatin1String("2000")}},
        {QLatin1String("music"), {QLatin1String("3000")}},
        {QLatin1String("software"), {QLatin1String("4000")}},
        {QLatin1String("tv"), {QLatin1String("5000")}}
    };

    const QString categoryParam = supportedCategories.value(category).join(QLatin1Char(','));
    QHash<QString, QString> params {{QLatin1String("cat"), categoryParam}};

    const QHash<QString, IndexerInfo> indexers = manager->indexers();
    for (auto it = indexers.cbegin(); it != indexers.cend(); ++it)
    {
        const QString &indexerName = it.key();
        const IndexerInfo &indexerInfo = it.value();
        if (!indexerInfo.enabled)
            continue;

        const IndexerOptions &indexerOptions = indexerInfo.options;
        params[QLatin1String("apikey")] = indexerOptions.apiKey;
        params[QLatin1String("q")] = pattern;

        const QString url = indexerOptions.url + QLatin1String("api?") + makeQueryString(params);
        Net::DownloadHandler *downloadHandler = Net::DownloadManager::instance()->download(Net::DownloadRequest(url));
        connect(downloadHandler, &Net::DownloadHandler::finished, this, [this, indexerName, downloadHandler](const Net::DownloadResult &result)
        {
            m_downloadHandlers.remove(downloadHandler);
            if (result.status == Net::DownloadStatus::Success)
            {
                QMetaObject::invokeMethod(m_parser, [indexerName, parser = m_parser, data = result.data]()
                {
                    parser->parse(indexerName, data);
                });
            }
            else
            {
                --m_numOutstandingRequests;
                if (!isActive())
                    emit searchFinished();
            }
        });
        ++m_numOutstandingRequests;
    }

    m_searchTimeout->setSingleShot(true);
    connect(m_searchTimeout, &QTimer::timeout, this, &SearchHandler::cancelSearch);
    m_searchTimeout->start(180000); // 3 min
}

SearchHandler::~SearchHandler()
{
    m_parsingThread->quit();
    m_parsingThread->wait();
}

bool SearchHandler::isActive() const
{
    return (m_numOutstandingRequests > 0);
}

void SearchHandler::cancelSearch()
{
    if (!isActive())
        return;

    for (Net::DownloadHandler *downloadHandler : asConst(m_downloadHandlers))
    {
        downloadHandler->cancel();
        downloadHandler->disconnect();
    }
    m_downloadHandlers.clear();

    m_parser->disconnect();

    m_parsingThread->quit();
    m_searchTimeout->stop();
    m_searchCancelled = true;
    m_numOutstandingRequests = 0;

    emit searchFinished(true);
}

void SearchHandler::handleParsingFinished(const TorznabRSSParsingResult &result)
{
    if (!result.items.isEmpty())
    {
        m_results.append(result.items);
        emit newSearchResults(result.items);
    }

    --m_numOutstandingRequests;
    if (!isActive())
        emit searchFinished();
}

SearchEngine *SearchHandler::manager() const
{
    return m_manager;
}

QList<SearchResult> SearchHandler::results() const
{
    return m_results;
}

QString SearchHandler::pattern() const
{
    return m_pattern;
}
