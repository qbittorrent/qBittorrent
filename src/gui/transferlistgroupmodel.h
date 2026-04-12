/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2025  AlfEspadero
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

#include <QAbstractItemModel>
#include <QVector>
#include <QHash>

#include "transferlistmodel.h"
#include "base/torrentgroup.h"

// A proxy-like aggregate model that exposes synthetic parent rows for groups.
// Child rows are underlying torrents. Delegate still works on DisplayRole.
class TransferListGroupModel final : public QAbstractItemModel
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TransferListGroupModel)
public:
    explicit TransferListGroupModel(TransferListModel *source, QObject *parent = nullptr);

    QModelIndex index(int row, int column, const QModelIndex &parent) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    int rowCount(const QModelIndex &parent) const override;
    int columnCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

    void rebuild();

    TransferListModel *sourceModel() const
    {
        return m_source;
    }
    BitTorrent::Torrent *torrentHandle(const QModelIndex &index) const; // nullptr for group parents
    QString groupName(const QModelIndex &index) const;
    void setGroupExpanded(const QString &name, bool expanded);

private slots:
    void handleSourceChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QList<int> &roles = {});
    void handleSourceReset();

private:
    struct GroupItem
    {
        QString name;
        QList<BitTorrent::Torrent*> members;
    };

    TransferListModel *m_source;
    QVector<GroupItem> m_groups; // ordered
    QList<BitTorrent::Torrent*> m_ungrouped; // top-level leaves after groups
    QHash<QString, int> m_groupRowByName; // name -> row in m_groups
};
