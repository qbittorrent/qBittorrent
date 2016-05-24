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

#ifndef QBT_NOTIFICATIONSLISTMODEL_H
#define QBT_NOTIFICATIONSLISTMODEL_H

#include <QAbstractListModel>
#include "base/notifications/eventoption.h"
#include <QList>

namespace Notifications
{
    class NotificationListModel: public QAbstractListModel
    {
    public:
        NotificationListModel(QList<EventDescription> &&items, QObject *parent = nullptr);
        int rowCount(const QModelIndex &parent) const override;
        QVariant data(const QModelIndex &index, int role) const override;

        // adds an item to list and re-sorts the list
        void addItem(const EventDescription &item);
        // adds items to list and re-sorts the list
        void addItems(const QList<EventDescription> &items);

        EventDescription takeAt(int index);
        void clear();

        const QList<EventDescription> &items() const;

    private:
        QList<EventDescription> m_items;
    };
}

#endif // QBT_NOTIFICATIONSLISTMODEL_H
