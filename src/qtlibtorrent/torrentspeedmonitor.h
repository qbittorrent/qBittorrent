#ifndef TORRENTSPEEDMONITOR_H
#define TORRENTSPEEDMONITOR_H

#include <QThread>
#include <QWaitCondition>
#include <QList>
#include <QHash>
#include <QMutex>
#include "qtorrenthandle.h"

class QBtSession;

class SpeedSample {
public:
  SpeedSample(){}
  void addSample(int s);
  float average() const;
  void clear();

private:
  static const int max_samples = 30;

private:
  QList<int> m_speedSamples;
};

class TorrentSpeedMonitor : public QThread
{
    Q_OBJECT

public:
    explicit TorrentSpeedMonitor(QBtSession* session);
    ~TorrentSpeedMonitor();
    qlonglong getETA(const QString &hash) const;

protected:
  void run();

signals:

private:
  void getSamples();

private slots:
  void removeSamples(const QString& hash);
  void removeSamples(const QTorrentHandle& h);

private:
  static const int sampling_interval = 1000; // 1s

private:
  bool m_abort;
  QWaitCondition m_abortCond;
  QHash<QString, SpeedSample> m_samples;
  mutable QMutex mutex;
  QBtSession *m_session;
};

#endif // TORRENTSPEEDMONITOR_H
