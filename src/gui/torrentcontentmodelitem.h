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

#pragma once

#include <QCoreApplication>
#include <QList>

#include "base/bittorrent/downloadpriority.h"

class QVariant;

class TorrentContentModelFolder;

class TorrentContentModelItem
{
    Q_DECLARE_TR_FUNCTIONS(TorrentContentModelItem)

public:
    enum TreeItemColumns
    {
        COL_NAME,
        COL_SIZE,
        COL_PROGRESS,
        COL_PRIO,
        COL_REMAINING,
        COL_AVAILABILITY,
        NB_COL
    };

    enum ItemType
    {
        FileType,
        FolderType
    };

    explicit TorrentContentModelItem(TorrentContentModelFolder *parent);
    virtual ~TorrentContentModelItem();

    bool isRootItem() const;
    TorrentContentModelFolder *parent() const;
    virtual ItemType itemType() const = 0;

    QString name() const;
    void setName(const QString &name);

    qulonglong size() const;
    qreal progress() const;
    qulonglong remaining() const;

    qreal availability() const;

    BitTorrent::DownloadPriority priority() const;
    virtual void setPriority(BitTorrent::DownloadPriority newPriority, bool updateParent = true) = 0;

    int columnCount() const;
    QString displayData(int column) const;
    QVariant underlyingData(int column) const;
    int row() const;

protected:
    TorrentContentModelFolder *m_parentItem = nullptr;
    // Root item members
    QList<QString> m_itemData;
    // Non-root item members
    QString m_name;
    qulonglong m_size = 0;
    qulonglong m_remaining = 0;
    BitTorrent::DownloadPriority m_priority = BitTorrent::DownloadPriority::Normal;
    qreal m_progress = 0;
    qreal m_availability = -1;
};
