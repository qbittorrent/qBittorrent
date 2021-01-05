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

#include "scanfoldersmodel.h"

#include <QDir>
#include <QFileInfo>
#include <QStringList>
#include <QTextStream>

#include "bittorrent/session.h"
#include "filesystemwatcher.h"
#include "global.h"
#include "preferences.h"
#include "utils/fs.h"

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

ScanFoldersModel *ScanFoldersModel::m_instance = nullptr;

void ScanFoldersModel::initInstance()
{
    if (!m_instance)
        m_instance = new ScanFoldersModel;
}

void ScanFoldersModel::freeInstance()
{
    delete m_instance;
    m_instance = nullptr;
}

ScanFoldersModel *ScanFoldersModel::instance()
{
    return m_instance;
}

ScanFoldersModel::ScanFoldersModel(QObject *parent)
    : QAbstractListModel(parent)
    , m_fsWatcher(nullptr)
{
    configure();
    connect(Preferences::instance(), &Preferences::changed, this, &ScanFoldersModel::configure);
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
        return {};

    const PathData *pathData = m_pathList.at(index.row());
    QVariant value;

    switch (index.column())
    {
    case WATCH:
        if (role == Qt::DisplayRole)
            value = Utils::Fs::toNativePath(pathData->watchPath);
        break;
    case DOWNLOAD:
        if (role == Qt::UserRole)
        {
            value = pathData->downloadType;
        }
        else if (role == Qt::DisplayRole)
        {
            switch (pathData->downloadType)
            {
            case DOWNLOAD_IN_WATCH_FOLDER:
            case DEFAULT_LOCATION:
                value = pathTypeDisplayName(pathData->downloadType);
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
        return {};

    QVariant title;

    switch (section)
    {
    case WATCH:
        title = tr("Monitored Folder");
        break;
    case DOWNLOAD:
        title = tr("Override Save Location");
        break;
    }

    return title;
}

Qt::ItemFlags ScanFoldersModel::flags(const QModelIndex &index) const
{
    if (!index.isValid() || (index.row() >= rowCount()))
        return QAbstractListModel::flags(index);

    Qt::ItemFlags flags;

    switch (index.column())
    {
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

    if (role == Qt::UserRole)
    {
        const auto type = static_cast<PathType>(value.toInt());
        if (type == CUSTOM_LOCATION)
            return false;

        m_pathList[index.row()]->downloadType = type;
        m_pathList[index.row()]->downloadPath.clear();
        emit dataChanged(index, index);
    }
    else if (role == Qt::DisplayRole)
    {
        const QString path = value.toString();
        if (path.isEmpty()) // means we didn't pass CUSTOM_LOCATION type
            return false;

        m_pathList[index.row()]->downloadType = CUSTOM_LOCATION;
        m_pathList[index.row()]->downloadPath = Utils::Fs::toNativePath(path);
        emit dataChanged(index, index);
    }
    else
    {
        return false;
    }

    return true;
}

ScanFoldersModel::PathStatus ScanFoldersModel::addPath(const QString &watchPath, const PathType &downloadType, const QString &downloadPath, bool addToFSWatcher)
{
    const QDir watchDir(watchPath);
    if (!watchDir.exists()) return DoesNotExist;
    if (!watchDir.isReadable()) return CannotRead;

    const QString canonicalWatchPath = watchDir.canonicalPath();
    if (findPathData(canonicalWatchPath) != -1) return AlreadyInList;

    const QDir downloadDir(downloadPath);
    const QString canonicalDownloadPath = downloadDir.canonicalPath();

    if (!m_fsWatcher)
    {
        m_fsWatcher = new FileSystemWatcher(this);
        connect(m_fsWatcher, &FileSystemWatcher::torrentsAdded, this, &ScanFoldersModel::addTorrentsToSession);
    }

    beginInsertRows(QModelIndex(), rowCount(), rowCount());
    m_pathList << new PathData(Utils::Fs::toNativePath(canonicalWatchPath), downloadType, Utils::Fs::toNativePath(canonicalDownloadPath));
    endInsertRows();

    // Start scanning
    if (addToFSWatcher)
        m_fsWatcher->addPath(canonicalWatchPath);
    return Ok;
}

ScanFoldersModel::PathStatus ScanFoldersModel::updatePath(const QString &watchPath, const PathType &downloadType, const QString &downloadPath)
{
    const QDir watchDir(watchPath);
    const QString canonicalWatchPath = watchDir.canonicalPath();
    const int row = findPathData(canonicalWatchPath);
    if (row == -1) return DoesNotExist;

    const QDir downloadDir(downloadPath);
    const QString canonicalDownloadPath = downloadDir.canonicalPath();

    m_pathList.at(row)->downloadType = downloadType;
    m_pathList.at(row)->downloadPath = Utils::Fs::toNativePath(canonicalDownloadPath);

    return Ok;
}

void ScanFoldersModel::addToFSWatcher(const QStringList &watchPaths)
{
    if (!m_fsWatcher)
        return; // addPath() wasn't called before this

    for (const QString &path : watchPaths)
    {
        const QDir watchDir(path);
        const QString canonicalWatchPath = watchDir.canonicalPath();
        m_fsWatcher->addPath(canonicalWatchPath);
    }
}

void ScanFoldersModel::removePath(const int row, const bool removeFromFSWatcher)
{
    Q_ASSERT((row >= 0) && (row < rowCount()));
    beginRemoveRows(QModelIndex(), row, row);
    if (removeFromFSWatcher)
        m_fsWatcher->removePath(m_pathList.at(row)->watchPath);
    delete m_pathList.takeAt(row);
    endRemoveRows();
}

bool ScanFoldersModel::removePath(const QString &path, const bool removeFromFSWatcher)
{
    const int row = findPathData(path);
    if (row == -1) return false;

    removePath(row, removeFromFSWatcher);
    return true;
}

void ScanFoldersModel::removeFromFSWatcher(const QStringList &watchPaths)
{
    for (const QString &path : watchPaths)
        m_fsWatcher->removePath(path);
}

bool ScanFoldersModel::downloadInWatchFolder(const QString &filePath) const
{
    const int row = findPathData(QFileInfo(filePath).dir().path());
    Q_ASSERT(row != -1);
    const PathData *data = m_pathList.at(row);
    return (data->downloadType == DOWNLOAD_IN_WATCH_FOLDER);
}

bool ScanFoldersModel::downloadInDefaultFolder(const QString &filePath) const
{
    const int row = findPathData(QFileInfo(filePath).dir().path());
    Q_ASSERT(row != -1);
    const PathData *data = m_pathList.at(row);
    return (data->downloadType == DEFAULT_LOCATION);
}

QString ScanFoldersModel::downloadPathTorrentFolder(const QString &filePath) const
{
    const int row = findPathData(QFileInfo(filePath).dir().path());
    Q_ASSERT(row != -1);
    const PathData *data = m_pathList.at(row);
    if (data->downloadType == CUSTOM_LOCATION)
        return data->downloadPath;

    return {};
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

    for (const PathData *pathData : asConst(m_pathList))
    {
        if (pathData->downloadType == CUSTOM_LOCATION)
            dirs.insert(Utils::Fs::toUniformPath(pathData->watchPath), Utils::Fs::toUniformPath(pathData->downloadPath));
        else
            dirs.insert(Utils::Fs::toUniformPath(pathData->watchPath), pathData->downloadType);
    }

    Preferences::instance()->setScanDirs(dirs);
}

void ScanFoldersModel::configure()
{
    const QVariantHash dirs = Preferences::instance()->getScanDirs();

    for (auto i = dirs.cbegin(); i != dirs.cend(); ++i)
    {
        if (i.value().type() == QVariant::Int)
            addPath(i.key(), static_cast<PathType>(i.value().toInt()), QString());
        else
            addPath(i.key(), CUSTOM_LOCATION, i.value().toString());
    }
}

void ScanFoldersModel::addTorrentsToSession(const QStringList &pathList)
{
    for (const QString &file : pathList)
    {
        qDebug("File %s added", qUtf8Printable(file));

        BitTorrent::AddTorrentParams params;
        if (downloadInWatchFolder(file))
        {
            params.savePath = QFileInfo(file).dir().path();
            params.useAutoTMM = false;
        }
        else if (!downloadInDefaultFolder(file))
        {
            params.savePath = downloadPathTorrentFolder(file);
            params.useAutoTMM = false;
        }

        if (file.endsWith(".magnet", Qt::CaseInsensitive))
        {
            QFile f(file);
            if (f.open(QIODevice::ReadOnly | QIODevice::Text))
            {
                QTextStream str(&f);
                while (!str.atEnd())
                    BitTorrent::Session::instance()->addTorrent(str.readLine(), params);

                f.close();
                Utils::Fs::forceRemove(file);
            }
            else
            {
                qDebug("Failed to open magnet file: %s", qUtf8Printable(f.errorString()));
            }
        }
        else
        {
            const BitTorrent::TorrentInfo torrentInfo = BitTorrent::TorrentInfo::loadFromFile(file);
            if (torrentInfo.isValid())
            {
                BitTorrent::Session::instance()->addTorrent(torrentInfo, params);
                Utils::Fs::forceRemove(file);
            }
            else
            {
                qDebug("Ignoring incomplete torrent file: %s", qUtf8Printable(file));
            }
        }
    }
}

QString ScanFoldersModel::pathTypeDisplayName(const PathType type)
{
    switch (type)
    {
    case DOWNLOAD_IN_WATCH_FOLDER:
        return tr("Monitored folder");
    case DEFAULT_LOCATION:
        return tr("Default save location");
    case CUSTOM_LOCATION:
        return tr("Browse...");
    default:
        qDebug("Invalid PathType: %d", type);
    };
    return {};
}
