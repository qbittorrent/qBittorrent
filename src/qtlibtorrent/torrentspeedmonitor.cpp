#include <QMutexLocker>
#include "torrentspeedmonitor.h"
#include "qbtsession.h"
#include "misc.h"

using namespace libtorrent;

TorrentSpeedMonitor::TorrentSpeedMonitor(QBtSession* session) :
  QThread(session), m_abort(false), m_session(session)
{
  connect(m_session, SIGNAL(deletedTorrent(QString)), SLOT(removeSamples(QString)));
  connect(m_session, SIGNAL(pausedTorrent(QTorrentHandle)), SLOT(removeSamples(QTorrentHandle)));
}

TorrentSpeedMonitor::~TorrentSpeedMonitor() {
  m_abort = true;
  m_abortCond.wakeOne();
  wait();
}

void TorrentSpeedMonitor::run()
{
  do {
    mutex.lock();
    getSamples();
    m_abortCond.wait(&mutex, 1000);
    mutex.unlock();
  } while(!m_abort);
}

void SpeedSample::addSample(int s)
{
  m_speedSamples << s;
  if(m_speedSamples.size() > max_samples)
    m_speedSamples.removeFirst();
}

float SpeedSample::average() const
{
  if(m_speedSamples.empty()) return 0;
  qlonglong sum = 0;
  foreach (int s, m_speedSamples) {
    sum += s;
  }
  return sum/static_cast<float>(m_speedSamples.size());
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
  } catch(invalid_handle&){}
}

qlonglong TorrentSpeedMonitor::getETA(const QString &hash) const
{
  QMutexLocker locker(&mutex);
  QTorrentHandle h = m_session->getTorrentHandle(hash);
  if(h.is_paused() || !m_samples.contains(hash)) return -1;
  const float speed_average = m_samples.value(hash).average();
  if(speed_average == 0) return -1;
  return (h.total_wanted() - h.total_done()) / speed_average;
}

void TorrentSpeedMonitor::getSamples()
{
  const std::vector<torrent_handle> torrents = m_session->getSession()->get_torrents();
  std::vector<torrent_handle>::const_iterator it;
  for(it = torrents.begin(); it != torrents.end(); it++) {
    try {
      if(!it->is_paused())
        m_samples[misc::toQString(it->info_hash())].addSample(it->status().download_payload_rate);
    } catch(invalid_handle&){}
  }
}
