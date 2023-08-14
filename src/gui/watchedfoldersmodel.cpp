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

#include "watchedfoldersmodel.h"

#include <QDir>

#include "base/exceptions.h"
#include "base/global.h"
#include "base/utils/fs.h"

WatchedFoldersModel::WatchedFoldersModel(TorrentFilesWatcher *fsWatcher, QObject *parent)
    : QAbstractListModel {parent}
    , m_fsWatcher {fsWatcher}
    , m_watchedFolders {m_fsWatcher->folders().keys()}
    , m_watchedFoldersOptions {m_fsWatcher->folders()}
{
    connect(m_fsWatcher, &TorrentFilesWatcher::watchedFolderSet, this, &WatchedFoldersModel::onFolderSet);
    connect(m_fsWatcher, &TorrentFilesWatcher::watchedFolderRemoved, this, &WatchedFoldersModel::onFolderRemoved);
}

int WatchedFoldersModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_watchedFolders.count();
}

int WatchedFoldersModel::columnCount([[maybe_unused]] const QModelIndex &parent) const
{
    return 1;
}

QVariant WatchedFoldersModel::data(const QModelIndex &index, const int role) const
{
    if (!index.isValid() || (index.row() >= rowCount()) || (index.column() >= columnCount()))
        return {};

    if (role == Qt::DisplayRole)
        return m_watchedFolders.at(index.row()).toString();

    return {};
}

QVariant WatchedFoldersModel::headerData(const int section, const Qt::Orientation orientation, const int role) const
{
    if ((orientation != Qt::Horizontal) || (role != Qt::DisplayRole)
            || (section < 0) || (section >= columnCount()))
    {
        return {};
    }

    return tr("Watched Folder");
}

bool WatchedFoldersModel::removeRows(const int row, const int count, const QModelIndex &parent)
{
    if (parent.isValid() || (row < 0) || (row >= rowCount())
        || (count <= 0) || ((row + count) > rowCount()))
    {
        return false;
    }

    const int firstRow = row;
    const int lastRow = row + (count - 1);

    beginRemoveRows(parent, firstRow, lastRow);
    for (int i = firstRow; i <= lastRow; ++i)
    {
        const Path folderPath = m_watchedFolders.takeAt(i);
        m_watchedFoldersOptions.remove(folderPath);
        m_deletedFolders.insert(folderPath);
    }
    endRemoveRows();

    return true;
}

void WatchedFoldersModel::addFolder(const Path &path, const TorrentFilesWatcher::WatchedFolderOptions &options)
{
    if (path.isEmpty())
        throw InvalidArgument(tr("Watched folder path cannot be empty."));

    if (path.isRelative())
        throw InvalidArgument(tr("Watched folder path cannot be relative."));

    if (m_watchedFoldersOptions.contains(path))
        throw RuntimeError(tr("Folder '%1' is already in watch list.").arg(path.toString()));

    const QDir watchDir {path.data()};
    if (!watchDir.exists())
        throw RuntimeError(tr("Folder '%1' doesn't exist.").arg(path.toString()));
    if (!watchDir.isReadable())
        throw RuntimeError(tr("Folder '%1' isn't readable.").arg(path.toString()));

    m_deletedFolders.remove(path);

    beginInsertRows(QModelIndex(), rowCount(), rowCount());
    m_watchedFolders.append(path);
    m_watchedFoldersOptions[path] = options;
    endInsertRows();
}

TorrentFilesWatcher::WatchedFolderOptions WatchedFoldersModel::folderOptions(const int row) const
{
    Q_ASSERT((row >= 0) && (row < rowCount()));

    const Path folderPath = m_watchedFolders.at(row);
    return m_watchedFoldersOptions[folderPath];
}

void WatchedFoldersModel::setFolderOptions(const int row, const TorrentFilesWatcher::WatchedFolderOptions &options)
{
    Q_ASSERT((row >= 0) && (row < rowCount()));

    const Path folderPath = m_watchedFolders.at(row);
    m_watchedFoldersOptions[folderPath] = options;
}

void WatchedFoldersModel::apply()
{
    const QSet<Path> deletedFolders {m_deletedFolders};
    // We have to clear `m_deletedFolders` for optimization reason, otherwise
    // it will be cleared one element at a time in `onFolderRemoved()` handler
    m_deletedFolders.clear();
    for (const Path &path : deletedFolders)
        m_fsWatcher->removeWatchedFolder(path);

    for (const Path &path : asConst(m_watchedFolders))
        m_fsWatcher->setWatchedFolder(path, m_watchedFoldersOptions.value(path));
}

void WatchedFoldersModel::onFolderSet(const Path &path, const TorrentFilesWatcher::WatchedFolderOptions &options)
{
    if (!m_watchedFoldersOptions.contains(path))
    {
        m_deletedFolders.remove(path);

        beginInsertRows(QModelIndex(), rowCount(), rowCount());
        m_watchedFolders.append(path);
        m_watchedFoldersOptions[path] = options;
        endInsertRows();
    }
    else
    {
        m_watchedFoldersOptions[path] = options;
    }
}

void WatchedFoldersModel::onFolderRemoved(const Path &path)
{
    const int row = m_watchedFolders.indexOf(path);
    if (row >= 0)
        removeRows(row, 1);

    m_deletedFolders.remove(path);
}
