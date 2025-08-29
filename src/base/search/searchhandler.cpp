/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015-2025  Vladimir Golovnev <glassez@yandex.ru>
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

#include <chrono>

#include <QJsonParseError>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QList>
#include <QMetaObject>
#include <QProcess>
#include <QTimer>

#include "base/global.h"
#include "base/path.h"
#include "base/utils/bytearray.h"
#include "base/utils/foreignapps.h"
#include "base/utils/fs.h"
#include "searchpluginmanager.h"

using namespace std::chrono_literals;

namespace
{
    enum SearchResultColumn
    {
        PL_DL_LINK,
        PL_NAME,
        PL_SIZE,
        PL_SEEDS,
        PL_LEECHS,
        PL_ENGINE_URL,
        PL_DESC_LINK,
        PL_PUB_DATE,
        NB_PLUGIN_COLUMNS
    };
}

SearchHandler::SearchHandler(const QString &pattern, const QString &category, const QStringList &usedPlugins, SearchPluginManager *manager)
    : QObject(manager)
    , m_pattern {pattern}
    , m_category {category}
    , m_usedPlugins {usedPlugins}
    , m_manager {manager}
    , m_searchProcess {new QProcess(this)}
    , m_searchTimeout {new QTimer(this)}
{
    // Load environment variables (proxy)
    m_searchProcess->setProcessEnvironment(m_manager->proxyEnvironment());
    m_searchProcess->setProgram(Utils::ForeignApps::pythonInfo().executablePath.data());
#ifdef Q_OS_UNIX
    m_searchProcess->setUnixProcessParameters(QProcess::UnixProcessFlag::CloseFileDescriptors);
#endif

    const QStringList params
    {
        Utils::ForeignApps::PYTHON_ISOLATE_MODE_FLAG,
        (SearchPluginManager::engineLocation() / Path(u"nova2.py"_s)).toString(),
        m_usedPlugins.join(u','),
        m_category
    };
    m_searchProcess->setArguments(params + m_pattern.split(u' '));

    connect(m_searchProcess, &QProcess::errorOccurred, this, &SearchHandler::processFailed);
    connect(m_searchProcess, &QProcess::readyReadStandardOutput, this, &SearchHandler::readSearchOutput);
    connect(m_searchProcess, qOverload<int, QProcess::ExitStatus>(&QProcess::finished)
            , this, &SearchHandler::processFinished);

    m_searchTimeout->setSingleShot(true);
    connect(m_searchTimeout, &QTimer::timeout, this, &SearchHandler::cancelSearch);
    m_searchTimeout->start(3min);

    // Launch search
    // deferred start allows clients to handle starting-related signals
    QMetaObject::invokeMethod(this, [this]() { m_searchProcess->start(QIODevice::ReadOnly); }
        , Qt::QueuedConnection);
}

bool SearchHandler::isActive() const
{
    return (m_searchProcess->state() != QProcess::NotRunning);
}

void SearchHandler::cancelSearch()
{
    if ((m_searchProcess->state() == QProcess::NotRunning) || m_searchCancelled)
        return;

#ifdef Q_OS_WIN
    m_searchProcess->kill();
#else
    m_searchProcess->terminate();
#endif
    m_searchCancelled = true;
    m_searchTimeout->stop();
}

// Slot called when QProcess is Finished
// QProcess can be finished for 3 reasons:
// Error | Stopped by user | Finished normally
void SearchHandler::processFinished(const int exitcode)
{
    m_searchTimeout->stop();

    if (m_searchCancelled)
        emit searchFinished(true);
    else if ((m_searchProcess->exitStatus() == QProcess::NormalExit) && (exitcode == 0))
        emit searchFinished(false);
    else
        emit searchFailed();
}

// search QProcess return output as soon as it gets new
// stuff to read. We split it into lines and parse each
// line to SearchResult calling parseSearchResult().
void SearchHandler::readSearchOutput()
{
    const QByteArray output = m_searchResultLineTruncated + m_searchProcess->readAllStandardOutput();
    QList<QByteArrayView> lines = Utils::ByteArray::splitToViews(output, "\n", Qt::KeepEmptyParts);

    m_searchResultLineTruncated = lines.takeLast().trimmed().toByteArray();

    QList<SearchResult> searchResultList;
    searchResultList.reserve(lines.size());

    for (const QByteArrayView &line : asConst(lines))
    {
        if (SearchResult searchResult; parseSearchResult(line, searchResult))
            searchResultList.append(std::move(searchResult));
    }

    if (!searchResultList.isEmpty())
    {
        m_results.append(searchResultList);
        emit newSearchResults(searchResultList);
    }
}

void SearchHandler::processFailed()
{
    if (!m_searchCancelled)
        emit searchFailed();
}

// Parse one line of search results list
bool SearchHandler::parseSearchResult(const QByteArrayView line, SearchResult &searchResult)
{
    const auto jsonDoc = QJsonDocument::fromJson(line.toByteArray());
    if (jsonDoc.isNull() || !jsonDoc.isObject())
        return false;

    const QJsonObject jsonObj = jsonDoc.object();

    searchResult = SearchResult();
    searchResult.fileUrl = jsonObj[u"link"_s].toString().trimmed(); // download URL
    searchResult.fileName = jsonObj[u"name"_s].toString().trimmed(); // Name
    searchResult.fileSize = jsonObj[u"size"_s].toInteger(); // Size
    searchResult.nbSeeders = jsonObj[u"seeds"_s].toInteger(-1); // Seeders
    searchResult.nbLeechers = jsonObj[u"leech"_s].toInteger(-1); // Leechers
    searchResult.siteUrl = jsonObj[u"engine_url"_s].toString().trimmed(); // Search engine site URL
    searchResult.descrLink = jsonObj[u"desc_link"_s].toString().trimmed(); // Description Link

    if (const qint64 secs = jsonObj[u"pub_date"_s].toInteger(); secs > 0)
        searchResult.pubDate = QDateTime::fromSecsSinceEpoch(secs); // Date

    searchResult.engineName = m_manager->pluginNameBySiteURL(searchResult.siteUrl); // Search engine name

    return true;
}

SearchPluginManager *SearchHandler::manager() const
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
