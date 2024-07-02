/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2018-2023  Vladimir Golovnev <glassez@yandex.ru>
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

#include <QSet>
#include <QVariantMap>

#include "base/bittorrent/infohash.h"
#include "base/tag.h"
#include "apicontroller.h"

namespace BitTorrent
{
    class Torrent;
}

class SyncController : public APIController
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(SyncController)

public:
    using APIController::APIController;

    explicit SyncController(IApplication *app, QObject *parent = nullptr);

public slots:
    void updateFreeDiskSpace(qint64 freeDiskSpace);

private slots:
    void maindataAction();
    void torrentPeersAction();

private:
    void makeMaindataSnapshot();
    QJsonObject generateMaindataSyncData(int id, bool fullUpdate);

    void onCategoryAdded(const QString &categoryName);
    void onCategoryRemoved(const QString &categoryName);
    void onCategoryOptionsChanged(const QString &categoryName);
    void onSubcategoriesSupportChanged();
    void onTagAdded(const Tag &tag);
    void onTagRemoved(const Tag &tag);
    void onTorrentAdded(BitTorrent::Torrent *torrent);
    void onTorrentAboutToBeRemoved(BitTorrent::Torrent *torrent);
    void onTorrentCategoryChanged(BitTorrent::Torrent *torrent, const QString &oldCategory);
    void onTorrentMetadataReceived(BitTorrent::Torrent *torrent);
    void onTorrentStopped(BitTorrent::Torrent *torrent);
    void onTorrentStarted(BitTorrent::Torrent *torrent);
    void onTorrentSavePathChanged(BitTorrent::Torrent *torrent);
    void onTorrentSavingModeChanged(BitTorrent::Torrent *torrent);
    void onTorrentTagAdded(BitTorrent::Torrent *torrent, const Tag &tag);
    void onTorrentTagRemoved(BitTorrent::Torrent *torrent, const Tag &tag);
    void onTorrentsUpdated(const QList<BitTorrent::Torrent *> &torrents);
    void onTorrentTrackersChanged(BitTorrent::Torrent *torrent);

    qint64 m_freeDiskSpace = 0;

    QVariantMap m_lastPeersResponse;
    QVariantMap m_lastAcceptedPeersResponse;

    QHash<QString, QSet<BitTorrent::TorrentID>> m_knownTrackers;

    QSet<QString> m_updatedCategories;
    QSet<QString> m_removedCategories;
    QSet<QString> m_addedTags;
    QSet<QString> m_removedTags;
    QSet<QString> m_updatedTrackers;
    QSet<QString> m_removedTrackers;
    QSet<BitTorrent::TorrentID> m_updatedTorrents;
    QSet<BitTorrent::TorrentID> m_removedTorrents;

    struct MaindataSyncBuf
    {
        QHash<QString, QVariantMap> categories;
        QVariantList tags;
        QHash<QString, QVariantMap> torrents;
        QHash<QString, QStringList> trackers;
        QVariantMap serverState;

        QStringList removedCategories;
        QStringList removedTags;
        QStringList removedTorrents;
        QStringList removedTrackers;
    };

    MaindataSyncBuf m_maindataSnapshot;
    MaindataSyncBuf m_maindataSyncBuf;
    int m_maindataLastSentID = 0;
    int m_maindataAcceptedID = -1;
};
