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

#include "torrentcontentmodelfolder.h"

#include <QVariant>

#include "base/global.h"

TorrentContentModelFolder::TorrentContentModelFolder(const QString &name, TorrentContentModelFolder *parent)
    : TorrentContentModelItem(parent)
{
    Q_ASSERT(parent);
    m_name = name;
}

TorrentContentModelFolder::TorrentContentModelFolder(const QList<QString> &data)
    : TorrentContentModelItem(nullptr)
{
    Q_ASSERT(data.size() == NB_COL);
    m_itemData = data;
}

TorrentContentModelFolder::~TorrentContentModelFolder()
{
    qDeleteAll(m_childItems);
}

TorrentContentModelItem::ItemType TorrentContentModelFolder::itemType() const
{
    return FolderType;
}

void TorrentContentModelFolder::deleteAllChildren()
{
    Q_ASSERT(isRootItem());
    qDeleteAll(m_childItems);
    m_childItems.clear();
}

const QList<TorrentContentModelItem *> &TorrentContentModelFolder::children() const
{
    return m_childItems;
}

void TorrentContentModelFolder::appendChild(TorrentContentModelItem *item)
{
    Q_ASSERT(item);
    m_childItems.append(item);
    // Update own size
    if (item->itemType() == FileType)
        increaseSize(item->size());
}

TorrentContentModelItem *TorrentContentModelFolder::child(int row) const
{
    return m_childItems.value(row, nullptr);
}
int TorrentContentModelFolder::childCount() const
{
    return m_childItems.count();
}

// Only non-root folders use this function
void TorrentContentModelFolder::updatePriority()
{
    if (isRootItem())
        return;

    Q_ASSERT(!m_childItems.isEmpty());

    // If all children have the same priority
    // then the folder should have the same
    // priority
    const BitTorrent::DownloadPriority prio = m_childItems.constFirst()->priority();
    for (int i = 1; i < m_childItems.size(); ++i)
    {
        if (m_childItems.at(i)->priority() != prio)
        {
            setPriority(BitTorrent::DownloadPriority::Mixed);
            return;
        }
    }
    // All child items have the same priority
    // Update own if necessary
    setPriority(prio);
}

void TorrentContentModelFolder::setPriority(BitTorrent::DownloadPriority newPriority, bool updateParent)
{
    if (m_priority == newPriority)
        return;

    m_priority = newPriority;

    // Update parent priority
    if (updateParent)
        m_parentItem->updatePriority();

    // Update children
    if (m_priority != BitTorrent::DownloadPriority::Mixed)
    {
        for (TorrentContentModelItem *child : asConst(m_childItems))
            child->setPriority(m_priority, false);
    }
}

void TorrentContentModelFolder::recalculateProgress()
{
    qreal tProgress = 0;
    qulonglong tSize = 0;
    qulonglong tRemaining = 0;
    for (TorrentContentModelItem *child : asConst(m_childItems))
    {
        if (child->priority() == BitTorrent::DownloadPriority::Ignored)
            continue;

        if (child->itemType() == FolderType)
            static_cast<TorrentContentModelFolder *>(child)->recalculateProgress();
        tProgress += child->progress() * child->size();
        tSize += child->size();
        tRemaining += child->remaining();
    }

    if (!isRootItem())
    {
        if (tSize > 0)
        {
            m_progress = tProgress / tSize;
            m_remaining = tRemaining;
        }
        else
        {
            m_progress = 1.0;
            m_remaining = 0;
        }

        Q_ASSERT(m_progress <= 1.);
    }
}

void TorrentContentModelFolder::recalculateAvailability()
{
    qreal tAvailability = 0;
    qulonglong tSize = 0;
    bool foundAnyData = false;
    for (TorrentContentModelItem *child : asConst(m_childItems))
    {
        if (child->priority() == BitTorrent::DownloadPriority::Ignored)
            continue;

        if (child->itemType() == FolderType)
            static_cast<TorrentContentModelFolder*>(child)->recalculateAvailability();
        const qreal childAvailability = child->availability();
        if (childAvailability >= 0)
        { // -1 means "no data"
            tAvailability += childAvailability * child->size();
            foundAnyData = true;
        }
        tSize += child->size();
    }

    if (!isRootItem() && (tSize > 0) && foundAnyData)
    {
        m_availability = tAvailability / tSize;
        Q_ASSERT(m_availability <= 1.);
    }
    else
    {
        m_availability = -1.;
    }
}

void TorrentContentModelFolder::increaseSize(qulonglong delta)
{
    if (isRootItem())
        return;

    m_size += delta;
    m_parentItem->increaseSize(delta);
}
