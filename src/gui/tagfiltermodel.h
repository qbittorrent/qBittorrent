/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2017  Tony Gregerson <tony.gregerson@gmail.com>
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

#include <QAbstractListModel>
#include <QtContainerFwd>

class QModelIndex;

class TagModelItem;

namespace BitTorrent
{
    class TorrentHandle;
}

class TagFilterModel final : public QAbstractListModel
{
    Q_OBJECT

public:
    explicit TagFilterModel(QObject *parent = nullptr);
    ~TagFilterModel() override;

    static bool isSpecialItem(const QModelIndex &index);

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    QModelIndex index(int row, int column, const QModelIndex &parent = {}) const override;
    int rowCount(const QModelIndex &parent = {}) const override;

    QModelIndex index(const QString &tag) const;
    QString tag(const QModelIndex &index) const;

private slots:
    void tagAdded(const QString &tag);
    void tagRemoved(const QString &tag);
    void torrentTagAdded(BitTorrent::TorrentHandle *const torrent, const QString &tag);
    void torrentTagRemoved(BitTorrent::TorrentHandle *const, const QString &tag);
    void torrentAdded(BitTorrent::TorrentHandle *const torrent);
    void torrentAboutToBeRemoved(BitTorrent::TorrentHandle *const torrent);

private:
    static QString tagDisplayName(const QString &tag);

    void populate();
    void addToModel(const QString &tag, int count);
    void removeFromModel(int row);
    bool isValidRow(int row) const;
    int findRow(const QString &tag) const;
    TagModelItem *findItem(const QString &tag);
    QVector<TagModelItem *> findItems(const QSet<QString> &tags);
    TagModelItem *allTagsItem();
    TagModelItem *untaggedItem();

    QList<TagModelItem> m_tagItems;  // Index corresponds to its row
};
