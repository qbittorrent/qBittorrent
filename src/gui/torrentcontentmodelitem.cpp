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

#include "torrentcontentmodelitem.h"

#include <QVariant>

#include "base/unicodestrings.h"
#include "base/utils/misc.h"
#include "base/utils/string.h"
#include "torrentcontentmodelfolder.h"

TorrentContentModelItem::TorrentContentModelItem(TorrentContentModelFolder *parent)
    : m_parentItem(parent)
{
}

TorrentContentModelItem::~TorrentContentModelItem() = default;

bool TorrentContentModelItem::isRootItem() const
{
    return !m_parentItem;
}

QString TorrentContentModelItem::name() const
{
    Q_ASSERT(!isRootItem());
    return m_name;
}

void TorrentContentModelItem::setName(const QString &name)
{
    Q_ASSERT(!isRootItem());
    m_name = name;
}

qulonglong TorrentContentModelItem::size() const
{
    Q_ASSERT(!isRootItem());

    return m_size;
}

qreal TorrentContentModelItem::progress() const
{
    Q_ASSERT(!isRootItem());

    return (m_size > 0) ? m_progress : 1;
}

qulonglong TorrentContentModelItem::remaining() const
{
    Q_ASSERT(!isRootItem());
    return (m_priority == BitTorrent::DownloadPriority::Ignored) ? 0 : m_remaining;
}

qreal TorrentContentModelItem::availability() const
{
    Q_ASSERT(!isRootItem());

    return (m_size > 0) ? m_availability : 0;
}

BitTorrent::DownloadPriority TorrentContentModelItem::priority() const
{
    Q_ASSERT(!isRootItem());
    return m_priority;
}

int TorrentContentModelItem::columnCount() const
{
    return NB_COL;
}

QString TorrentContentModelItem::displayData(const int column) const
{
    if (isRootItem())
        return m_itemData.value(column);

    switch (column)
    {
    case COL_NAME:
        return m_name;
    case COL_PRIO:
        switch (m_priority)
        {
        case BitTorrent::DownloadPriority::Mixed:
            return tr("Mixed", "Mixed (priorities");
        case BitTorrent::DownloadPriority::Ignored:
            return tr("Not downloaded");
        case BitTorrent::DownloadPriority::High:
            return tr("High", "High (priority)");
        case BitTorrent::DownloadPriority::Maximum:
            return tr("Maximum", "Maximum (priority)");
        default:
            return tr("Normal", "Normal (priority)");
        }
    case COL_PROGRESS:
        return (m_progress >= 1)
               ? u"100%"_s
               : (Utils::String::fromDouble((m_progress * 100), 1) + u'%');
    case COL_SIZE:
        return Utils::Misc::friendlyUnit(m_size);
    case COL_REMAINING:
        return Utils::Misc::friendlyUnit(remaining());
    case COL_AVAILABILITY:
        {
            const qreal avail = availability();
            if (avail < 0)
                return tr("N/A");

            const QString value = (avail >= 1)
                                  ? u"100"_s
                                  : Utils::String::fromDouble((avail * 100), 1);
            return (value + u'%');
        }
    default:
        Q_UNREACHABLE();
        break;
    }

    return {};
}

QVariant TorrentContentModelItem::underlyingData(const int column) const
{
    if (isRootItem())
        return m_itemData.value(column);

    switch (column)
    {
    case COL_NAME:
        return m_name;
    case COL_PRIO:
        return static_cast<int>(m_priority);
    case COL_PROGRESS:
        return progress() * 100;
    case COL_SIZE:
        return m_size;
    case COL_REMAINING:
        return remaining();
    case COL_AVAILABILITY:
        return availability();
    default:
        Q_UNREACHABLE();
        break;
    }

    return {};
}

int TorrentContentModelItem::row() const
{
    if (m_parentItem)
        return m_parentItem->children().indexOf(const_cast<TorrentContentModelItem *>(this));
    return 0;
}

TorrentContentModelFolder *TorrentContentModelItem::parent() const
{
    return m_parentItem;
}
