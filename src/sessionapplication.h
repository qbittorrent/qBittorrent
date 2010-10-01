#ifndef SESSIONAPPLICATION_H
#define SESSIONAPPLICATION_H

#ifdef Q_WS_MAC
#include "qmacapplication.h"
#else
#include "qtsingleapplication.h"
#endif

#include <QSessionManager>

class SessionApplication :
#ifdef Q_WS_MAC
    public QMacApplication
#else
    public QtSingleApplication
#endif
{
  Q_OBJECT

public:
  SessionApplication(const QString &id, int &argc, char **argv) :
#ifdef Q_WS_MAC
  QMacApplication(id, argc, argv)
#else
  QtSingleApplication(id, argc, argv)
#endif
  {}

  void commitData(QSessionManager & manager) {
    Q_UNUSED(manager);
    emit sessionIsShuttingDown();
  }

  signals:
    void sessionIsShuttingDown();
};

#endif // SESSIONAPPLICATION_H
