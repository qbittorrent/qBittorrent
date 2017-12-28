/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015  Vladimir Golovnev <glassez@yandex.ru>
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

#include <QProcess>

#include "websearchengine.h"

WebSearchEngine::WebSearchEngine()
{
    connect(m_searchProcess, &QProcess::readyReadStandardOutput, this, &WebSearchEngine::readSearchOutput);
    connect(this, &WebSearchEngine::checkForUpdatesFinished, this, &WebSearchEngine::updateAllPlugins);
}

bool WebSearchEngine::startSearch(const QString &pattern, const QString &category, const QStringList &usedPlugins)
{
    m_stdoutQueue.clear();
    return SearchEngine::startSearch(pattern, category, usedPlugins);
}

void WebSearchEngine::cancelSearch()
{
    m_stdoutQueue.clear();
    SearchEngine::cancelSearch();
}

int WebSearchEngine::getQueueSize() const
{
    QMutexLocker lock(&m_stdoutQueueMutex);
    return m_stdoutQueue.size();
}

void WebSearchEngine::readSearchOutput()
{
    QByteArray output = m_searchProcess->readAllStandardOutput();
    output.replace("\r", "");

    QMutexLocker lock(&m_stdoutQueueMutex);
    foreach (const QByteArray &line, output.split('\n')) {
        SearchResult searchResult;
        if (parseSearchResult(QString::fromUtf8(line), searchResult))
            m_stdoutQueue.enqueue(searchResult);
    }
}

QList<SearchResult> WebSearchEngine::readBufferedSearchOutput()
{
    QMutexLocker lock(&m_stdoutQueueMutex);
    const int size = m_stdoutQueue.size();
    QList<SearchResult> searchResultList;

    // limit to 500 search results
    for (int i = 0; (i < size) && (i < 500); i++)
        searchResultList << m_stdoutQueue.dequeue();

    return searchResultList;
}

void WebSearchEngine::updateAllPlugins(const QHash<QString, PluginVersion> &updateInfo)
{
    if (updateInfo.isEmpty()) {
        qDebug() << "All your plugins are already up to date.";
        return;
    }

    foreach (const QString &pluginName, updateInfo.keys()) {
        qDebug() << "Updating plugin" << pluginName;
        updatePlugin(pluginName);
    }
}

void WebSearchEngine::clearQueue()
{
    QMutexLocker lock(&m_stdoutQueueMutex);
    m_stdoutQueue.clear();
}
