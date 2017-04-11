/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2017  Michael Ziminsky
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

#include "rssfeedlistwidgetitem.h"

#include <QDebug>
#include <QHeaderView>

#include "base/rss/rssfile.h"
#include "base/rss/rssfolder.h"
#include "rssfeedlistwidget.h"

RssFeedListWidgetItem::RssFeedListWidgetItem(FilePtr file, RssFeedListWidget *view, ItemType type)
    : QTreeWidgetItem(view, type)
    , m_file(file)
{
    Q_ASSERT(m_file);

    if (const FolderPtr folder = m_file.dynamicCast<Rss::Folder>()) {
        for (FilePtr child : folder->getContent())
            addChild(new RssFeedListWidgetItem(child));
    }
    else { // Feed
        connect(m_file.data(), SIGNAL(iconChanged()), SLOT(refreshIcon()));
    }

    setName(m_file->displayName());
    connect(m_file.data(), SIGNAL(nameChanged(QString)), SLOT(setName(QString)));

    setUnread(m_file->unreadCount());
    connect(m_file.data(), SIGNAL(unreadCountChanged(int)), SLOT(setUnread(int)));

    setTotal(m_file->count());
    connect(m_file.data(), SIGNAL(countChanged(int)), SLOT(setTotal(int)));

    refreshIcon();
}

RssFeedListWidgetItem::RssFeedListWidgetItem(FilePtr file, RssFeedListWidget *view)
    : RssFeedListWidgetItem(file, view, file.dynamicCast<Rss::Folder>() ? Folder : Feed)
{}

RssFeedListWidgetItem::~RssFeedListWidgetItem() {}

void RssFeedListWidgetItem::setName(const QString &name)
{
    setText(0, name);
}

void RssFeedListWidgetItem::setUnread(int unread)
{
    setData(1, Qt::DisplayRole, unread > -1 ? unread : m_file->unreadCount());
}

void RssFeedListWidgetItem::setTotal(int total)
{
    setData(2, Qt::DisplayRole, total > -1 ? total : m_file->count());
}

void RssFeedListWidgetItem::refreshIcon()
{
    setIcon(0, QIcon(m_file->iconPath()));
}

Qt::SortOrder RssFeedListWidgetItem::sortOrder() const
{
    return treeWidget()->header()->sortIndicatorOrder();
}

FilePtr RssFeedListWidgetItem::rssFile() const
{
    return m_file;
}

bool RssFeedListWidgetItem::operator <(const QTreeWidgetItem &other) const
{
    // Sticky item always first
    if (type() == Sticky)
        return sortOrder() == Qt::AscendingOrder;
    if (other.type() == Sticky)
        return sortOrder() == Qt::DescendingOrder;

    // Folders before feeds
    if (type() != other.type())
        return (sortOrder() == Qt::AscendingOrder ? type() < other.type() : other.type() < type());

    return QTreeWidgetItem::operator <(other);
}
