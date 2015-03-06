#ifndef KNOTIFY_H
#define KNOTIFY_H

#include <QObject>
#include <QMap>
#include "notifier.h"

class KNotify : public Notifier
{
	Q_OBJECT
public:
    explicit KNotify(QWidget *mainWindow = 0);
    // Notifier interface
protected:

    virtual void showNotification(NotificationKind kind, const QString &title, const QString &message, const Context *context);
private slots:
    void notificationClosed();
    void torrentFinishedNotificationActivated(unsigned int actionId);
private:
    void showTorrentFinishedNotification(const QString &title, const QString &message, const Context *context);
    QWidget* m_mainWindow;
    QMap<QObject*, QTorrentHandle> m_activeNotifications;
};

#endif // KNOTIFY_H
