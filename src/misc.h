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

#include <libtorrent/version.hpp>
#include <libtorrent/torrent_info.hpp>
#include <libtorrent/torrent_handle.hpp>
#include <sstream>
#include <QString>
#include <QStringList>
#include <QThread>
#include <ctime>
#include <QPoint>
#include <QFile>
#include <QDir>
#ifndef DISABLE_GUI
#include <QIcon>
#endif

const qlonglong MAX_ETA = 8640000;

/*  Miscellaneaous functions that can be useful */
class misc : public QObject{
  Q_OBJECT

private:
  misc(); // Forbidden

public:
  static inline QString toQString(const std::string &str) {
    return QString::fromLocal8Bit(str.c_str());
  }

  static inline QString toQString(const char* str) {
    return QString::fromLocal8Bit(str);
  }

  static inline QString toQStringU(const std::string &str) {
    return QString::fromUtf8(str.c_str());
  }

  static inline QString toQStringU(const char* str) {
    return QString::fromUtf8(str);
  }

  static inline QString toQString(const libtorrent::sha1_hash &hash) {
    char out[41];
    to_hex((char const*)&hash[0], libtorrent::sha1_hash::size, out);
    return QString(out);
  }

  static void chmod644(const QDir& folder);

  static inline QString file_extension(const QString &filename) {
    QString extension;
    int point_index = filename.lastIndexOf(".");
    if(point_index >= 0) {
      extension = filename.mid(point_index+1);
    }
    return extension;
  }

#ifndef DISABLE_GUI
  static void shutdownComputer(bool sleep=false);
#endif

  static QString parseHtmlLinks(const QString &raw_text);

  static quint64 computePathSize(QString path);

  static QString truncateRootFolder(boost::intrusive_ptr<libtorrent::torrent_info> t);
  static QString truncateRootFolder(libtorrent::torrent_handle h);
  static QString fixFileNames(QString path);

  static QString updateLabelInSavePath(QString defaultSavePath, QString save_path, const QString &old_label, const QString &new_label);

  static bool sameFiles(const QString &path1, const QString &path2);
  static bool isUrl(const QString &s);
  static void copyDir(QString src_path, QString dst_path);
  static QString toValidFileSystemName(QString filename);
  static bool isValidFileSystemName(const QString& filename);

  /* Ported from Qt4 to drop dependency on QtGui */
  static QString QDesktopServicesDataLocation();
  static QString QDesktopServicesCacheLocation();
  static QString QDesktopServicesDownloadLocation();
  /* End of Qt4 code */

#ifndef DISABLE_GUI
  // Get screen center
  static QPoint screenCenter(QWidget *win);
#endif
  static int pythonVersion();
  static QString searchEngineLocation();
  static QString BTBackupLocation();
  static QString cacheLocation();
  static long long freeDiskSpaceOnPath(QString path);
  // return best userfriendly storage unit (B, KiB, MiB, GiB, TiB)
  // use Binary prefix standards from IEC 60027-2
  // see http://en.wikipedia.org/wiki/Kilobyte
  // value must be given in bytes
  static QString friendlyUnit(qreal val);
  static bool isPreviewable(QString extension);
  static QString branchPath(QString file_path, bool uses_slashes=false);
  static QString fileName(QString file_path);
  static QString magnetUriToName(QString magnet_uri);
  static QString magnetUriToHash(QString magnet_uri);
  static QString bcLinkToMagnet(QString bc_link);
  static QString time_tToQString(const boost::optional<time_t> &t);
  // Replace ~ in path
  static QString expandPath(QString path);
  // Take a number of seconds and return an user-friendly
  // time duration like "1d 2h 10m".
  static QString userFriendlyDuration(qlonglong seconds);
  static QString getUserIDString();

  // Convert functions
  static QStringList toStringList(const QList<bool> &l);
  static QList<int> intListfromStringList(const QStringList &l);
  static QList<bool> boolListfromStringList(const QStringList &l);

  static bool isValidTorrentFile(const QString &path);
};

//  Trick to get a portable sleep() function
class SleeperThread : public QThread {
public:
  static void msleep(unsigned long msecs)
  {
    QThread::msleep(msecs);
  }
};

#endif
