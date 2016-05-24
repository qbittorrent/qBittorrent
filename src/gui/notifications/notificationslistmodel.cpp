/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2016 Eugene Shalygin
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
 *
 */

#include "notificationslistmodel.h"
#include <algorithm>

Notifications::NotificationListModel::NotificationListModel(QList<EventDescription> &&items, QObject *parent)
    : QAbstractListModel {parent}
{
    m_items.swap(items);
}

int Notifications::NotificationListModel::rowCount(const QModelIndex & /*parent*/) const
{
    return m_items.count();
}

QVariant Notifications::NotificationListModel::data(const QModelIndex &index, int role) const
{
    const int row = index.row();
    if (( row < 0) || ( row >= m_items.size()) ) return {};

    const EventDescription &item = m_items[row];
    switch (role) {
    case Qt::DisplayRole:
        return item.name();
    case Qt::ToolTipRole:
        return item.description();
    default:
        return {};
    }
}

namespace
{
    struct compareByName
    {
        bool operator()(const Notifications::EventDescription &l, const Notifications::EventDescription &r)
        {
            return l.name().localeAwareCompare(r.name()) < 0;
        }
    };
}

void Notifications::NotificationListModel::addItem(const Notifications::EventDescription &item)
{
    beginResetModel();     // it's small and have only one sorting scheme
    m_items.append(item);
    std::sort(m_items.begin(), m_items.end(), compareByName());
    endResetModel();
}

void Notifications::NotificationListModel::addItems(const QList<Notifications::EventDescription> &items)
{
    beginResetModel();         // it's small and have only one sorting scheme
    m_items.append(items);
    std::sort(m_items.begin(), m_items.end(), compareByName());
    endResetModel();
}

#if 0
bool Notifications::NotificationListModel::removeRows(int row, int count, const QModelIndex & /*parent*/)
{
    beginRemoveRows({}, row, row + count - 1);
    auto iFirst = m_items.begin() + row;
    m_items.erase(iFirst, iFirst + count);
    endRemoveRows();
}

#endif

Notifications::EventDescription Notifications::NotificationListModel::takeAt(int index)
{
    Q_ASSERT(index >= 0);
    Q_ASSERT(index < m_items.size());

    beginRemoveRows({}, index, 1);
    EventDescription res = m_items.takeAt(index);
    endRemoveRows();
    return res;
}

void Notifications::NotificationListModel::clear()
{
    beginResetModel();
    m_items.clear();
    endResetModel();
}

const QList<Notifications::EventDescription> &Notifications::NotificationListModel::items() const
{
    return m_items;
}
