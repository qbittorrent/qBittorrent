/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2021-2023  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2010  Christian Kandeler, Christophe Dumez <chris@qbittorrent.org>
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

#include <QHash>

#include "base/bittorrent/addtorrentparams.h"
#include "base/bittorrent/torrentdescriptor.h"
#include "base/path.h"
#include "base/utils/thread.h"

/*
 * Watches the configured directories for new .torrent files in order
 * to add torrents to BitTorrent session. Supports Network File System
 * watching (NFS, CIFS) on Linux and Mac OS.
 */
class TorrentFilesWatcher final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TorrentFilesWatcher)

public:
    struct WatchedFolderOptions
    {
        BitTorrent::AddTorrentParams addTorrentParams;
        bool recursive = false;
    };

    static void initInstance();
    static void freeInstance();
    static TorrentFilesWatcher *instance();

    QHash<Path, WatchedFolderOptions> folders() const;
    void setWatchedFolder(const Path &path, const WatchedFolderOptions &options);
    void removeWatchedFolder(const Path &path);

signals:
    void watchedFolderSet(const Path &path, const WatchedFolderOptions &options);
    void watchedFolderRemoved(const Path &path);

private slots:
    void onTorrentFound(const BitTorrent::TorrentDescriptor &torrentDescr, const BitTorrent::AddTorrentParams &addTorrentParams);

private:
    explicit TorrentFilesWatcher(QObject *parent = nullptr);

    void load();
    void loadLegacy();
    void store() const;

    void doSetWatchedFolder(const Path &path, const WatchedFolderOptions &options);

    static TorrentFilesWatcher *m_instance;

    QHash<Path, WatchedFolderOptions> m_watchedFolders;

    Utils::Thread::UniquePtr m_ioThread;

    class Worker;
    Worker *m_asyncWorker = nullptr;
};
