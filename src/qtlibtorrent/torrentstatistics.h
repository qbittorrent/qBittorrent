#ifndef TORRENTSTATISTICS_H
#define TORRENTSTATISTICS_H

#include <QObject>
#include <QTimer>

class QBtSession;

class TorrentStatistics : QObject
{
  Q_OBJECT
  Q_DISABLE_COPY(TorrentStatistics)

public:
  TorrentStatistics(QBtSession* session, QObject* parent = 0);
  ~TorrentStatistics();

  quint64 getAlltimeDL() const;
  quint64 getAlltimeUL() const;

private slots:
  void gatherStats();

private:
  void saveStats() const;
  void loadStats();

private:
  QBtSession* m_session;
  // Will overflow at 15.9 EiB
  quint64 m_alltimeUL;
  quint64 m_alltimeDL;
  qint64 m_sessionUL;
  qint64 m_sessionDL;
  mutable qint64 m_lastWrite;
  mutable bool m_dirty;

  QTimer m_timer;
};

#endif // TORRENTSTATISTICS_H
