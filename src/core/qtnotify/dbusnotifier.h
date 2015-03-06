#ifndef DBUSNOTIFIER_H
#define DBUSNOTIFIER_H

#include "notifier.h"

class DBusNotifier : public Notifier
{
public:
    DBusNotifier(QObject* parent);

    // Notifier interface
public:


    // Notifier interface
public:
    virtual void showNotification(NotificationKind kind, const QString &title, const QString &message, const Context *context);
};

#endif // DBUSNOTIFIER_H
