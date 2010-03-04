/*
 * Bittorrent Client using Qt4 and libtorrent.
 * Copyright (C) 2006  Christophe Dumez
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

#ifndef MISC_H
#define MISC_H

#include <sstream>
#include <stdexcept>
#include <QObject>
#include <QString>
#include <QByteArray>
#include <QFileInfo>
#include <QDir>
#include <QList>
#include <QPair>
#include <QThread>
#include <QUrl>
#include <ctime>
#include <QDateTime>
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <boost/date_time/posix_time/conversion.hpp>

#ifdef DISABLE_GUI
#include <QCoreApplication>
#else
#include <QApplication>
#include <QDesktopWidget>
#endif

#ifdef Q_WS_WIN
#include <shlobj.h>
#endif

#ifdef Q_WS_MAC
#include <Files.h>
#include <Folders.h>
#endif

#ifndef Q_WS_WIN
#ifdef Q_WS_MAC
#include <sys/param.h>
#include <sys/mount.h>
#else
#include <sys/vfs.h>
#endif
#else
#include <winbase.h>
#endif

#include <libtorrent/torrent_info.hpp>
using namespace libtorrent;

/*  Miscellaneaous functions that can be useful */
class misc : public QObject{
  Q_OBJECT

public:
  static inline QString toQString(std::string str) {
    return QString::fromLocal8Bit(str.c_str());
  }

  static inline QString toQString(char* str) {
    return QString::fromLocal8Bit(str);
  }

  static inline QString toQString(sha1_hash hash) {
    std::ostringstream o;
    if(!(o<<hash)) {
      throw std::runtime_error("::toString()");
    }
    return QString::fromLocal8Bit(o.str().c_str());
    //return QString::fromLocal8Bit(hash.to_string().c_str());
  }

  static inline sha1_hash QStringToSha1(const QString& s) {
      sha1_hash x;
      std::istringstream i(s.toStdString());
      if(!(i>>x)) {
        throw std::runtime_error("::fromString()");
      }
      return x;
    }

  static bool sameFiles(QString path1, QString path2) {
    QFile f1(path1);
    if(!f1.exists()) return false;
    QFile f2(path2);
    if(!f2.exists()) return false;
    QByteArray content1, content2;
    if(f1.open(QIODevice::ReadOnly)) {
      content1 = f1.readAll();
      f1.close();
    } else {
      return false;
    }
    if(f2.open(QIODevice::ReadOnly)) {
      content1 = f2.readAll();
      f2.close();
    } else {
      return false;
    }
    return content1 == content2;
  }

  static void copyDir(QString src_path, QString dst_path) {
    QDir sourceDir(src_path);
    if(!sourceDir.exists()) return;
    // Create destination directory
    QDir destDir(dst_path);
    if(!destDir.exists()) {
      if(!destDir.mkpath(destDir.absolutePath())) return;
    }
    // List source directory
    const QFileInfoList &content = sourceDir.entryInfoList();
    foreach(const QFileInfo& child, content) {
      if(child.fileName()[0] == '.') continue;
      if(child.isDir()) {
        copyDir(child.absoluteFilePath(), dst_path+QDir::separator()+QDir(child.absoluteFilePath()).dirName());
        continue;
      }
      const QString &src_child_path = child.absoluteFilePath();
      const QString &dest_child_path = destDir.absoluteFilePath(child.fileName());
      // Copy the file from src to dest
      QFile::copy(src_child_path, dest_child_path);
      // Remove source file
      QFile::remove(src_child_path);
    }
    // Remove source folder
    const QString &dir_name = sourceDir.dirName();
    if(sourceDir.cdUp()) {
      sourceDir.rmdir(dir_name);
    }
  }

