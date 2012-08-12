/*
 * Bittorrent Client using Qt4 and libtorrent.
 * Copyright (C) 2012  Christophe Dumez
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

#include "fs_utils.h"
#include "misc.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#ifdef DISABLE_GUI
#include <QCoreApplication>
#else
#include <QApplication>
#endif
#include <libtorrent/torrent_info.hpp>

#ifdef Q_WS_MAC
#include <CoreServices/CoreServices.h>
#include <Carbon/Carbon.h>
#endif

#ifndef Q_WS_WIN
#if defined(Q_WS_MAC) || defined(Q_OS_FREEBSD)
#include <sys/param.h>
#include <sys/mount.h>
#else
#include <sys/vfs.h>
#endif
#else
#include <shlobj.h>
#include <winbase.h>
#endif

#if defined(Q_WS_WIN) || defined(Q_OS_OS2)
#include <QDesktopServices>
#endif

using namespace libtorrent;

/**
 * Converts a path to a string suitable for display.
 * This function makes sure the directory separator used is consistent
 * with the OS being run.
 */
QString fsutils::toDisplayPath(const QString& path)
{
#if defined(Q_WS_WIN) || defined(Q_OS_OS2)
 QString ret = path;
 return ret.replace("/", "\\");
#else
 return path;
#endif
}

/**
 * Returns the file extension part of a file name.
 */
QString fsutils::fileExtension(const QString &filename)
{
  const int point_index = filename.lastIndexOf(".");
  return (point_index >= 0) ? filename.mid(point_index + 1) : QString();
}

QString fsutils::fileName(const QString& file_path)
{
  const int slash_index = file_path.lastIndexOf(QRegExp("[/\\\\]"));
  if (slash_index == -1)
    return file_path;
  return file_path.mid(slash_index + 1);
}

bool fsutils::isValidTorrentFile(const QString& torrent_path) {
  try {
    boost::intrusive_ptr<libtorrent::torrent_info> t = new torrent_info(torrent_path.toUtf8().constData());
    if (!t->is_valid() || t->num_files() == 0)
      return false;
  } catch(std::exception&) {
    return false;
  }
  return true;
}

/**
 * Remove an empty folder tree.
 *
 * This function will also remove .DS_Store files on Mac OS and
 * Thumbs.db on Windows.
 */
bool fsutils::smartRemoveEmptyFolderTree(const QString& dir_path)
{
  qDebug() << Q_FUNC_INFO << dir_path;
  if (dir_path.isEmpty())
    return false;

  QDir dir(dir_path);
  if (!dir.exists())
    return true;

  // Remove Files created by the OS
#if defined Q_WS_MAC
  fsutils::forceRemove(dir_path + QLatin1String("/.DS_Store"));
#elif defined Q_WS_WIN
  fsutils::forceRemove(dir_path + QLatin1String("/Thumbs.db"));
#endif

  QFileInfoList sub_files = dir.entryInfoList();
  foreach (const QFileInfo& info, sub_files) {
    QString sub_name = info.fileName();
    if (sub_name == "." || sub_name == "..")
      continue;

    QString sub_path = info.absoluteFilePath();
    qDebug() << Q_FUNC_INFO << "sub file: " << sub_path;
    if (info.isDir()) {
      if (!smartRemoveEmptyFolderTree(sub_path)) {
        qWarning() << Q_FUNC_INFO << "Failed to remove folder: " << sub_path;
        return false;
      }
    } else {
      if (info.isHidden()) {
        qDebug() << Q_FUNC_INFO << "Removing hidden file: " << sub_path;
        if (!fsutils::forceRemove(sub_path)) {
          qWarning() << Q_FUNC_INFO << "Failed to remove " << sub_path;
          return false;
        }
      } else {
        qWarning() << Q_FUNC_INFO << "Folder is not empty, aborting. Found: " << sub_path;
      }
    }
  }
  qDebug() << Q_FUNC_INFO << "Calling rmdir on " << dir_path;
  return QDir().rmdir(dir_path);
}

/**
 * Removes the file with the given file_path.
 *
 * This function will try to fix the file permissions before removing it.
 */
