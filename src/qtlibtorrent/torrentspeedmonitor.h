/*
 * Bittorrent Client using Qt4 and libtorrent.
 * Copyright (C) 2011  Christophe Dumez
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

#ifndef TORRENTSPEEDMONITOR_H
#define TORRENTSPEEDMONITOR_H

#include <QString>
#include <QThread>
#include <QWaitCondition>
#include <QHash>
#include <QMutex>
#include "qtorrenthandle.h"

class QBtSession;
class SpeedSample;

class TorrentSpeedMonitor : public QThread
{
  Q_OBJECT

public:
  explicit TorrentSpeedMonitor(QBtSession* session);
  ~TorrentSpeedMonitor();
  qlonglong getETA(const QString &hash) const;
  quint64 getAlltimeDL() const;
  quint64 getAlltimeUL() const;

protected:
  void run();

private:
  void getSamples();
  void saveStats() const;
  void loadStats();

private slots:
  void removeSamples(const QString& hash);
  void removeSamples(const QTorrentHandle& h);

private:
  bool m_abort;
  QWaitCondition m_abortCond;
  QHash<QString, SpeedSample> m_samples;
  mutable QMutex m_mutex;
  QBtSession *m_session;
  // Will overflow at 15.9 EiB
  quint64 alltimeUL;
  quint64 alltimeDL;
  quint64 sessionUL;
  quint64 sessionDL;
};

#endif // TORRENTSPEEDMONITOR_H
