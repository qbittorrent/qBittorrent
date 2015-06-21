/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2010  Christophe Dumez <chris@qbittorrent.org>
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
 * Contact : chris@qbittorrent.org
 */

#ifndef TORRENTMODEL_H
#define TORRENTMODEL_H

#include <QAbstractListModel>
#include <QList>

namespace BitTorrent
{
    class InfoHash;
    class TorrentHandle;
}

class TorrentModel : public QAbstractListModel
{
    Q_OBJECT
    Q_DISABLE_COPY(TorrentModel)

public:
    enum Column
    {
        TR_PRIORITY,
        TR_NAME,
        TR_SIZE,
        TR_TOTAL_SIZE,
        TR_PROGRESS,
        TR_STATUS,
        TR_SEEDS,
        TR_PEERS,
        TR_DLSPEED,
        TR_UPSPEED,
        TR_ETA,
        TR_RATIO,
        TR_LABEL,
        TR_ADD_DATE,
        TR_SEED_DATE,
        TR_TRACKER,
        TR_DLLIMIT,
        TR_UPLIMIT,
        TR_AMOUNT_DOWNLOADED,
        TR_AMOUNT_UPLOADED,
        TR_AMOUNT_DOWNLOADED_SESSION,
        TR_AMOUNT_UPLOADED_SESSION,
        TR_AMOUNT_LEFT,
        TR_TIME_ELAPSED,
        TR_SAVE_PATH,
        TR_COMPLETED,
        TR_RATIO_LIMIT,
        TR_SEEN_COMPLETE_DATE,
        TR_LAST_ACTIVITY,

        NB_COLUMNS
    };

    explicit TorrentModel(QObject *parent = 0);

    int rowCount(const QModelIndex& index = QModelIndex()) const;
    int columnCount(const QModelIndex &parent=QModelIndex()) const;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::DisplayRole);
    QVariant headerData(int section, Qt::Orientation orientation, int role) const;
    Qt::ItemFlags flags(const QModelIndex &index) const;

    BitTorrent::TorrentHandle *torrentHandle(const QModelIndex &index) const;

private slots:
    void addTorrent(BitTorrent::TorrentHandle *const torrent);
    void handleTorrentAboutToBeRemoved(BitTorrent::TorrentHandle *const torrent);
    void handleTorrentStatusUpdated(BitTorrent::TorrentHandle *const torrent);

private:
    QList<BitTorrent::TorrentHandle *> m_torrents;
};

#endif // TORRENTMODEL_H