bool fsutils::forceRemove(const QString& file_path)
{
  QFile f(file_path);
  if (!f.exists())
    return true;
  // Make sure we have read/write permissions
  f.setPermissions(f.permissions()|QFile::ReadOwner|QFile::WriteOwner|QFile::ReadUser|QFile::WriteUser);
  // Remove the file
  return f.remove();
}

/**
 * Returns the size of a file.
 * If the file is a folder, it will compute its size based on its content.
 *
 * Returns -1 in case of error.
 */
qint64 fsutils::computePathSize(const QString& path)
{
  // Check if it is a file
  QFileInfo fi(path);
  if (!fi.exists()) return -1;
  if (fi.isFile()) return fi.size();
  // Compute folder size based on its content
  qint64 size = 0;
  foreach (const QFileInfo &subfi, QDir(path).entryInfoList(QDir::Dirs|QDir::Files)) {
    if (subfi.fileName().startsWith(".")) continue;
    if (subfi.isDir())
      size += fsutils::computePathSize(subfi.absoluteFilePath());
    else
      size += subfi.size();
  }
  return size;
}

/**
 * Makes deep comparison of two files to make sure they are identical.
 */
bool fsutils::sameFiles(const QString& path1, const QString& path2)
{
  QFile f1(path1), f2(path2);
  if (!f1.exists() || !f2.exists()) return false;
  if (f1.size() != f2.size()) return false;
  if (!f1.open(QIODevice::ReadOnly)) return false;
  if (!f2.open(QIODevice::ReadOnly)) {
    f1.close();
    return false;
  }
  bool same = true;
  while(!f1.atEnd() && !f2.atEnd()) {
    if (f1.read(1024) != f2.read(1024)) {
      same = false;
      break;
    }
  }
  f1.close(); f2.close();
  return same;
}

QString fsutils::updateLabelInSavePath(QString defaultSavePath,QString save_path, const QString& old_label, const QString& new_label) {
  if (old_label == new_label) return save_path;
  defaultSavePath.replace("\\", "/");
  save_path.replace("\\", "/");
  qDebug("UpdateLabelInSavePath(%s, %s, %s)", qPrintable(save_path), qPrintable(old_label), qPrintable(new_label));
  if (!save_path.startsWith(defaultSavePath)) return save_path;
  QString new_save_path = save_path;
  new_save_path.replace(defaultSavePath, "");
  QStringList path_parts = new_save_path.split("/", QString::SkipEmptyParts);
  if (path_parts.empty()) {
    if (!new_label.isEmpty())
      path_parts << new_label;
  } else {
    if (old_label.isEmpty() || path_parts.first() != old_label) {
      if (path_parts.first() != new_label)
        path_parts.prepend(new_label);
    } else {
      if (new_label.isEmpty()) {
        path_parts.removeAt(0);
      } else {
        if (path_parts.first() != new_label)
          path_parts.replace(0, new_label);
      }
    }
  }
  new_save_path = defaultSavePath;
  if (!new_save_path.endsWith(QDir::separator())) new_save_path += QDir::separator();
  new_save_path += path_parts.join(QDir::separator());
  qDebug("New save path is %s", qPrintable(new_save_path));
  return new_save_path;
}

QString fsutils::toValidFileSystemName(QString filename) {
  qDebug("toValidFSName: %s", qPrintable(filename));
  const QRegExp regex("[\\\\/:?\"*<>|]");
  filename.replace(regex, " ");
  qDebug("toValidFSName, result: %s", qPrintable(filename));
  return filename.trimmed();
}

bool fsutils::isValidFileSystemName(const QString& filename) {
  if (filename.isEmpty()) return false;
  const QRegExp regex("[\\\\/:?\"*<>|]");
  return !filename.contains(regex);
}

