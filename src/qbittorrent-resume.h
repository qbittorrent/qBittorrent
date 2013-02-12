/*
 * Bittorrent Client using Qt4 and libtorrent.
 * Copyright (C) 2013  sledgehammer_999
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

#ifndef QBITTORRENTRESUME_H
#define QBITTORRENTRESUME_H

#include "qinisettings.h"

class Qbittorrent_resume : public QIniSettings {
  Q_DISABLE_COPY(Qbittorrent_resume)

public:
  Qbittorrent_resume() : QIniSettings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent-resume")) {
    qDebug() << "Qbittorrent_resume constructor";
  }

  QHash<QString, QVariant> getTorrentsTmp() const {
    return value(QString::fromUtf8("torrents-tmp")).toHash();
  }

  void setTorrentsTmp(const QHash<QString, QVariant> &data) {
    setValue(QString::fromUtf8("torrents-tmp"), data);
  }

  QHash<QString, QVariant> getTorrents() const {
    return value(QString::fromUtf8("torrents")).toHash();
  }

  void setTorrents(const QHash<QString, QVariant> &data) {
    setValue(QString::fromUtf8("torrents"), data);
  }
};
#endif // QBITTORRENTRESUME_H
