/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2006-2012  Christophe Dumez <chris@qbittorrent.org>
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

#include "torrentcontentmodelfile.h"

#include "torrentcontentmodelfolder.h"

TorrentContentModelFile::TorrentContentModelFile(const QString &fileName, qulonglong fileSize,
                                                 TorrentContentModelFolder *parent, int fileIndex)
    : TorrentContentModelItem(parent)
    , m_fileIndex(fileIndex)
{
    Q_ASSERT(parent);

    m_name = fileName;
    m_size = fileSize;
}

int TorrentContentModelFile::fileIndex() const
{
    return m_fileIndex;
}

void TorrentContentModelFile::setPriority(BitTorrent::DownloadPriority newPriority, bool updateParent)
{
    Q_ASSERT(newPriority != BitTorrent::DownloadPriority::Mixed);

    if (m_priority == newPriority)
        return;

    m_priority = newPriority;

    // Update parent
    if (updateParent)
        m_parentItem->updatePriority();
}

void TorrentContentModelFile::setProgress(qreal progress)
{
    m_progress = progress;
    m_remaining = static_cast<qulonglong>(m_size * (1.0 - m_progress));
    Q_ASSERT(m_progress <= 1.);
}

void TorrentContentModelFile::setAvailability(const qreal availability)
{
    m_availability = availability;
    Q_ASSERT(m_availability <= 1.);
}

TorrentContentModelItem::ItemType TorrentContentModelFile::itemType() const
{
    return FileType;
}