long long fsutils::freeDiskSpaceOnPath(QString path) {
  if (path.isEmpty()) return -1;
  path.replace("\\", "/");
  QDir dir_path(path);
  if (!dir_path.exists()) {
    QStringList parts = path.split("/");
    while (parts.size() > 1 && !QDir(parts.join("/")).exists()) {
      parts.removeLast();
    }
    dir_path = QDir(parts.join("/"));
    if (!dir_path.exists()) return -1;
  }
  Q_ASSERT(dir_path.exists());

#ifndef Q_WS_WIN
  unsigned long long available;
  struct statfs stats;
  const QString statfs_path = dir_path.path()+"/.";
  const int ret = statfs (qPrintable(statfs_path), &stats) ;
  if (ret == 0) {
    available = ((unsigned long long)stats.f_bavail) *
        ((unsigned long long)stats.f_bsize) ;
    return available;
  } else {
    return -1;
  }
#else
  typedef BOOL (WINAPI *GetDiskFreeSpaceEx_t)(LPCTSTR,
                                              PULARGE_INTEGER,
                                              PULARGE_INTEGER,
                                              PULARGE_INTEGER);
  GetDiskFreeSpaceEx_t
      pGetDiskFreeSpaceEx = (GetDiskFreeSpaceEx_t)::GetProcAddress
      (
        ::GetModuleHandle(TEXT("kernel32.dll")),
        "GetDiskFreeSpaceExW"
        );
  if ( pGetDiskFreeSpaceEx )
  {
    ULARGE_INTEGER bytesFree, bytesTotal;
    unsigned long long *ret;
    if (pGetDiskFreeSpaceEx((LPCTSTR)(QDir::toNativeSeparators(dir_path.path())).utf16(), &bytesFree, &bytesTotal, NULL)) {
      ret = (unsigned long long*)&bytesFree;
      return *ret;
    } else {
      return -1;
    }
  } else {
    return -1;
  }
#endif
}

QString fsutils::branchPath(const QString& file_path, QString* removed)
{
  QString ret = file_path;
  if (ret.endsWith("/") || ret.endsWith("\\"))
    ret.chop(1);
  const int slashIndex = ret.lastIndexOf(QRegExp("[/\\\\]"));
  if (slashIndex >= 0) {
    if (removed)
      *removed = ret.mid(slashIndex + 1);
    ret = ret.left(slashIndex);
  }
  return ret;
}

bool fsutils::sameFileNames(const QString &first, const QString &second)
{
#if defined(Q_WS_X11) || defined(Q_WS_MAC) || defined(Q_WS_QWS)
  return QString::compare(first, second, Qt::CaseSensitive) == 0;
#else
  return QString::compare(first, second, Qt::CaseInsensitive) == 0;
#endif
}

// Replace ~ in path
QString fsutils::expandPath(const QString& path) {
  QString ret = path.trimmed();
  if (ret.isEmpty()) return ret;
  if (ret == "~")
    return QDir::homePath();
  if (ret[0] == '~' && (ret[1] == '/' || ret[1] == '\\')) {
    ret.replace(0, 1, QDir::homePath());
  } else {
    if (!QDir::isAbsolutePath(ret))
      ret = QDir(ret).absolutePath();
  }
  return QDir::cleanPath(path);
}

QString fsutils::QDesktopServicesDataLocation() {
#ifdef Q_WS_WIN
  LPWSTR path=new WCHAR[256];
  QString result;
#if defined Q_WS_WINCE
  if (SHGetSpecialFolderPath(0, path, CSIDL_APPDATA, FALSE))
#else
  if (SHGetSpecialFolderPath(0, path, CSIDL_LOCAL_APPDATA, FALSE))
#endif
    result = QString::fromWCharArray(path);
  if (!QCoreApplication::applicationName().isEmpty())
    result = result + QLatin1String("\\") + qApp->applicationName();
  if (!result.endsWith("\\"))
    result += "\\";
  return result;
#else
#ifdef Q_WS_MAC
  FSRef ref;
  OSErr err = FSFindFolder(kUserDomain, kApplicationSupportFolderType, false, &ref);
  if (err)
    return QString();
  QString path;
  QByteArray ba(2048, 0);
  if (FSRefMakePath(&ref, reinterpret_cast<UInt8 *>(ba.data()), ba.size()) == noErr)
    path = QString::fromUtf8(ba).normalized(QString::NormalizationForm_C);
  path += QLatin1Char('/') + qApp->applicationName();
  return path;
#else
  QString xdgDataHome = QLatin1String(qgetenv("XDG_DATA_HOME"));
  if (xdgDataHome.isEmpty())
    xdgDataHome = QDir::homePath() + QLatin1String("/.local/share");
  xdgDataHome += QLatin1String("/data/")
      + qApp->applicationName();
  return xdgDataHome;
#endif
#endif
}

