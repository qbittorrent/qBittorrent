/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015-2023  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2006  Christophe Dumez <chris@qbittorrent.org>
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

#include <memory>

#include <QHash>
#include <QObject>

#include "base/applicationcomponent.h"
#include "base/bittorrent/addtorrentparams.h"
#include "base/torrentfileguard.h"

namespace BitTorrent
{
    class InfoHash;
    class Session;
    class Torrent;
    class TorrentDescriptor;
}

namespace Net
{
    struct DownloadResult;
}

class QString;

class AddTorrentManager : public ApplicationComponent<QObject>
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(AddTorrentManager)

public:
    AddTorrentManager(IApplication *app, BitTorrent::Session *btSession, QObject *parent = nullptr);

    BitTorrent::Session *btSession() const;
    bool addTorrent(const QString &source, const BitTorrent::AddTorrentParams &params = {});

signals:
    void torrentAdded(const QString &source, BitTorrent::Torrent *torrent);
    void addTorrentFailed(const QString &source, const QString &reason);

protected:
    bool addTorrentToSession(const QString &source, const BitTorrent::TorrentDescriptor &torrentDescr
            , const BitTorrent::AddTorrentParams &addTorrentParams);
    void handleAddTorrentFailed(const QString &source, const QString &reason);
    void handleDuplicateTorrent(const QString &source, const BitTorrent::TorrentDescriptor &torrentDescr, BitTorrent::Torrent *existingTorrent);
    void setTorrentFileGuard(const QString &source, std::shared_ptr<TorrentFileGuard> torrentFileGuard);
    std::shared_ptr<TorrentFileGuard> releaseTorrentFileGuard(const QString &source);

private:
    void onDownloadFinished(const Net::DownloadResult &result);
    void onSessionTorrentAdded(BitTorrent::Torrent *torrent);
    void onSessionAddTorrentFailed(const BitTorrent::InfoHash &infoHash, const QString &reason);
    bool processTorrent(const QString &source, const BitTorrent::TorrentDescriptor &torrentDescr
            , const BitTorrent::AddTorrentParams &addTorrentParams);

    BitTorrent::Session *m_btSession = nullptr;
    QHash<QString, BitTorrent::AddTorrentParams> m_downloadedTorrents;
    QHash<BitTorrent::InfoHash, QString> m_sourcesByInfoHash;
    QHash<QString, std::shared_ptr<TorrentFileGuard>> m_guardedTorrentFiles;
};
