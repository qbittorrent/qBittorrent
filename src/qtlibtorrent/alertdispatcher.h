#ifndef ALERTDISPATCHER_H
#define ALERTDISPATCHER_H

#include <QObject>
#include <QMutex>
#include <QWaitCondition>
#include <QAtomicPointer>
#include <QSharedPointer>
#include <libtorrent/session.hpp>

class QAlertDispatcher : public QObject
{
  Q_OBJECT
  Q_DISABLE_COPY(QAlertDispatcher)

public:
  QAlertDispatcher(libtorrent::session *session, QObject* parent);
  ~QAlertDispatcher();

  void getPendingAlertsNoWait(std::deque<libtorrent::alert*>&);
  void getPendingAlerts(std::deque<libtorrent::alert*>&);

signals:
  void alertsReceived();

private:
  static void dispatch(QSharedPointer<QAtomicPointer<QAlertDispatcher> >,
                       std::auto_ptr<libtorrent::alert>);
  void enqueueToMainThread();

private slots:
  void deliverSignal();

private:
  libtorrent::session *session;
  QMutex alerts_mutex;
  QWaitCondition alerts_condvar;
  std::deque<libtorrent::alert*> alerts;
  QSharedPointer<QAtomicPointer<QAlertDispatcher> > current_tag;
  bool event_posted;
};

#endif // ALERTDISPATCHER_H
