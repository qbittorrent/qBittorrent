#include "torrentstatistics.h"

#include <QDateTime>

#include <libtorrent/session.hpp>

#include "qbtsession.h"
#include "qinisettings.h"

TorrentStatistics::TorrentStatistics(QBtSession* session, QObject* parent)
  : QObject(parent)
  , m_session(session)
  , m_sessionUL(0)
  , m_sessionDL(0)
  , m_lastWrite(0)
  , m_dirty(false)
{
  loadStats();
  connect(&m_timer, SIGNAL(timeout()), this, SLOT(gatherStats()));
  m_timer.start(60 * 1000);
}

TorrentStatistics::~TorrentStatistics() {
  if (m_dirty)
    m_lastWrite = 0;
  saveStats();
}

quint64 TorrentStatistics::getAlltimeDL() const {
  return m_alltimeDL + m_sessionDL;
}

quint64 TorrentStatistics::getAlltimeUL() const {
  return m_alltimeUL + m_sessionUL;
}

void TorrentStatistics::gatherStats() {
  libtorrent::session_status ss = m_session->getSessionStatus();
  if (ss.total_download > m_sessionDL) {
    m_sessionDL = ss.total_download;
    m_dirty = true;
  }
  if (ss.total_upload > m_sessionUL) {
    m_sessionUL = ss.total_upload;
    m_dirty = true;
  }

  saveStats();
}

void TorrentStatistics::saveStats() const {
  if (!(m_dirty && (QDateTime::currentMSecsSinceEpoch() - m_lastWrite >= 15*60*1000) ))
    return;
  QIniSettings s("qBittorrent", "qBittorrent-data");
  QVariantHash v;
  v.insert("AlltimeDL", m_alltimeDL + m_sessionDL);
  v.insert("AlltimeUL", m_alltimeUL + m_sessionUL);
  s.setValue("Stats/AllStats", v);
  m_dirty = false;
  m_lastWrite = QDateTime::currentMSecsSinceEpoch();
}

void TorrentStatistics::loadStats() {
  // Temp code. Versions v3.1.4 and v3.1.5 saved the data in the qbittorrent.ini file.
  // This code reads the data from there, writes it to the new file, and removes the keys
  // from the old file. This code should be removed after some time has passed.
  // e.g. When we reach v3.3.0
  QIniSettings s_old;
  QIniSettings s("qBittorrent", "qBittorrent-data");
  QVariantHash v;

  // Let's test if the qbittorrent.ini holds the key
  if (s_old.contains("Stats/AllStats")) {
    v = s_old.value("Stats/AllStats").toHash();
    m_dirty = true;

    // If the user has used qbt > 3.1.5 and then reinstalled/used
    // qbt < 3.1.6, there will be stats in qbittorrent-data.ini too
    // so we need to merge those 2.
    if (s.contains("Stats/AllStats")) {
      QVariantHash tmp = s.value("Stats/AllStats").toHash();
      v["AlltimeDL"] = v["AlltimeDL"].toULongLong() + tmp["AlltimeDL"].toULongLong();
      v["AlltimeUL"] = v["AlltimeUL"].toULongLong() + tmp["AlltimeUL"].toULongLong();
    }
  }
  else
    v = s.value("Stats/AllStats").toHash();

  m_alltimeDL = v["AlltimeDL"].toULongLong();
  m_alltimeUL = v["AlltimeUL"].toULongLong();

  if (m_dirty) {
    saveStats();
    s_old.remove("Stats/AllStats");
  }
}
