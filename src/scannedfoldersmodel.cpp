/*
 * Bittorrent Client using Qt4 and libtorrent.
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

#include "scannedfoldersmodel.h"
#include "preferences.h"
#include "filesystemwatcher.h"

#include <QDir>
#include <QFileInfo>
#include <QString>
#include <QTemporaryFile>
#include "qinisettings.h"
#include "misc.h"

namespace {
  const int PathColumn = 0;
  const int DownloadAtTorrentColumn = 1;
  const int DownloadPath = 2;
}

class ScanFoldersModel::PathData {
public:
  PathData(const QString &path) : path(path), downloadAtPath(false), downloadPath(path) {}
  PathData(const QString &path, bool download_at_path, const QString &download_path) : path(path), downloadAtPath(download_at_path), downloadPath(download_path) {}
  const QString path;   //watching directory
  bool downloadAtPath;  //if TRUE save data to watching directory
  QString downloadPath; //if 'downloadAtPath' FALSE use this path for save data
};

ScanFoldersModel *ScanFoldersModel::instance(QObject *parent) {
  //Q_ASSERT(!parent != !m_instance);
  if (!m_instance)
    m_instance = new ScanFoldersModel(parent);
  return m_instance;
}

ScanFoldersModel::ScanFoldersModel(QObject *parent) :
    QAbstractTableModel(parent), m_fsWatcher(0)
{ }

ScanFoldersModel::~ScanFoldersModel() {
  qDeleteAll(m_pathList);
}

int ScanFoldersModel::rowCount(const QModelIndex &parent) const {
  return parent.isValid() ? 0 : m_pathList.count();
}

int ScanFoldersModel::columnCount(const QModelIndex &parent) const {
  Q_UNUSED(parent);
  return 3;
}

QVariant ScanFoldersModel::data(const QModelIndex &index, int role) const
{
  if (!index.isValid() || index.row() >= rowCount())
  return QVariant();
  const PathData* pathData = m_pathList.at(index.row());

  switch (index.column())
  {
      case PathColumn:
      {
          if (role != Qt::DisplayRole)          return QVariant();
    #if defined(Q_WS_WIN) || defined(Q_OS_OS2)
          QString ret = pathData->path;
          return ret.replace("/", "\\");
    #else
          return pathData->path;
    #endif
      }
      case DownloadAtTorrentColumn:
      {
          if (role != Qt::CheckStateRole)          return QVariant();
          return pathData->downloadAtPath ? Qt::Checked : Qt::Unchecked;
      }
      case DownloadPath:
      {
          if (role == Qt::DisplayRole || role == Qt::EditRole || role == Qt::ToolTipRole){
    #if defined(Q_WS_WIN) || defined(Q_OS_OS2)
          QString ret = pathData->downloadPath;
          return ret.replace("/", "\\");
    #else
          return pathData->downloadPath;
    #endif
          }
      }
  }
  return QVariant();
}

QVariant ScanFoldersModel::headerData(int section, Qt::Orientation orientation, int role) const {
  if (orientation != Qt::Horizontal || role != Qt::DisplayRole || section < 0 || section >= columnCount())
    return QVariant();

  switch (section)
  {
  case PathColumn: return tr("Watched Folder");
  case DownloadAtTorrentColumn: return tr("Download here");
  case DownloadPath: return tr("Download path");
  }

  return QVariant();
}

Qt::ItemFlags ScanFoldersModel::flags(const QModelIndex &index) const {
    if (!index.isValid() || index.row() >= rowCount())
        return QAbstractTableModel::flags(index);

    const PathData* pathData = m_pathList.at(index.row());

    switch (index.column())
    {
    case PathColumn: return QAbstractTableModel::flags(index);
    case DownloadAtTorrentColumn: return QAbstractTableModel::flags(index) | Qt::ItemIsUserCheckable;
    case DownloadPath:{
        if (pathData->downloadAtPath == 0){
          return QAbstractTableModel::flags(index) | Qt::ItemIsEditable | Qt::ItemIsEnabled;
        }
        else{   //dont edit if checked 'downloadAtPath'
          return QAbstractTableModel::flags(index);
        }
    }

    }

    return QAbstractTableModel::flags(index);
}

bool ScanFoldersModel::setData(const QModelIndex &index, const QVariant &value, int role) {
  if (!index.isValid() || index.row() >= rowCount() || index.column() > DownloadPath  )
    return false;

  if (index.column() == PathColumn)
      return false;

  if (index.column() == DownloadAtTorrentColumn && role == Qt::CheckStateRole)  {
      Q_ASSERT(index.column() == DownloadAtTorrentColumn);
      m_pathList[index.row()]->downloadAtPath = (value.toInt() == Qt::Checked);
      emit dataChanged(index, index);
      return true;
  }

  if (index.column() == DownloadPath)  {
      Q_ASSERT(index.column() == DownloadPath);
      m_pathList[index.row()]->downloadPath = value.toString();
      emit dataChanged(index, index);
      return true;
  }
  return true;
}



ScanFoldersModel::PathStatus ScanFoldersModel::addPath(const QString &path, bool download_at_path, const QString &download_path) {
  QDir dir(path);
  if (!dir.exists())
    return DoesNotExist;
  if (!dir.isReadable())
    return CannotRead;
  const QString &canonicalPath = dir.canonicalPath();
  if (findPathData(canonicalPath) != -1)
    return AlreadyInList;
  if (!m_fsWatcher) {
    m_fsWatcher = new FileSystemWatcher(this);
    connect(m_fsWatcher, SIGNAL(torrentsAdded(QStringList&)), this, SIGNAL(torrentsAdded(QStringList&)));
  }
  beginInsertRows(QModelIndex(), rowCount(), rowCount());
  m_pathList << new PathData(canonicalPath, download_at_path, download_path);
  endInsertRows();
  // Start scanning
  m_fsWatcher->addPath(canonicalPath);
  return Ok;
}

void ScanFoldersModel::removePath(int row) {
  Q_ASSERT(row >= 0 && row < rowCount());
  beginRemoveRows(QModelIndex(), row, row);
  m_fsWatcher->removePath(m_pathList.at(row)->path);
  m_pathList.removeAt(row);
  endRemoveRows();
}

bool ScanFoldersModel::removePath(const QString &path) {
  const int row = findPathData(path);
  if (row == -1)
    return false;
  removePath(row);
  return true;
}

ScanFoldersModel::PathStatus ScanFoldersModel::setDownloadAtPath(int row, bool downloadAtPath) {
  Q_ASSERT(row >= 0 && row < rowCount());

  bool &oldValue = m_pathList[row]->downloadAtPath;
  if (oldValue != downloadAtPath) {
    if (downloadAtPath) {
      QTemporaryFile testFile(m_pathList[row]->path + "/tmpFile");
      if (!testFile.open())
        return CannotWrite;
    }
    oldValue = downloadAtPath;
    const QModelIndex changedIndex = index(row, DownloadAtTorrentColumn);
    emit dataChanged(changedIndex, changedIndex);
  }
  return Ok;
}

bool ScanFoldersModel::downloadInTorrentFolder(const QString &filePath) const {
  const int row = findPathData(QFileInfo(filePath).dir().path());
  Q_ASSERT(row != -1);
  PathData *data = m_pathList.at(row);
  return data->downloadAtPath;
}

QString ScanFoldersModel::downloadPathTorrentFolder(const QString &filePath) const {
  const int row = findPathData(QFileInfo(filePath).dir().path());
  Q_ASSERT(row != -1);
  PathData *data = m_pathList.at(row);
  return  data->downloadPath;
}

int ScanFoldersModel::findPathData(const QString &path) const {
  for (int i = 0; i < m_pathList.count(); ++i) {
    const PathData* pathData = m_pathList.at(i);
    if (pathData->path == path)
      return i;
  }

  return -1;
}

void ScanFoldersModel::makePersistent() {
  Preferences pref;
  QStringList paths;
  QStringList downloadpaths;
  QList<bool> downloadInFolderInfo;
  foreach (const PathData* pathData, m_pathList) {
    paths << pathData->path;
    downloadpaths << pathData->downloadPath;
    downloadInFolderInfo << pathData->downloadAtPath;
  }
  pref.setScanDirs(paths);
  pref.setDownloadInScanDirs(downloadInFolderInfo);
  pref.setDownloadPathsInScanDir(downloadpaths);
}

ScanFoldersModel *ScanFoldersModel::m_instance = 0;
