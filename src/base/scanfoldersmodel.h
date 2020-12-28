/*
 * Bittorrent Client using Qt and libtorrent.
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

#include <QAbstractListModel>
#include <QList>
#include <QtContainerFwd>

class FileSystemWatcher;

class ScanFoldersModel final : public QAbstractListModel
{
    Q_OBJECT
    Q_DISABLE_COPY(ScanFoldersModel)

public:
    enum PathStatus
    {
        Ok,
        DoesNotExist,
        CannotRead,
        CannotWrite,
        AlreadyInList
    };

    enum Column
    {
        WATCH,
        DOWNLOAD,
        NB_COLUMNS
    };

    enum PathType
    {
        DOWNLOAD_IN_WATCH_FOLDER,
        DEFAULT_LOCATION,
        CUSTOM_LOCATION
    };

    static void initInstance();
    static void freeInstance();
    static ScanFoldersModel *instance();

    static QString pathTypeDisplayName(PathType type);

    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

    // TODO: removePaths(); singular version becomes private helper functions;
    // also: remove functions should take modelindexes
    PathStatus addPath(const QString &watchPath, const PathType &downloadType, const QString &downloadPath, bool addToFSWatcher = true);
    PathStatus updatePath(const QString &watchPath, const PathType &downloadType, const QString &downloadPath);
    // PRECONDITION: The paths must have been added with addPath() first.
    void addToFSWatcher(const QStringList &watchPaths);
    void removePath(int row, bool removeFromFSWatcher = true);
    bool removePath(const QString &path, bool removeFromFSWatcher = true);
    void removeFromFSWatcher(const QStringList &watchPaths);

    void makePersistent();

public slots:
    void configure();

private slots:
    void addTorrentsToSession(const QStringList &pathList);

private:
    explicit ScanFoldersModel(QObject *parent = nullptr);
    ~ScanFoldersModel();

    virtual bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    bool downloadInWatchFolder(const QString &filePath) const;
    bool downloadInDefaultFolder(const QString &filePath) const;
    QString downloadPathTorrentFolder(const QString &filePath) const;
    int findPathData(const QString &path) const;

    static ScanFoldersModel *m_instance;
    struct PathData;

    QList<PathData*> m_pathList;
    FileSystemWatcher *m_fsWatcher;
};
