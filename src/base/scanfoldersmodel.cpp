/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2010  Christian Kandeler, Christophe Dumez
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

#include <QDir>
#include <QFileInfo>
#include <QString>
#include <QStringList>
#include <QTemporaryFile>
#include <QTextStream>

#include "utils/misc.h"
#include "utils/fs.h"
#include "preferences.h"
#include "logger.h"
#include "filesystemwatcher.h"
#include "bittorrent/session.h"
#include "scanfoldersmodel.h"

struct ScanFoldersModel::PathData
{
    PathData(const QString &watchPath, const PathType &type, const QString &downloadPath)
        : watchPath(watchPath)
        , downloadType(type)
        , downloadPath(downloadPath)
    {
        if (this->downloadPath.isEmpty() && downloadType == CUSTOM_LOCATION)
            downloadType = DEFAULT_LOCATION;
    }

    QString watchPath;
    PathType downloadType;
    QString downloadPath; // valid for CUSTOM_LOCATION
};

ScanFoldersModel *ScanFoldersModel::m_instance = 0;

bool ScanFoldersModel::initInstance(QObject *parent)
{
    if (!m_instance) {
        m_instance = new ScanFoldersModel(parent);
        return true;
    }

    return false;
}

void ScanFoldersModel::freeInstance()
{
    if (m_instance) {
        delete m_instance;
        m_instance = 0;
    }
}

ScanFoldersModel *ScanFoldersModel::instance()
{
    return m_instance;
}

ScanFoldersModel::ScanFoldersModel(QObject *parent)
    : QAbstractListModel(parent)
    , m_fsWatcher(0)
{
    configure();
    connect(Preferences::instance(), SIGNAL(changed()), SLOT(configure()));
}

ScanFoldersModel::~ScanFoldersModel()
{
    qDeleteAll(m_pathList);
}

int ScanFoldersModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_pathList.count();
}

int ScanFoldersModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return NB_COLUMNS;
}

QVariant ScanFoldersModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || (index.row() >= rowCount()))
        return QVariant();

    const PathData *pathData = m_pathList.at(index.row());
    QVariant value;

    switch (index.column()) {
    case WATCH:
        if (role == Qt::DisplayRole)
            value = Utils::Fs::toNativePath(pathData->watchPath);
        break;
    case DOWNLOAD:
        if (role == Qt::UserRole) {
            value = pathData->downloadType;
        }
        else if (role == Qt::DisplayRole) {
            switch (pathData->downloadType) {
            case DOWNLOAD_IN_WATCH_FOLDER:
                value = tr("Watch Folder");
                break;
            case DEFAULT_LOCATION:
                value = tr("Default Folder");
                break;
            case CUSTOM_LOCATION:
                value = pathData->downloadPath;
                break;
            }
        }
        break;
    }

    return value;
}

QVariant ScanFoldersModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if ((orientation != Qt::Horizontal) || (role != Qt::DisplayRole) || (section < 0) || (section >= columnCount()))
        return QVariant();

    QVariant title;

    switch (section) {
    case WATCH:
        title = tr("Watched Folder");
        break;
    case DOWNLOAD:
        title = tr("Save Files to");
        break;
    }

    return title;
}

Qt::ItemFlags ScanFoldersModel::flags(const QModelIndex &index) const
{
    if (!index.isValid() || (index.row() >= rowCount()))
        return QAbstractListModel::flags(index);

    Qt::ItemFlags flags;

    switch (index.column()) {
    case WATCH:
        flags = QAbstractListModel::flags(index);
        break;
    case DOWNLOAD:
        flags = QAbstractListModel::flags(index) | Qt::ItemIsEditable;
        break;
    }

    return flags;
}

bool ScanFoldersModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid() || (index.row() >= rowCount()) || (index.column() >= columnCount())
        || (index.column() != DOWNLOAD))
        return false;

    if (role == Qt::UserRole) {
        PathType type = static_cast<PathType>(value.toInt());
        if (type == CUSTOM_LOCATION)
            return false;

        m_pathList[index.row()]->downloadType = type;
        m_pathList[index.row()]->downloadPath.clear();
        emit dataChanged(index, index);
    }
    else if (role == Qt::DisplayRole) {
        QString path = value.toString();
        if (path.isEmpty()) // means we didn't pass CUSTOM_LOCATION type
            return false;

        m_pathList[index.row()]->downloadType = CUSTOM_LOCATION;
        m_pathList[index.row()]->downloadPath = Utils::Fs::toNativePath(path);
        emit dataChanged(index, index);
    }
    else {
        return false;
    }

    return true;
}

ScanFoldersModel::PathStatus ScanFoldersModel::addPath(const QString &watchPath, const PathType &downloadType, const QString &downloadPath, bool addToFSWatcher)
{
    QDir watchDir(watchPath);
    if (!watchDir.exists()) return DoesNotExist;
    if (!watchDir.isReadable()) return CannotRead;

    const QString &canonicalWatchPath = watchDir.canonicalPath();
    if (findPathData(canonicalWatchPath) != -1) return AlreadyInList;

    QDir downloadDir(downloadPath);
    const QString &canonicalDownloadPath = downloadDir.canonicalPath();

    if (!m_fsWatcher) {
        m_fsWatcher = new FileSystemWatcher(this);
        connect(m_fsWatcher, SIGNAL(torrentsAdded(const QStringList &)), this, SLOT(addTorrentsToSession(const QStringList  &)));
    }

    beginInsertRows(QModelIndex(), rowCount(), rowCount());
    m_pathList << new PathData(Utils::Fs::toNativePath(canonicalWatchPath), downloadType, Utils::Fs::toNativePath(canonicalDownloadPath));
    endInsertRows();

    // Start scanning
    if (addToFSWatcher)
        m_fsWatcher->addPath(canonicalWatchPath);
    return Ok;
}

