#include "knotify.h"
#include <KNotification>
#include <QDesktopServices>
#include <QDir>
#include <QUrl>

KNotify::KNotify(QWidget *mainWindow)
    : Notifier(mainWindow),
      m_mainWindow(mainWindow)
{
}

void KNotify::showNotification(NotificationKind kind, const QString &title, const QString &message, const Context *context)
{
    QWidget* contextWidget = context && context->widget ? context->widget : m_mainWindow;
    switch(kind)
    {
    case Notifier::TorrentFinished:
        showTorrentFinishedNotification(title, message, context);
        break;
    case Notifier::TorrentIOError:
        KNotification::event("ioerror", title, message,
                             QString(), contextWidget, KNotification::Persistent);
        break;
    case Notifier::UrlDownloadError:
        KNotification::event("urlerror", title, message,
                             QString(), contextWidget, KNotification::Persistent);
        break;
    case Notifier::SearchFinished:
         KNotification::event("searchfinished", title, message, QString(),
                              contextWidget, contextWidget ? KNotification::CloseWhenWidgetActivated : KNotification::CloseOnTimeout);
         break;
    default:
        KNotification::event(KNotification::Notification, title, message, QString(), contextWidget);
        break;
    }
}

void KNotify::showTorrentFinishedNotification(const QString &title, const QString &message, const Context *context)
{
    KNotification* notification = new KNotification("downloadfinished",
                           KNotification::Persistent /*CloseOnTimeout?*/, this);
    notification->setTitle(title);
    notification->setText(message);

    if(context) {
        if(context->widget) {
            notification->setWidget(context->widget);
        }
        m_activeNotifications.insert(notification, context->torrent);
        connect(notification, SIGNAL(closed()), this, SLOT(notificationClosed()));
        connect(notification, SIGNAL(activated(unsigned )), this, SLOT(torrentFinishedNotificationActivated(unsigned )));
    }

    notification->sendEvent();
}

void KNotify::notificationClosed()
{
    m_activeNotifications.remove(sender());
}

void KNotify::torrentFinishedNotificationActivated(unsigned int /*actionId*/)
{
    if(m_activeNotifications.contains(sender())) { // the map should not be long
        QTorrentHandle h = m_activeNotifications.value(sender());
        // if there is only a single file in the torrent, we open it
        if(h.num_files() == 1) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(
                         QDir(h.save_path()).absoluteFilePath(h.filepath_at(0))));
        } else { // otherwise we open top directory
            QDesktopServices::openUrl(QUrl::fromLocalFile(h.root_path()));
        }
    }
}
