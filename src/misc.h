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
#include <QUrl>
#ifndef DISABLE_GUI
#include <QIcon>
#endif

const qlonglong MAX_ETA = 8640000;

/*  Miscellaneaous functions that can be useful */
namespace misc
{
  inline QString toQString(const std::string &str) {
    return QString::fromLocal8Bit(str.c_str());
  }

  inline QString toQString(const char* str) {
    return QString::fromLocal8Bit(str);
  }

  inline QString toQStringU(const std::string &str) {
    return QString::fromUtf8(str.c_str());
  }

  inline QString toQStringU(const char* str) {
    return QString::fromUtf8(str);
  }

  inline QString toQString(const libtorrent::sha1_hash &hash) {
    char out[41];
	 libtorrent::to_hex((char const*)&hash[0], libtorrent::sha1_hash::size, out);
    return QString(out);
  }

#ifndef DISABLE_GUI
  void shutdownComputer(bool sleep=false);
#endif

  QString parseHtmlLinks(const QString &raw_text);

  bool isUrl(const QString &s);

#ifndef DISABLE_GUI
  // Get screen center
  QPoint screenCenter(QWidget *win);
#endif
  int pythonVersion();
  // return best userfriendly storage unit (B, KiB, MiB, GiB, TiB)
  // use Binary prefix standards from IEC 60027-2
  // see http://en.wikipedia.org/wiki/Kilobyte
  // value must be given in bytes
  QString friendlyUnit(qreal val, bool is_speed = false);
  bool isPreviewable(const QString& extension);
  QString magnetUriToName(const QString& magnet_uri);
  QString magnetUriToHash(const QString& magnet_uri);
  QList<QUrl> magnetUriToTrackers(const QString& magnet_uri);
  QString bcLinkToMagnet(QString bc_link);
  // Take a number of seconds and return an user-friendly
  // time duration like "1d 2h 10m".
  QString userFriendlyDuration(qlonglong seconds);
  QString getUserIDString();

  // Convert functions
  QStringList toStringList(const QList<bool> &l);
  QList<int> intListfromStringList(const QStringList &l);
  QList<bool> boolListfromStringList(const QStringList &l);

  QString toQString(time_t t);
  QString accurateDoubleToString(const double &n, const int &precision);

#ifndef DISABLE_GUI
  bool naturalSort(QString left, QString right, bool& result);
#endif
}

//  Trick to get a portable sleep() function
class SleeperThread : public QThread {
public:
  static void msleep(unsigned long msecs)
  {
    QThread::msleep(msecs);
  }
};

#endif