  // Introduced in v2.1.0
  // For backward compatibility
  // Remove after some releases
  static void moveToXDGFolders() {
    const QString &old_qBtPath = QDir::homePath()+QDir::separator()+QString::fromUtf8(".qbittorrent") + QDir::separator();
    if(QDir(old_qBtPath).exists()) {
      // Copy BT_backup folder
      const QString &old_BTBackupPath = old_qBtPath + "BT_backup";
      if(QDir(old_BTBackupPath).exists()) {
        copyDir(old_BTBackupPath, BTBackupLocation());
      }
      // Copy search engine folder
      const QString &old_searchPath = old_qBtPath + "search_engine";
      if(QDir(old_searchPath).exists()) {
        copyDir(old_searchPath, searchEngineLocation());
      }
      // Copy *_state files
      if(QFile::exists(old_qBtPath+"dht_state")) {
        QFile::copy(old_qBtPath+"dht_state", cacheLocation()+QDir::separator()+"dht_state");
        QFile::remove(old_qBtPath+"dht_state");
      }
      if(QFile::exists(old_qBtPath+"ses_state")) {
        QFile::copy(old_qBtPath+"ses_state", cacheLocation()+QDir::separator()+"ses_state");
        QFile::remove(old_qBtPath+"ses_state");
      }
      // Remove .qbittorrent folder if empty
      QDir::home().rmdir(".qbittorrent");
    }
  }

  static QString toValidFileSystemName(QString filename) {
    qDebug("toValidFSName: %s", qPrintable(filename));
    filename = filename.replace("\\", "/").trimmed();
    const QRegExp regex("[/:?\"*<>|]");
    filename = filename.replace(regex, " ").trimmed();
    qDebug("toValidFSName, result: %s", qPrintable(filename));
    return filename;
  }

  static bool isValidFileSystemName(QString filename) {
    filename = filename.replace("\\", "/").trimmed();
    if(filename.isEmpty()) return false;
    const QRegExp regex("[/:?\"*<>|]");
    if(filename.contains(regex))
      return false;
    return true;
  }

/* Ported from Qt4 to drop dependency on QtGui */

#ifdef Q_WS_MAC
  static QString getFullPath(const FSRef &ref)
  {
    QByteArray ba(2048, 0);
    if (FSRefMakePath(&ref, reinterpret_cast<UInt8 *>(ba.data()), ba.size()) == noErr)
      return QString::fromUtf8(ba).normalized(QString::NormalizationForm_C);
    return QString();
  }
#endif

