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

#include <QList>
#include <QDateTime>
#include <vector>

#include "qbtsession.h"
#include "misc.h"
#include "torrentspeedmonitor.h"
#include "qinisettings.h"

using namespace libtorrent;

template<class T> struct Sample {
  Sample(const T down = 0, const T up = 0) : download(down), upload(up) {}
  T download;
  T upload;
};

class SpeedSample {

public:
  SpeedSample() {}
  void addSample(int speedDL, int speedUL);
  Sample<qreal> average() const;
  void clear();

private:
  static const int max_samples = 30;

private:
  QList<Sample<int> > m_speedSamples;
};

TorrentSpeedMonitor::TorrentSpeedMonitor(QBtSession* session)
  : m_session(session)
{
  connect(m_session, SIGNAL(deletedTorrent(QString)), SLOT(removeSamples(QString)));
  connect(m_session, SIGNAL(pausedTorrent(QTorrentHandle)), SLOT(removeSamples(QTorrentHandle)));
  connect(m_session, SIGNAL(statsReceived(libtorrent::stats_alert)), SLOT(statsReceived(libtorrent::stats_alert)));
}

TorrentSpeedMonitor::~TorrentSpeedMonitor()
{}

void SpeedSample::addSample(int speedDL, int speedUL)
{
  m_speedSamples << Sample<int>(speedDL, speedUL);
  if (m_speedSamples.size() > max_samples)
    m_speedSamples.removeFirst();
}

Sample<qreal> SpeedSample::average() const
{
  if (m_speedSamples.empty())
    return Sample<qreal>();

  qlonglong sumDL = 0;
  qlonglong sumUL = 0;

  foreach (const Sample<int>& s, m_speedSamples) {
    sumDL += s.download;
    sumUL += s.upload;
  }

  const qreal numSamples = m_speedSamples.size();
  return Sample<qreal>(sumDL/numSamples, sumUL/numSamples);
}

void SpeedSample::clear()
{
  m_speedSamples.clear();
}

void TorrentSpeedMonitor::removeSamples(const QString &hash)
{
  m_samples.remove(hash);
}

void TorrentSpeedMonitor::removeSamples(const QTorrentHandle& h) {
  try {
    m_samples.remove(h.hash());
  } catch(invalid_handle&) {}
}

qlonglong TorrentSpeedMonitor::getETA(const QString &hash, const libtorrent::torrent_status &status) const
{
  if (QTorrentHandle::is_paused(status) || !m_samples.contains(hash))
    return MAX_ETA;

  const Sample<qreal> speed_average = m_samples[hash].average();

  if (QTorrentHandle::is_seed(status)) {
    if (!speed_average.upload)
      return MAX_ETA;

    bool _unused;
    qreal max_ratio = m_session->getMaxRatioPerTorrent(hash, &_unused);
    if (max_ratio < 0)
      return MAX_ETA;

    libtorrent::size_type realDL = status.all_time_download;
    if (realDL <= 0)
      realDL = status.total_wanted;

    return (realDL * max_ratio - status.all_time_upload) / speed_average.upload;
  }

  if (!speed_average.download)
    return MAX_ETA;

  return (status.total_wanted - status.total_wanted_done) / speed_average.download;
}

void TorrentSpeedMonitor::statsReceived(const stats_alert &stats)
{
  m_samples[misc::toQString(stats.handle.info_hash())].addSample(stats.transferred[stats_alert::download_payload] * 1000 / stats.interval,
                                                                 stats.transferred[stats_alert::upload_payload] * 1000 / stats.interval);
}
