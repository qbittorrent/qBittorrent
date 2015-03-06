#ifndef NOTIFIER_H
#define NOTIFIER_H

#include <QObject>
#include <QSystemTrayIcon>

#include "qtorrenthandle.h"

class Notifier : public QObject
{
public:
    explicit Notifier(QWidget *mainWindow);

    enum NotificationKind {
        NoType = 0,
        TorrentFinished = 1,
        TorrentIOError = 2,
        UrlDownloadError = 3,
        SearchFinished = 4
    };

    struct Context {
        Context(QWidget* w = 0L, const QTorrentHandle& t = QTorrentHandle())
            :widget(w), torrent(t) {
        }

        QWidget* widget;
        QTorrentHandle torrent;
    };

    virtual void showNotification(NotificationKind kind, const QString& title, const QString& message, const Context* context) = 0;

    // These helpers follow specific events to the general showNotification()
    void notifyTorrentFinished(const QTorrentHandle& h, QWidget* widget = 0L);
    void notifyTorrentIOError(const QTorrentHandle& h, const QString& message, QWidget* widget = 0L);
    void notifyUrlDownloadError(const QString& url, const QString& reason, QWidget* widget = 0L);
    void notifySearchFinished(QWidget* widget);
    void showNotification(const QString& title, const QString& message, QWidget* widget = 0L);
};

Notifier* createNotifier(QWidget* mainWindow, QSystemTrayIcon* systrayIcon);

#endif // NOTIFIER_H
