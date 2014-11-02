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

#include "qbtsession.h"
#include "misc.h"
#include "torrentspeedmonitor.h"

using namespace libtorrent;

namespace {

  template<class T> struct Sample {
    Sample()
      : download()
      , upload()
    {}

    Sample(T download, T upload)
      : download(download)
      , upload(upload)
    {}

    template <typename U>
    explicit Sample(Sample<U> other)
      : download(static_cast<U>(other.download))
      , upload(static_cast<U>(other.upload))
    {}

    T download;
    T upload;
  };

  template <typename T>
  Sample<T>& operator+=(Sample<T>& lhs, Sample<T> const& rhs) {
    lhs.download += rhs.download;
    lhs.upload   += rhs.upload;
    return lhs;
  }

  template <typename T>
  Sample<T>& operator-=(Sample<T>& lhs, Sample<T> const& rhs) {
    lhs.download -= rhs.download;
    lhs.upload   -= rhs.upload;
    return lhs;
  }

  template <typename T>
  Sample<T> operator+(Sample<T> const& lhs, Sample<T> const& rhs) {
    return Sample<T>(lhs.download + rhs.download, lhs.upload + rhs.upload);
  }

  template <typename T>
  Sample<T> operator-(Sample<T> const& lhs, Sample<T> const& rhs) {
    return Sample<T>(lhs.download - rhs.download, lhs.upload - rhs.upload);
  }

  template <typename T>
  Sample<T> operator*(Sample<T> const& lhs, T rhs) {
    return Sample<T>(lhs.download * rhs, lhs.upload * rhs);
  }

  template <typename T>
  Sample<T> operator*(T lhs,Sample<T> const& rhs) {
    return Sample<T>(lhs * rhs.download, lhs * rhs.upload);
  }

  template <typename T>
  Sample<T> operator/(Sample<T> const& lhs, T rhs) {
    return Sample<T>(lhs.download / rhs, lhs.upload / rhs);
  }
}

class SpeedSample {

public:
  SpeedSample() {}
  void addSample(Sample<int> const& item);
  Sample<qreal> average() const;

private:
  static const int max_samples = 30;

private:
  QList<Sample<int> > m_speedSamples;
  Sample<long long> m_sum;
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

void SpeedSample::addSample(Sample<int> const& item)
{
  m_speedSamples.push_back(item);
  m_sum += Sample<long long>(item);
  if (m_speedSamples.size() > max_samples) {
    m_sum -= Sample<long long>(m_speedSamples.front());
    m_speedSamples.pop_front();
  }
}

Sample<qreal> SpeedSample::average() const
{
  if (m_speedSamples.empty())
    return Sample<qreal>();

  return Sample<qreal>(m_sum) * (1. / m_speedSamples.size());
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
  if (QTorrentHandle::is_paused(status))
    return MAX_ETA;

  QHash<QString, SpeedSample>::const_iterator i = m_samples.find(hash);
  if (i == m_samples.end())
    return MAX_ETA;

  const Sample<qreal> speed_average = i->average();

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
  Q_ASSERT(stats.interval >= 1000);

  Sample<int> transferred(stats.transferred[stats_alert::download_payload],
                          stats.transferred[stats_alert::upload_payload]);

  Sample<int> normalized = Sample<int>(Sample<long long>(transferred) * 1000LL / static_cast<long long>(stats.interval));

  m_samples[misc::toQString(stats.handle.info_hash())].addSample(normalized);
}
