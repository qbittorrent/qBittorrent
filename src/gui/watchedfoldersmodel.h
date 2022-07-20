/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2021  Vladimir Golovnev <glassez@yandex.ru>
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

#include <QtContainerFwd>
#include <QAbstractListModel>
#include <QHash>
#include <QSet>
#include <QStringList>

#include "base/path.h"
#include "base/torrentfileswatcher.h"

class WatchedFoldersModel final : public QAbstractListModel
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(WatchedFoldersModel)

public:
    explicit WatchedFoldersModel(TorrentFilesWatcher *fsWatcher, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;

    void addFolder(const Path &path, const TorrentFilesWatcher::WatchedFolderOptions &options);

    TorrentFilesWatcher::WatchedFolderOptions folderOptions(int row) const;
    void setFolderOptions(int row, const TorrentFilesWatcher::WatchedFolderOptions &options);

    void apply();

private:
    void onFolderSet(const Path &path, const TorrentFilesWatcher::WatchedFolderOptions &options);
    void onFolderRemoved(const Path &path);

    TorrentFilesWatcher *m_fsWatcher = nullptr;
    PathList m_watchedFolders;
    QHash<Path, TorrentFilesWatcher::WatchedFolderOptions> m_watchedFoldersOptions;
    QSet<Path> m_deletedFolders;
};
