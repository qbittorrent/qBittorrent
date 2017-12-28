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

#include "searchenginewidget.h"

SearchEngineWidget::SearchEngineWidget()
{
    connect(m_searchProcess, &QProcess::readyReadStandardOutput, this, &SearchEngineWidget::readSearchOutput);
}

// search QProcess return output as soon as it gets new
// stuff to read. We split it into lines and parse each
// line to SearchResult calling parseSearchResult().
void SearchEngineWidget::readSearchOutput()
{
    QByteArray output = m_searchProcess->readAllStandardOutput();
    output.replace("\r", "");
    QList<QByteArray> lines = output.split('\n');
    if (!m_searchResultLineTruncated.isEmpty())
        lines.prepend(m_searchResultLineTruncated + lines.takeFirst());
    m_searchResultLineTruncated = lines.takeLast().trimmed();

    QList<SearchResult> searchResultList;
    foreach (const QByteArray &line, lines) {
        SearchResult searchResult;
        if (parseSearchResult(QString::fromUtf8(line), searchResult))
            searchResultList << searchResult;
    }

    if (!searchResultList.isEmpty())
        emit newSearchResults(searchResultList);
}
