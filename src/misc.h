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
#include <ctime>
#include <QDateTime>
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <boost/date_time/posix_time/conversion.hpp>

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
  // Convert any type of variable to C++ String
  // convert=true will convert -1 to 0
  template <class T> static std::string toString(const T& x, bool convert=false) {
    std::ostringstream o;
    if(!(o<<x)) {
      throw std::runtime_error("::toString()");
    }
    if(o.str() == "-1" && convert)
      return "0";
    return o.str();
  }

  template <class T> static QString toQString(const T& x, bool convert=false) {
    std::ostringstream o;
    if(!(o<<x)) {
      throw std::runtime_error("::toString()");
    }
    if(o.str() == "-1" && convert)
      return QString::fromUtf8("0");
    return QString::fromUtf8(o.str().c_str());
  }

  template <class T> static QByteArray toQByteArray(const T& x, bool convert=false) {
    std::ostringstream o;
    if(!(o<<x)) {
      throw std::runtime_error("::toString()");
    }
    if(o.str() == "-1" && convert)
      return "0";
    return QByteArray(o.str().c_str());
  }

  // Convert C++ string to any type of variable
  template <class T> static T fromString(const std::string& s) {
    T x;
    std::istringstream i(s);
    if(!(i>>x)) {
      throw std::runtime_error("::fromString()");
    }
    return x;
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
    int ret = statfs ((dir_path.path()+"/.").toLocal8Bit().data(), &stats) ;
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
    while(val > 1024. && i++<6)
      val /= 1024.;
    return QString(QByteArray::number(val, 'f', 1)) + " " + units[(int)i];
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

  // return qBittorrent config path
  static QString qBittorrentPath() {
    QString qBtPath = QDir::homePath()+QDir::separator()+QString::fromUtf8(".qbittorrent") + QDir::separator();
    return qBtPath;
  }

  // Insertion sort, used instead of bubble sort because it is
  // approx. 5 times faster.
  template <class T> static void insertSort(QList<QPair<int, T> > &list, const QPair<int, T>& value, Qt::SortOrder sortOrder) {
    int i = 0;
    if(sortOrder == Qt::AscendingOrder) {
      while(i < list.size() and value.second > list.at(i).second) {
        ++i;
      }
    }else{
      while(i < list.size() and value.second < list.at(i).second) {
        ++i;
      }
    }
    list.insert(i, value);
  }

  template <class T> static void insertSort2(QList<QPair<int, T> > &list, const QPair<int, T>& value, Qt::SortOrder sortOrder=Qt::AscendingOrder) {
    int i = 0;
    if(sortOrder == Qt::AscendingOrder) {
      while(i < list.size() and value.first > list.at(i).first) {
        ++i;
      }
    }else{
      while(i < list.size() and value.first < list.at(i).first) {
        ++i;
      }
    }
    list.insert(i, value);
  }

  // Can't use template class for QString because >,< use unicode code for sorting
  // which is not what a human would expect when sorting strings.
  static void insertSortString(QList<QPair<int, QString> > &list, QPair<int, QString> value, Qt::SortOrder sortOrder) {
    int i = 0;
    if(sortOrder == Qt::AscendingOrder) {
      while(i < list.size() and QString::localeAwareCompare(value.second, list.at(i).second) > 0) {
        ++i;
      }
    }else{
      while(i < list.size() and QString::localeAwareCompare(value.second, list.at(i).second) < 0) {
        ++i;
      }
    }
    list.insert(i, value);
  }

  static QString magnetUriToHash(QString magnet_uri) {
    QString hash = "";
    QRegExp regHex("urn:btih:([0-9A-Za-z]+)");
    // Hex
    int pos = regHex.indexIn(magnet_uri);
    if(pos > -1) {
      QString found = regHex.cap(1);
      if(found.length() == 40) {
        sha1_hash sha1;
        sha1.assign(QString(QByteArray::fromHex(regHex.cap(1).toLocal8Bit())).toStdString());
        qDebug("magnetUriToHash (Hex): hash: %s", misc::toString(sha1).c_str());
        return misc::toQString(sha1);
      }
    }
    // Base 32
    QRegExp regBase32("urn:btih:([A-Za-z2-7=]+)");
    pos = regBase32.indexIn(magnet_uri);
    if(pos > -1) {
      QString found = regBase32.cap(1);
      if(found.length() > 20 && (found.length()*5)%40 == 0) {
        sha1_hash sha1;
        sha1.assign(base32decode(regBase32.cap(1).toStdString()));
        hash = misc::toQString(sha1);
      }
    }
    qDebug("magnetUriToHash (base32): hash: %s", hash.toLocal8Bit().data());
    return hash;
  }

  static QString boostTimeToQString(boost::optional<boost::posix_time::ptime> boostDate) {
    if(!boostDate) return tr("Unknown");
    struct std::tm tm = boost::posix_time::to_tm(*boostDate);
    return QDateTime::fromTime_t(mktime(&tm)).toString(Qt::DefaultLocaleLongDate);
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
      return tr("%1m","e.g: 10minutes").arg(QString::QString::fromUtf8(misc::toString(minutes).c_str()));
    }
    int hours = minutes / 60;
    minutes = minutes - hours*60;
    if(hours < 24) {
      return tr("%1h%2m", "e.g: 3hours 5minutes").arg(QString::fromUtf8(misc::toString(hours).c_str())).arg(QString::fromUtf8(misc::toString(minutes).c_str()));
    }
    int days = hours / 24;
    hours = hours - days * 24;
    if(days < 100) {
      return tr("%1d%2h%3m", "e.g: 2days 10hours 2minutes").arg(QString::fromUtf8(misc::toString(days).c_str())).arg(QString::fromUtf8(misc::toString(hours).c_str())).arg(QString::fromUtf8(misc::toString(minutes).c_str()));
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
