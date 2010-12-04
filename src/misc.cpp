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

#include "misc.h"

#include <QUrl>
#include <QDir>
#include <QFileInfo>
#include <QDateTime>
#include <QByteArray>

#ifdef DISABLE_GUI
#include <QCoreApplication>
#else
#include <QApplication>
#include <QDesktopWidget>
#endif

#ifdef Q_WS_WIN
#include <shlobj.h>
#include <windows.h>
const int UNLEN = 256;
#else
#include <unistd.h>
#include <sys/types.h>
#endif

#ifdef Q_WS_MAC
#include <CoreServices/CoreServices.h>
#include <Carbon/Carbon.h>
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

#ifdef Q_WS_X11
#include <QDBusInterface>
#include <QDBusMessage>
#endif

using namespace libtorrent;

const QString misc::units[5] = {tr("B", "bytes"), tr("KiB", "kibibytes (1024 bytes)"), tr("MiB", "mebibytes (1024 kibibytes)"), tr("GiB", "gibibytes (1024 mibibytes)"), tr("TiB", "tebibytes (1024 gibibytes)")};

QString misc::QDesktopServicesDataLocation() {
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
  if(!result.endsWith("\\"))
    result += "\\";
  return result;
#else
#ifdef Q_WS_MAC
  // http://developer.apple.com/documentation/Carbon/Reference/Folder_Manager/Reference/reference.html
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

QString misc::QDesktopServicesCacheLocation() {
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

long long misc::freeDiskSpaceOnPath(QString path) {
  if(path.isEmpty()) return -1;
  path = path.replace("\\", "/");
  QDir dir_path(path);
  if(!dir_path.exists()) {
    QStringList parts = path.split("/");
    while (parts.size() > 1 && !QDir(parts.join("/")).exists()) {
      parts.removeLast();
    }
    dir_path = QDir(parts.join("/"));
    if(!dir_path.exists()) return -1;
  }
  Q_ASSERT(dir_path.exists());
#ifndef Q_WS_WIN
  unsigned long long available;
  struct statfs stats;
  const QString statfs_path = dir_path.path()+"/.";
  const int ret = statfs (qPrintable(statfs_path), &stats) ;
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
        ::GetModuleHandle(TEXT("kernel32.dll")),
        "GetDiskFreeSpaceExW"
        );
  if ( pGetDiskFreeSpaceEx )
  {
    ULARGE_INTEGER bytesFree, bytesTotal;
    unsigned long long *ret;
    if (pGetDiskFreeSpaceEx((LPCTSTR)path.utf16(), &bytesFree, &bytesTotal, NULL)) {
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

void misc::shutdownComputer() {
#ifdef Q_WS_X11
  // Use dbus to power off the system
  // dbus-send --print-reply --system --dest=org.freedesktop.Hal /org/freedesktop/Hal/devices/computer org.freedesktop.Hal.Device.SystemPowerManagement.Shutdown
  QDBusInterface computer("org.freedesktop.Hal", "/org/freedesktop/Hal/devices/computer", "org.freedesktop.Hal.Device.SystemPowerManagement", QDBusConnection::systemBus());
  computer.call("Shutdown");
#endif
#ifdef Q_WS_MAC
  AEEventID EventToSend = kAEShutDown;
  AEAddressDesc targetDesc;
  static const ProcessSerialNumber kPSNOfSystemProcess = { 0, kSystemProcess };
  AppleEvent eventReply = {typeNull, NULL};
  AppleEvent appleEventToSend = {typeNull, NULL};

  OSStatus error = noErr;

  error = AECreateDesc(typeProcessSerialNumber, &kPSNOfSystemProcess,
                       sizeof(kPSNOfSystemProcess), &targetDesc);

  if (error != noErr)
  {
    return;
  }

  error = AECreateAppleEvent(kCoreEventClass, EventToSend, &targetDesc,
                             kAutoGenerateReturnID, kAnyTransactionID, &appleEventToSend);

  AEDisposeDesc(&targetDesc);
  if (error != noErr)
  {
    return;
  }

  error = AESend(&appleEventToSend, &eventReply, kAENoReply,
                 kAENormalPriority, kAEDefaultTimeout, NULL, NULL);

  AEDisposeDesc(&appleEventToSend);
  if (error != noErr)
  {
    return;
  }

  AEDisposeDesc(&eventReply);
#endif
#ifdef Q_WS_WIN
  HANDLE hToken;              // handle to process token
  TOKEN_PRIVILEGES tkp;       // pointer to token structure
  if(!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
    return;
  // Get the LUID for shutdown privilege.
  LookupPrivilegeValue(NULL, SE_SHUTDOWN_NAME,
                       &tkp.Privileges[0].Luid);

  tkp.PrivilegeCount = 1;  // one privilege to set
  tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

  // Get shutdown privilege for this process.

  AdjustTokenPrivileges(hToken, FALSE, &tkp, 0,
                        (PTOKEN_PRIVILEGES) NULL, 0);

  // Cannot test the return value of AdjustTokenPrivileges.

  if (GetLastError() != ERROR_SUCCESS)
    return;
  bool ret = InitiateSystemShutdownA(0, tr("qBittorrent will shutdown the computer now because all downloads are complete.").toLocal8Bit().data(), 10, true, false);
  qDebug("ret: %d", (int)ret);

  // Disable shutdown privilege.
  tkp.Privileges[0].Attributes = 0;
  AdjustTokenPrivileges(hToken, FALSE, &tkp, 0,
                        (PTOKEN_PRIVILEGES) NULL, 0);
#endif
}

QString misc::truncateRootFolder(boost::intrusive_ptr<torrent_info> t) {
#if LIBTORRENT_VERSION_MINOR >= 16
  file_storage fs = t->files();
#endif
  if(t->num_files() == 1) {
    // Single file torrent
    // Remove possible subfolders
#if LIBTORRENT_VERSION_MINOR >= 16
    QString path = toQStringU(fs.file_path(t->file_at(0)));
#else
    QString path = QString::fromUtf8(t->file_at(0).path.string().c_str());
#endif
    QStringList path_parts = path.split("/", QString::SkipEmptyParts);
    t->rename_file(0, path_parts.last().toUtf8().data());
    return QString::null;
  }
  QString root_folder;
  int i = 0;
  for(torrent_info::file_iterator it = t->begin_files(); it < t->end_files(); it++) {
#if LIBTORRENT_VERSION_MINOR >= 16
    QString path = toQStringU(fs.file_path(*it));
#else
    QString path = QString::fromUtf8(it->path.string().c_str());
#endif
    QStringList path_parts = path.split("/", QString::SkipEmptyParts);
    if(path_parts.size() > 1) {
      root_folder = path_parts.takeFirst();
      t->rename_file(i, path_parts.join("/").toUtf8().data());
    }
    ++i;
  }
  return root_folder;
}

QString misc::truncateRootFolder(libtorrent::torrent_handle h) {
  torrent_info t = h.get_torrent_info();
#if LIBTORRENT_VERSION_MINOR >= 16
  file_storage fs = t.files();
#endif
  if(t.num_files() == 1) {
    // Single file torrent
    // Remove possible subfolders
#if LIBTORRENT_VERSION_MINOR >= 16
    QString path = misc::toQStringU(fs.file_path(t.file_at(0)));
#else
    QString path = QString::fromUtf8(t.file_at(0).path.string().c_str());
#endif
    QStringList path_parts = path.split("/", QString::SkipEmptyParts);
    t.rename_file(0, path_parts.last().toUtf8().data());
    return QString::null;
  }
  QString root_folder;
  int i = 0;
  for(torrent_info::file_iterator it = t.begin_files(); it < t.end_files(); it++) {
#if LIBTORRENT_VERSION_MINOR >= 16
    QString path = toQStringU(fs.file_path(*it));
#else
    QString path = QString::fromUtf8(it->path.string().c_str());
#endif
    QStringList path_parts = path.split("/", QString::SkipEmptyParts);
    if(path_parts.size() > 1) {
      root_folder = path_parts.takeFirst();
      h.rename_file(i, path_parts.join("/").toUtf8().data());
    }
    ++i;
  }
  return root_folder;
}

bool misc::sameFiles(const QString &path1, const QString &path2) {
  QFile f1(path1), f2(path2);
  if(!f1.exists() || !f2.exists()) return false;
  if(f1.size() != f2.size()) return false;
  if(!f1.open(QIODevice::ReadOnly)) return false;
  if(!f2.open(QIODevice::ReadOnly)) {
    f1.close();
    return false;
  }
  bool same = true;
  while(!f1.atEnd() && !f2.atEnd()) {
    if(f1.read(5) != f2.read(5)) {
      same = false;
      break;
    }
  }
  f1.close(); f2.close();
  return same;
}

void misc::copyDir(QString src_path, QString dst_path) {
  QDir sourceDir(src_path);
  if(!sourceDir.exists()) return;
  // Create destination directory
  QDir destDir(dst_path);
  if(!destDir.exists()) {
    if(!destDir.mkpath(destDir.absolutePath())) return;
  }
  // List source directory
  const QFileInfoList content = sourceDir.entryInfoList();
  foreach(const QFileInfo& child, content) {
    if(child.fileName()[0] == '.') continue;
    if(child.isDir()) {
      copyDir(child.absoluteFilePath(), dst_path+QDir::separator()+QDir(child.absoluteFilePath()).dirName());
      continue;
    }
    const QString src_child_path = child.absoluteFilePath();
    const QString dest_child_path = destDir.absoluteFilePath(child.fileName());
    // Copy the file from src to dest
    QFile::copy(src_child_path, dest_child_path);
    // Remove source file
    safeRemove(src_child_path);
  }
  // Remove source folder
  const QString dir_name = sourceDir.dirName();
  if(sourceDir.cdUp()) {
    sourceDir.rmdir(dir_name);
  }
}

void misc::chmod644(const QDir& folder) {
  qDebug("chmod644(%s)", qPrintable(folder.absolutePath()));
  if(!folder.exists()) return;
  foreach(const QFileInfo &fi, folder.entryInfoList(QDir::Dirs|QDir::Files|QDir::NoSymLinks)) {
    if(fi.fileName().startsWith(".")) continue;
    if(fi.isDir()) {
      misc::chmod644(QDir(fi.absoluteFilePath()));
    } else {
      QFile f(fi.absoluteFilePath());
      f.setPermissions(f.permissions()|QFile::ReadUser|QFile::WriteUser|QFile::ReadGroup|QFile::ReadOther);
    }
  }
}

QString misc::updateLabelInSavePath(const QString& defaultSavePath, const QString &save_path, const QString &old_label, const QString &new_label) {
  if(old_label == new_label) return save_path;
  qDebug("UpdateLabelInSavePath(%s, %s, %s)", qPrintable(save_path), qPrintable(old_label), qPrintable(new_label));
  if(!save_path.startsWith(defaultSavePath)) return save_path;
  QString new_save_path = save_path;
  new_save_path.replace(defaultSavePath, "");
  QStringList path_parts = new_save_path.split(QDir::separator(), QString::SkipEmptyParts);
  if(path_parts.empty()) {
    if(!new_label.isEmpty())
      path_parts << new_label;
  } else {
    if(old_label.isEmpty() || path_parts.first() != old_label) {
      if(path_parts.first() != new_label)
        path_parts.prepend(new_label);
    } else {
      if(new_label.isEmpty()) {
        path_parts.removeAt(0);
      } else {
        if(path_parts.first() != new_label)
          path_parts.replace(0, new_label);
      }
    }
  }
  new_save_path = defaultSavePath;
  if(!new_save_path.endsWith(QDir::separator())) new_save_path += QDir::separator();
  new_save_path += path_parts.join(QDir::separator());
  qDebug("New save path is %s", qPrintable(new_save_path));
  return new_save_path;
}

QString misc::toValidFileSystemName(QString filename) {
  qDebug("toValidFSName: %s", qPrintable(filename));
  filename = filename.replace("\\", "/").trimmed();
  const QRegExp regex("[/:?\"*<>|]");
  filename = filename.replace(regex, " ").trimmed();
  qDebug("toValidFSName, result: %s", qPrintable(filename));
  return filename;
}

bool misc::isValidFileSystemName(QString filename) {
  filename = filename.replace("\\", "/").trimmed();
  if(filename.isEmpty()) return false;
  const QRegExp regex("[/:?\"*<>|]");
  if(filename.contains(regex))
    return false;
  return true;
}

#ifndef DISABLE_GUI
// Get screen center
QPoint misc::screenCenter(QWidget *win) {
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

QString misc::searchEngineLocation() {
  const QString location = QDir::cleanPath(QDesktopServicesDataLocation()
                                           + QDir::separator() + "nova");
  QDir locationDir(location);
  if(!locationDir.exists())
    locationDir.mkpath(locationDir.absolutePath());
  return location;
}

QString misc::BTBackupLocation() {
  const QString location = QDir::cleanPath(QDesktopServicesDataLocation()
                                           + QDir::separator() + "BT_backup");
  QDir locationDir(location);
  if(!locationDir.exists())
    locationDir.mkpath(locationDir.absolutePath());
  return location;
}

QString misc::cacheLocation() {
  QString location = QDir::cleanPath(QDesktopServicesCacheLocation());
  QDir locationDir(location);
  if(!locationDir.exists())
    locationDir.mkpath(locationDir.absolutePath());
  return location;
}

// return best userfriendly storage unit (B, KiB, MiB, GiB, TiB)
// use Binary prefix standards from IEC 60027-2
// see http://en.wikipedia.org/wiki/Kilobyte
// value must be given in bytes
QString misc::friendlyUnit(double val) {
  if(val < 0)
    return tr("Unknown", "Unknown (size)");
  int i = 0;
  while(val >= 1024. && i++<6)
    val /= 1024.;
  if(i == 0)
    return QString::number((long)val) + " " + units[0];
  return QString::number(val, 'f', 1) + " " + units[i];
}

bool misc::isPreviewable(QString extension){
  if(extension.isEmpty()) return false;
  extension = extension.toUpper();
  if(extension == "AVI") return true;
  if(extension == "MP3") return true;
  if(extension == "OGG") return true;
  if(extension == "OGV") return true;
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
  if(extension == "M3U") return true;
  return false;
}

bool misc::removeEmptyTree(QString path) {
  QDir dir(path);
  foreach(const QString &child, dir.entryList(QDir::AllDirs)) {
    if(child == "." || child == "..") continue;
    return removeEmptyTree(dir.absoluteFilePath(child));
  }
  const QString dir_name = dir.dirName();
  if(dir.cdUp()) {
    return dir.rmdir(dir_name);
  }
  return false;
}

QString misc::bcLinkToMagnet(QString bc_link) {
  QByteArray raw_bc = bc_link.toUtf8();
  raw_bc = raw_bc.mid(8); // skip bc://bt/
  raw_bc = QByteArray::fromBase64(raw_bc); // Decode base64
  // Format is now AA/url_encoded_filename/size_bytes/info_hash/ZZ
  QStringList parts = QString(raw_bc).split("/");
  if(parts.size() != 5) return QString::null;
  QString filename = parts.at(1);
  QString hash = parts.at(3);
  QString magnet = "magnet:?xt=urn:btih:" + hash;
  magnet += "&dn=" + filename;
  return magnet;
}

QString misc::magnetUriToName(QString magnet_uri) {
  QString name = "";
  QRegExp regHex("dn=([^&]+)");
  const int pos = regHex.indexIn(magnet_uri);
  if(pos > -1) {
    const QString found = regHex.cap(1);
    // URL decode
    name = QUrl::fromPercentEncoding(found.toLocal8Bit()).replace("+", " ");
  }
  return name;
}

QString misc::magnetUriToHash(QString magnet_uri) {
  QString hash = "";
  QRegExp regHex("urn:btih:([0-9A-Za-z]+)");
  // Hex
  int pos = regHex.indexIn(magnet_uri);
  if(pos > -1) {
    const QString found = regHex.cap(1);
    if(found.length() == 40) {
      const sha1_hash sha1(QString(QByteArray::fromHex(regHex.cap(1).toLocal8Bit())).toStdString());
      qDebug("magnetUriToHash (Hex): hash: %s", qPrintable(misc::toQString(sha1)));
      return misc::toQString(sha1);
    }
  }
  // Base 32
  QRegExp regBase32("urn:btih:([A-Za-z2-7=]+)");
  pos = regBase32.indexIn(magnet_uri);
  if(pos > -1) {
    const QString found = regBase32.cap(1);
    if(found.length() > 20 && (found.length()*5)%40 == 0) {
      const sha1_hash sha1(base32decode(regBase32.cap(1).toStdString()));
      hash = misc::toQString(sha1);
    }
  }
  qDebug("magnetUriToHash (base32): hash: %s", qPrintable(hash));
  return hash;
}

QString misc::boostTimeToQString(const boost::optional<boost::posix_time::ptime> &boostDate) {
  if(!boostDate || !boostDate.is_initialized() || boostDate->is_not_a_date_time()) return tr("Unknown");
  struct std::tm tm;
  try {
    tm = boost::posix_time::to_tm(*boostDate);
  } catch(std::exception e) {
    return tr("Unknown");
  }
  const time_t t = mktime(&tm);
  if(t < 0)
    return tr("Unknown");
  const QDateTime dt = QDateTime::fromTime_t(t);
  if(dt.isNull() || !dt.isValid())
    return tr("Unknown");
  return dt.toString(Qt::DefaultLocaleLongDate);
}

QString misc::time_tToQString(const boost::optional<time_t> &t) {
  if(!t.is_initialized() || *t < 0) return tr("Unknown");
  QDateTime dt = QDateTime::fromTime_t(*t);
  if(dt.isNull() || !dt.isValid())
    return tr("Unknown");
  return dt.toString(Qt::DefaultLocaleLongDate);
}

// Replace ~ in path
QString misc::expandPath(QString path) {
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
QString misc::userFriendlyDuration(qlonglong seconds) {
  if(seconds < 0 || seconds >= MAX_ETA) {
    return QString::fromUtf8("∞");
  }
  if(seconds == 0) {
    return "0";
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
    return tr("%1h %2m", "e.g: 3hours 5minutes").arg(QString::number(hours)).arg(QString::number(minutes));
  }
  int days = hours / 24;
  hours = hours - days * 24;
  if(days < 100) {
    return tr("%1d %2h", "e.g: 2days 10hours").arg(QString::number(days)).arg(QString::number(hours));
  }
  return QString::fromUtf8("∞");
}

QString misc::getUserIDString() {
  QString uid = "0";
#ifdef Q_WS_WIN
  char buffer[UNLEN+1] = {0};
  DWORD buffer_len = UNLEN + 1;
  if (!GetUserNameA(buffer, &buffer_len))
    uid = QString(buffer);
#else
  uid = QString::number(getuid());
#endif
  return uid;
}

QStringList misc::toStringList(const QList<bool> &l) {
  QStringList ret;
  foreach(const bool &b, l) {
    ret << (b ? "1" : "0");
  }
  return ret;
}

QList<int> misc::intListfromStringList(const QStringList &l) {
  QList<int> ret;
  foreach(const QString &s, l) {
    ret << s.toInt();
  }
  return ret;
}

QList<bool> misc::boolListfromStringList(const QStringList &l) {
  QList<bool> ret;
  foreach(const QString &s, l) {
    ret << (s=="1");
  }
  return ret;
}

quint64 misc::computePathSize(QString path)
{
  // Check if it is a file
  QFileInfo fi(path);
  if(!fi.exists()) return 0;
  if(fi.isFile()) return fi.size();
  // Compute folder size
  quint64 size = 0;
  foreach(const QFileInfo &subfi, QDir(path).entryInfoList(QDir::Dirs|QDir::Files)) {
    if(subfi.fileName().startsWith(".")) continue;
    if(subfi.isDir())
      size += misc::computePathSize(subfi.absoluteFilePath());
    else
      size += subfi.size();
  }
  return size;
}

bool misc::isValidTorrentFile(const QString &torrent_path) {
  try {
    boost::intrusive_ptr<libtorrent::torrent_info> t = new torrent_info(torrent_path.toUtf8().constData());
    if(!t->is_valid() || t->num_files() == 0)
      throw std::exception();
  } catch(std::exception&) {
    return false;
  }
  return true;
}