QString fsutils::QDesktopServicesCacheLocation() {
#ifdef Q_WS_WIN
  return QDesktopServicesDataLocation() + QLatin1String("\\cache");
#else
#ifdef Q_WS_MAC
  // http://developer.apple.com/documentation/Carbon/Reference/Folder_Manager/Reference/reference.html
  FSRef ref;
  OSErr err = FSFindFolder(kUserDomain, kCachedDataFolderType, false, &ref);
  if (err)
    return QString();
  QString path;
  QByteArray ba(2048, 0);
  if (FSRefMakePath(&ref, reinterpret_cast<UInt8 *>(ba.data()), ba.size()) == noErr)
    path = QString::fromUtf8(ba).normalized(QString::NormalizationForm_C);
  path += QLatin1Char('/') + qApp->applicationName();
  return path;
#else
  QString xdgCacheHome = QLatin1String(qgetenv("XDG_CACHE_HOME"));
  if (xdgCacheHome.isEmpty())
    xdgCacheHome = QDir::homePath() + QLatin1String("/.cache");
  xdgCacheHome += QLatin1Char('/') + QCoreApplication::applicationName();
  return xdgCacheHome;
#endif
#endif
}

QString fsutils::QDesktopServicesDownloadLocation() {
#if defined(Q_WS_WIN) || defined(Q_OS_OS2)
  // as long as it stays WinXP like we do the same on OS/2
  // TODO: Use IKnownFolderManager to get path of FOLDERID_Downloads
  // instead of hardcoding "Downloads"
  // Unfortunately, this would break compatibility with WinXP
  return QDir(QDesktopServices::storageLocation(QDesktopServices::DocumentsLocation)).absoluteFilePath(tr("Downloads"));
#endif

#ifdef Q_WS_X11
  QString save_path;
  // Default save path on Linux
  QString config_path = QString::fromLocal8Bit(qgetenv("XDG_CONFIG_HOME").constData());
  if (config_path.isEmpty())
    config_path = QDir::home().absoluteFilePath(".config");

  QString user_dirs_file = config_path + "/user-dirs.dirs";
  if (QFile::exists(user_dirs_file)) {
    QSettings settings(user_dirs_file, QSettings::IniFormat);
    QString xdg_download_dir = settings.value("XDG_DOWNLOAD_DIR").toString();
    if (!xdg_download_dir.isEmpty()) {
      // Resolve $HOME environment variables
      xdg_download_dir.replace("$HOME", QDir::homePath());
      save_path = xdg_download_dir;
      qDebug() << Q_FUNC_INFO << "SUCCESS: Using XDG path for downloads: " << save_path;
    }
  }

  // Fallback
  if (!save_path.isEmpty() && !QFile::exists(save_path)) {
    QDir().mkpath(save_path);
  }

  if (save_path.isEmpty() || !QFile::exists(save_path)) {
    save_path = QDir::home().absoluteFilePath(tr("Downloads"));
    qDebug() << Q_FUNC_INFO << "using" << save_path << "as fallback since the XDG detection did not work";
  }

  return save_path;
#endif

#ifdef Q_WS_MAC
  // TODO: How to support this on Mac OS X?
#endif

  // Fallback
  return QDir::home().absoluteFilePath(tr("Downloads"));
}

QString fsutils::searchEngineLocation() {
  QString folder = "nova";
  if (misc::pythonVersion() >= 3)
    folder = "nova3";
  const QString location = QDir::cleanPath(QDesktopServicesDataLocation()
                                           + QDir::separator() + folder);
  QDir locationDir(location);
  if (!locationDir.exists())
    locationDir.mkpath(locationDir.absolutePath());
  return location;
}

QString fsutils::BTBackupLocation() {
  const QString location = QDir::cleanPath(QDesktopServicesDataLocation()
                                           + QDir::separator() + "BT_backup");
  QDir locationDir(location);
  if (!locationDir.exists())
    locationDir.mkpath(locationDir.absolutePath());
  return location;
}

QString fsutils::cacheLocation() {
  QString location = QDir::cleanPath(QDesktopServicesCacheLocation());
  QDir locationDir(location);
  if (!locationDir.exists())
    locationDir.mkpath(locationDir.absolutePath());
  return location;
}
