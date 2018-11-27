/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015, 2018  Vladimir Golovnev <glassez@yandex.ru>
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

#include <QProcess>
#include <QTimer>

#include "../global.h"
#include "../utils/foreignapps.h"
#include "../utils/fs.h"
#include "searchpluginmanager.h"

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
        NB_PLUGIN_COLUMNS
    };
}

SearchHandler::SearchHandler(const QString &pattern, const QString &category, const QStringList &usedPlugins, SearchPluginManager *manager)
    : QObject {manager}
    , m_pattern {pattern}
    , m_category {category}
    , m_usedPlugins {usedPlugins}
    , m_manager {manager}
    , m_searchProcess {new QProcess {this}}
    , m_searchTimeout {new QTimer {this}}
{
    // Load environment variables (proxy)
    m_searchProcess->setEnvironment(QProcess::systemEnvironment());

    const QStringList params {
        Utils::Fs::toNativePath(m_manager->engineLocation() + "/nova2.py"),
        m_usedPlugins.join(','),
        m_category
    };

    // Launch search
    m_searchProcess->setProgram(Utils::ForeignApps::pythonInfo().executableName);
    m_searchProcess->setArguments(params + m_pattern.split(' '));

#if (QT_VERSION >= QT_VERSION_CHECK(5, 6, 0))
    connect(m_searchProcess, &QProcess::errorOccurred, this, &SearchHandler::processFailed);
#else
    connect(m_searchProcess, static_cast<void (QProcess::*)(QProcess::ProcessError)>(&QProcess::error)
            , this, &SearchHandler::processFailed);
#endif
    connect(m_searchProcess, &QProcess::readyReadStandardOutput, this, &SearchHandler::readSearchOutput);
    connect(m_searchProcess, static_cast<void (QProcess::*)(int)>(&QProcess::finished)
            , this, &SearchHandler::processFinished);

    m_searchTimeout->setSingleShot(true);
    connect(m_searchTimeout, &QTimer::timeout, this, &SearchHandler::cancelSearch);
    m_searchTimeout->start(180000); // 3 min

    // deferred start allows clients to handle starting-related signals
    QTimer::singleShot(0, this, [this]() { m_searchProcess->start(QIODevice::ReadOnly); });
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
void SearchHandler::processFinished(int exitcode)
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
    QByteArray output = m_searchProcess->readAllStandardOutput();
    output.replace("\r", "");
    QList<QByteArray> lines = output.split('\n');
    if (!m_searchResultLineTruncated.isEmpty())
        lines.prepend(m_searchResultLineTruncated + lines.takeFirst());
    m_searchResultLineTruncated = lines.takeLast().trimmed();

    QList<SearchResult> searchResultList;
    for (const QByteArray &line : asConst(lines)) {
        SearchResult searchResult;
        if (parseSearchResult(QString::fromUtf8(line), searchResult))
            searchResultList << searchResult;
    }

    if (!searchResultList.isEmpty()) {
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
// Line is in the following form:
// file url | file name | file size | nb seeds | nb leechers | Search engine url
bool SearchHandler::parseSearchResult(const QString &line, SearchResult &searchResult)
{
    const QStringList parts = line.split('|');
    const int nbFields = parts.size();
    if (nbFields < (NB_PLUGIN_COLUMNS - 1)) return false; // -1 because desc_link is optional

    searchResult = SearchResult();
    searchResult.fileUrl = parts.at(PL_DL_LINK).trimmed(); // download URL
    searchResult.fileName = parts.at(PL_NAME).trimmed(); // Name
    searchResult.fileSize = parts.at(PL_SIZE).trimmed().toLongLong(); // Size
    bool ok = false;
    searchResult.nbSeeders = parts.at(PL_SEEDS).trimmed().toLongLong(&ok); // Seeders
    if (!ok || (searchResult.nbSeeders < 0))
        searchResult.nbSeeders = -1;
    searchResult.nbLeechers = parts.at(PL_LEECHS).trimmed().toLongLong(&ok); // Leechers
    if (!ok || (searchResult.nbLeechers < 0))
        searchResult.nbLeechers = -1;
    searchResult.siteUrl = parts.at(PL_ENGINE_URL).trimmed(); // Search site URL
    if (nbFields == NB_PLUGIN_COLUMNS)
        searchResult.descrLink = parts.at(PL_DESC_LINK).trimmed(); // Description Link

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
