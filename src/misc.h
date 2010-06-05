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
#include <QString>
#include <QThread>
#include <ctime>
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <boost/date_time/posix_time/conversion.hpp>
#include <QPoint>

#include <libtorrent/torrent_info.hpp>
const qlonglong MAX_ETA = 8640000;

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

  static inline QString toQStringU(std::string str) {
    return QString::fromUtf8(str.c_str());
  }

  static inline QString toQStringU(char* str) {
    return QString::fromUtf8(str);
  }

  static inline QString toQString(sha1_hash hash) {
    std::ostringstream o;
    o << hash;
    return QString(o.str().c_str());
  }

  static inline sha1_hash QStringToSha1(const QString& s) {
    std::istringstream i(s.toStdString());
    sha1_hash x;
    i>>x;
    return x;
  }

  static bool sameFiles(QString path1, QString path2);
  static void copyDir(QString src_path, QString dst_path);
  // Introduced in v2.1.0 for backward compatibility
  // Remove after some releases
  static void moveToXDGFolders();
  static QString toValidFileSystemName(QString filename);
  static bool isValidFileSystemName(QString filename);

  /* Ported from Qt4 to drop dependency on QtGui */
  static QString QDesktopServicesDataLocation();
  static QString QDesktopServicesCacheLocation();
  /* End of Qt4 code */

#ifndef DISABLE_GUI
  // Get screen center
  static QPoint screenCenter(QWidget *win);
#endif
  static QString searchEngineLocation();
  static QString BTBackupLocation();
  static QString cacheLocation();
  static long long freeDiskSpaceOnPath(QString path);
  // return best userfriendly storage unit (B, KiB, MiB, GiB, TiB)
  // use Binary prefix standards from IEC 60027-2
  // see http://en.wikipedia.org/wiki/Kilobyte
  // value must be given in bytes
  static QString friendlyUnit(double val);
  static bool isPreviewable(QString extension);
  static bool removeEmptyTree(QString path);
  static QString magnetUriToName(QString magnet_uri);
  static QString magnetUriToHash(QString magnet_uri);
  static QString boostTimeToQString(const boost::optional<boost::posix_time::ptime> boostDate);
  // Replace ~ in path
  static QString expandPath(QString path);
  // Take a number of seconds and return an user-friendly
  // time duration like "1d 2h 10m".
  static QString userFriendlyDuration(qlonglong seconds);

  // Convert functions
  static QStringList toStringList(const QList<bool> &l);
  static QList<int> intListfromStringList(const QStringList &l);
  static QList<bool> boolListfromStringList(const QStringList &l);
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