  static QString QDesktopServicesDataLocation() {
#ifdef Q_WS_WIN
#if defined Q_WS_WINCE
    if (SHGetSpecialFolderPath(0, path, CSIDL_APPDATA, FALSE))
#else
      if (SHGetSpecialFolderPath(0, path, CSIDL_LOCAL_APPDATA, FALSE))
#endif
        result = QString::fromWCharArray(path);
    if (!QCoreApplication::applicationName().isEmpty())
      result = result + QLatin1String("\\") + qApp->applicationName();
#else
#ifdef Q_WS_MAC
    // http://developer.apple.com/documentation/Carbon/Reference/Folder_Manager/Reference/reference.html
    FSRef ref;
    OSErr err = FSFindFolder(kUserDomain, kApplicationSupportFolderType, false, &ref);
    if (err)
      return QString();
    QString path = getFullPath(ref);
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

  static QString QDesktopServicesCacheLocation() {
#ifdef Q_WS_WIN
    return QDesktopServicesDataLocation() + QLatin1String("\\cache");
#else
#ifdef Q_WS_MAC
    // http://developer.apple.com/documentation/Carbon/Reference/Folder_Manager/Reference/reference.html
    FSRef ref;
    OSErr err = FSFindFolder(kUserDomain, kCachedDataFolderType, false, &ref);
    if (err)
      return QString();
    QString path = getFullPath(ref);
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

  /* End of Qt4 code */

#ifndef DISABLE_GUI
  // Get screen center
  static QPoint screenCenter(QWidget *win) {
    int scrn = 0;
    const QWidget *w = win->window();

    if(w)
      scrn = QApplication::desktop()->screenNumber(w);
    else if(QApplication::desktop()->isVirtualDesktop())
      scrn = QApplication::desktop()->screenNumber(QCursor::pos());
    else
      scrn = QApplication::desktop()->screenNumber(win);

    QRect desk(QApplication::desktop()->availableGeometry(scrn));
    return QPoint((desk.width() - win->frameGeometry().width()) / 2, (desk.height() - win->frameGeometry().height()) / 2);
  }
#endif

  static QString searchEngineLocation() {
    const QString &location = QDir::cleanPath(QDesktopServicesDataLocation()
                                       + QDir::separator() + "search_engine");
    QDir locationDir(location);
    if(!locationDir.exists())
      locationDir.mkpath(locationDir.absolutePath());
    return location;
  }

  static QString BTBackupLocation() {
    const QString &location = QDir::cleanPath(QDesktopServicesDataLocation()
                                       + QDir::separator() + "BT_backup");
    QDir locationDir(location);
    if(!locationDir.exists())
      locationDir.mkpath(locationDir.absolutePath());
    return location;
  }

  static QString cacheLocation() {
    const QString &location = QDir::cleanPath(QDesktopServicesCacheLocation());
    QDir locationDir(location);
    if(!locationDir.exists())
      locationDir.mkpath(locationDir.absolutePath());
    return location;
  }

  static long long freeDiskSpaceOnPath(QString path) {
    if(path.isEmpty()) return -1;
    QDir dir_path(path);
    if(!dir_path.exists()) {
      if(!dir_path.cdUp()) return -1;
    }
    Q_ASSERT(dir_path.exists());
#ifndef Q_WS_WIN
    unsigned long long available;
    struct statfs stats;
    const int ret = statfs ((dir_path.path()+"/.").toLocal8Bit().data(), &stats) ;
    if(ret == 0) {
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
                                  ::GetModuleHandle(_T("kernel32.dll")),
                                  "GetDiskFreeSpaceExW"
                                  );
    if ( pGetDiskFreeSpaceEx )
    {
      ULARGE_INTEGER bytesFree, bytesTotal;
      unsigned long long *ret;
      if (pGetDiskFreeSpaceEx((LPCTSTR)path.ucs2(), &bytesFree, &bytesTotal, NULL)) {
        tmp = (unsigned long long*)&bytesFree
              return ret;
      } else {
        return -1;
      }
    } else {
      return -1;
    }
#endif
  }

  // return best userfriendly storage unit (B, KiB, MiB, GiB, TiB)
  // use Binary prefix standards from IEC 60027-2
  // see http://en.wikipedia.org/wiki/Kilobyte
  // value must be given in bytes
  static QString friendlyUnit(double val) {
    if(val < 0)
      return tr("Unknown", "Unknown (size)");
    const QString units[5] = {tr("B", "bytes"), tr("KiB", "kibibytes (1024 bytes)"), tr("MiB", "mebibytes (1024 kibibytes)"), tr("GiB", "gibibytes (1024 mibibytes)"), tr("TiB", "tebibytes (1024 gibibytes)")};
    char i = 0;
    while(val >= 1024. && i++<6)
      val /= 1024.;
    return QString::number(val, 'f', 1) + " " + units[(int)i];
  }

  static bool isPreviewable(QString extension){
    extension = extension.toUpper();
    if(extension == "AVI") return true;
    if(extension == "MP3") return true;
    if(extension == "OGG") return true;
    if(extension == "OGM") return true;
    if(extension == "WMV") return true;
    if(extension == "WMA") return true;
    if(extension == "MPEG") return true;
    if(extension == "MPG") return true;
    if(extension == "ASF") return true;
    if(extension == "QT") return true;
    if(extension == "RM") return true;
    if(extension == "RMVB") return true;
    if(extension == "RMV") return true;
    if(extension == "SWF") return true;
    if(extension == "FLV") return true;
    if(extension == "WAV") return true;
    if(extension == "MOV") return true;
    if(extension == "VOB") return true;
    if(extension == "MID") return true;
    if(extension == "AC3") return true;
    if(extension == "MP4") return true;
    if(extension == "MP2") return true;
    if(extension == "AVI") return true;
    if(extension == "FLAC") return true;
    if(extension == "AU") return true;
    if(extension == "MPE") return true;
    if(extension == "MOV") return true;
    if(extension == "MKV") return true;
    if(extension == "AIF") return true;
    if(extension == "AIFF") return true;
    if(extension == "AIFC") return true;
    if(extension == "RA") return true;
    if(extension == "RAM") return true;
    if(extension == "M4P") return true;
    if(extension == "M4A") return true;
    if(extension == "3GP") return true;
    if(extension == "AAC") return true;
    if(extension == "SWA") return true;
    if(extension == "MPC") return true;
    if(extension == "MPP") return true;
    return false;
  }

  static bool removeEmptyTree(QString path) {
    QDir dir(path);
    foreach(const QString &child, dir.entryList(QDir::AllDirs)) {
      if(child == "." || child == "..") continue;
      return removeEmptyTree(dir.absoluteFilePath(child));
    }
    const QString &dir_name = dir.dirName();
    if(dir.cdUp()) {
      return dir.rmdir(dir_name);
    }
    return false;
  }

  static QString magnetUriToName(QString magnet_uri) {
    QString name = "";
    const QRegExp regHex("dn=([^&]+)");
    const int pos = regHex.indexIn(magnet_uri);
    if(pos > -1) {
      const QString &found = regHex.cap(1);
      // URL decode
      name = QUrl::fromPercentEncoding(found.toLocal8Bit()).replace("+", " ");
    }
    return name;
  }

  static QString magnetUriToHash(QString magnet_uri) {
    QString hash = "";
    const QRegExp regHex("urn:btih:([0-9A-Za-z]+)");
    // Hex
    int pos = regHex.indexIn(magnet_uri);
    if(pos > -1) {
      const QString &found = regHex.cap(1);
      if(found.length() == 40) {
        const sha1_hash sha1(QString(QByteArray::fromHex(regHex.cap(1).toLocal8Bit())).toStdString());
        qDebug("magnetUriToHash (Hex): hash: %s", qPrintable(misc::toQString(sha1)));
        return misc::toQString(sha1);
      }
    }
    // Base 32
    const QRegExp regBase32("urn:btih:([A-Za-z2-7=]+)");
    pos = regBase32.indexIn(magnet_uri);
    if(pos > -1) {
      const QString &found = regBase32.cap(1);
      if(found.length() > 20 && (found.length()*5)%40 == 0) {
        const sha1_hash sha1(base32decode(regBase32.cap(1).toStdString()));
        hash = misc::toQString(sha1);
      }
    }
    qDebug("magnetUriToHash (base32): hash: %s", qPrintable(hash));
    return hash;
  }

  static QString boostTimeToQString(const boost::optional<boost::posix_time::ptime> boostDate) {
    if(!boostDate) return tr("Unknown");
    struct std::tm tm = boost::posix_time::to_tm(*boostDate);
    return QDateTime::fromTime_t(mktime(&tm)).toString(Qt::DefaultLocaleLongDate);
  }

  // Replace ~ in path
  static QString expandPath(QString path) {
    path = path.trimmed();
    if(path.isEmpty()) return path;
    if(path.length() == 1) {
      if(path[0] == '~' ) return QDir::homePath();
    }
    if(path[0] == '~' && path[1] == QDir::separator()) {
      path = path.replace(0, 1, QDir::homePath());
    } else {
      if(QDir::isAbsolutePath(path)) {
        path = QDir(path).absolutePath();
      }
    }
    return QDir::cleanPath(path);
  }

  // Take a number of seconds and return an user-friendly
  // time duration like "1d 2h 10m".
  static QString userFriendlyDuration(qlonglong seconds) {
    if(seconds < 0) {
      return QString::fromUtf8("∞");
    }
    if(seconds < 60) {
      return tr("< 1m", "< 1 minute");
    }
    int minutes = seconds / 60;
    if(minutes < 60) {
      return tr("%1m","e.g: 10minutes").arg(QString::number(minutes));
    }
    int hours = minutes / 60;
    minutes = minutes - hours*60;
    if(hours < 24) {
      return tr("%1h%2m", "e.g: 3hours 5minutes").arg(QString::number(hours)).arg(QString::number(minutes));
    }
    int days = hours / 24;
    hours = hours - days * 24;
    if(days < 100) {
      return tr("%1d%2h%3m", "e.g: 2days 10hours 2minutes").arg(QString::number(days)).arg(QString::number(hours)).arg(QString::number(minutes));
    }
    return QString::fromUtf8("∞");
  }
};

//  Trick to get a portable sleep() function
class SleeperThread : public QThread{
public:
  static void msleep(unsigned long msecs)
  {
    QThread::msleep(msecs);
  }
};

#endif