ScanFoldersModel::PathStatus ScanFoldersModel::updatePath(const QString &watchPath, const PathType& downloadType, const QString &downloadPath)
{
    QDir watchDir(watchPath);
    const QString &canonicalWatchPath = watchDir.canonicalPath();
    int row = findPathData(canonicalWatchPath);
    if (row == -1) return DoesNotExist;

    QDir downloadDir(downloadPath);
    const QString &canonicalDownloadPath = downloadDir.canonicalPath();

    m_pathList.at(row)->downloadType = downloadType;
    m_pathList.at(row)->downloadPath = Utils::Fs::toNativePath(canonicalDownloadPath);

    return Ok;
}

void ScanFoldersModel::addToFSWatcher(const QStringList &watchPaths)
{
    if (!m_fsWatcher)
        return; // addPath() wasn't called before this

    foreach (const QString &path, watchPaths) {
        QDir watchDir(path);
        const QString &canonicalWatchPath = watchDir.canonicalPath();
        m_fsWatcher->addPath(canonicalWatchPath);
    }
}

void ScanFoldersModel::removePath(int row, bool removeFromFSWatcher)
{
    Q_ASSERT((row >= 0) && (row < rowCount()));
    beginRemoveRows(QModelIndex(), row, row);
    if (removeFromFSWatcher)
        m_fsWatcher->removePath(m_pathList.at(row)->watchPath);
    delete m_pathList.takeAt(row);
    endRemoveRows();
}

bool ScanFoldersModel::removePath(const QString &path, bool removeFromFSWatcher)
{
    const int row = findPathData(path);
    if (row == -1) return false;

    removePath(row, removeFromFSWatcher);
    return true;
}

void ScanFoldersModel::removeFromFSWatcher(const QStringList &watchPaths)
{
    foreach (const QString &path, watchPaths)
        m_fsWatcher->removePath(path);
}

bool ScanFoldersModel::downloadInWatchFolder(const QString &filePath) const
{
    const int row = findPathData(QFileInfo(filePath).dir().path());
    Q_ASSERT(row != -1);
    PathData *data = m_pathList.at(row);
    return (data->downloadType == DOWNLOAD_IN_WATCH_FOLDER);
}

bool ScanFoldersModel::downloadInDefaultFolder(const QString &filePath) const
{
    const int row = findPathData(QFileInfo(filePath).dir().path());
    Q_ASSERT(row != -1);
    PathData *data = m_pathList.at(row);
    return (data->downloadType == DEFAULT_LOCATION);
}

QString ScanFoldersModel::downloadPathTorrentFolder(const QString &filePath) const
{
    const int row = findPathData(QFileInfo(filePath).dir().path());
    Q_ASSERT(row != -1);
    PathData *data = m_pathList.at(row);
    if (data->downloadType == CUSTOM_LOCATION)
        return data->downloadPath;

    return  QString();
}

int ScanFoldersModel::findPathData(const QString &path) const
{
    for (int i = 0; i < m_pathList.count(); ++i)
        if (m_pathList.at(i)->watchPath == Utils::Fs::toNativePath(path))
            return i;

    return -1;
}

void ScanFoldersModel::makePersistent()
{
    QVariantHash dirs;

    foreach (const PathData *pathData, m_pathList) {
        if (pathData->downloadType == CUSTOM_LOCATION)
            dirs.insert(Utils::Fs::fromNativePath(pathData->watchPath), Utils::Fs::fromNativePath(pathData->downloadPath));
        else
            dirs.insert(Utils::Fs::fromNativePath(pathData->watchPath), pathData->downloadType);
    }

    Preferences::instance()->setScanDirs(dirs);
}

void ScanFoldersModel::configure()
{
    QVariantHash dirs = Preferences::instance()->getScanDirs();

    for (QVariantHash::const_iterator i = dirs.begin(), e = dirs.end(); i != e; ++i) {
        if (i.value().type() == QVariant::Int)
            addPath(i.key(), static_cast<PathType>(i.value().toInt()), QString());
        else
            addPath(i.key(), CUSTOM_LOCATION, i.value().toString());
    }
}

void ScanFoldersModel::addTorrentsToSession(const QStringList &pathList)
{
    foreach (const QString &file, pathList) {
        qDebug("File %s added", qPrintable(file));

        BitTorrent::AddTorrentParams params;
        if (downloadInWatchFolder(file))
            params.savePath = QFileInfo(file).dir().path();
        else if (!downloadInDefaultFolder(file))
            params.savePath = downloadPathTorrentFolder(file);

        if (file.endsWith(".magnet")) {
            QFile f(file);
            if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QTextStream str(&f);
                while (!str.atEnd())
                    BitTorrent::Session::instance()->addTorrent(str.readLine(), params);

                f.close();
                Utils::Fs::forceRemove(file);
            }
            else {
                qDebug("Failed to open magnet file: %s", qPrintable(f.errorString()));
            }
        }
        else {
            BitTorrent::TorrentInfo torrentInfo = BitTorrent::TorrentInfo::loadFromFile(file);
            if (torrentInfo.isValid()) {
                BitTorrent::Session::instance()->addTorrent(torrentInfo, params);
                Utils::Fs::forceRemove(file);
            }
            else {
                qDebug("Ignoring incomplete torrent file: %s", qPrintable(file));
            }
        }
    }
}
