#include "dbusnotifier.h"
#include "./notifications.h"

DBusNotifier::DBusNotifier(QObject *parent): Notifier(parent)
{

}

void DBusNotifier::showNotification(Notifier::NotificationKind /*kind*/, const QString &title, const QString &message,
                                    const Notifier::Context */*context*/)
{
    org::freedesktop::Notifications notifications("org.freedesktop.Notifications",
                                                  "/org/freedesktop/Notifications",
                                                  QDBusConnection::sessionBus());
    QVariantMap hints;
    hints["desktop-entry"] = "qBittorrent";
    QDBusPendingReply<uint> reply = notifications.Notify("qBittorrent", 0, "qbittorrent", title,
                                                         message, QStringList(), hints, -1);
    reply.waitForFinished();
}

