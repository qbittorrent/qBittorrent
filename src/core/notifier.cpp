#include "notifier.h"
#include <QWidget>

#ifdef ENABLE_KF5
    #include "knotify/knotify.h"
#else
#if (defined(Q_OS_UNIX) && !defined(Q_OS_MAC)) && defined(QT_DBUS_LIB)
    #include <QDBusConnection>
    #include "qtnotify/dbusnotifier.h"
#else

#define TIME_TRAY_BALLOON 5000
class SystemTrayNotifier : public Notifier {
public:
    SystemTrayNotifier(QObject* parent, QSystemTrayIcon* systrayIcon)
        :Notifier(parent),
         m_systrayIcon(QSystemTrayIcon::supportsMessages() ? systrayIcon : 0) {
    }

    // Notifier interface
protected:
    void showNotification(NotificationKind kind, const QString& title, const QString& message, const Context* context);
private:
    QSystemTrayIcon* m_systrayIcon;
};

void SystemTrayNotifier::showNotification(NotificationKind kind, const QString& title, const QString& message, const Context* /*context*/)
{
    QSystemTrayIcon::MessageIcon icon;
    switch(kind)
    {
    case Notifier::UrlDownloadError:
        icon = QSystemTrayIcon::Warning;
        break;
    case Notifier::TorrentIOError:
        icon = QSystemTrayIcon::Critical;
        break;
    default:
        icon = QSystemTrayIcon::Information;
        break;
    }

    if (m_systrayIcon) {
        m_systrayIcon->showMessage(title, message, icon, TIME_TRAY_BALLOON);
    }
}

#endif
#endif



Notifier::Notifier(QWidget *mainWindow)
    : QObject(mainWindow)
{

}

void Notifier::notifyTorrentFinished(const QTorrentHandle &h, QWidget *widget)
{
    Context ct(widget, h);
    showNotification(Notifier::TorrentFinished, tr("Download completion"),
                     tr("%1 has finished downloading.", "e.g: xxx.avi has finished downloading.").arg(h.name()), &ct);
}

void Notifier::notifyTorrentIOError(const QTorrentHandle &h, const QString &message, QWidget *widget)
{
    Context ct(widget);
    showNotification(Notifier::TorrentIOError, tr("I/O Error", "i.e: Input/Output Error"),
                     tr("An I/O error occurred for torrent %1.\n Reason: %2",
                        "e.g: An error occurred for torrent xxx.avi.\n Reason: disk is full.").arg(h.name()).arg(message), &ct);
}

void Notifier::notifyUrlDownloadError(const QString &url, const QString &reason, QWidget *widget)
{
    Context ct(widget);
    showNotification(Notifier::UrlDownloadError, tr("Url download error"),
                     tr("Couldn't download file at url: %1, reason: %2.").arg(url).arg(reason), &ct);
}

void Notifier::notifySearchFinished(QWidget *widget)
{
    Context ct(widget);
    showNotification(Notifier::SearchFinished, tr("Search Engine"), tr("Search has finished"), &ct);
}

void Notifier::showNotification(const QString &title, const QString &message, QWidget *widget)
{
    Context ct(widget);
    showNotification(Notifier::NoType, title, message, &ct);
}


Notifier *createNotifier(QWidget *mainWindow, QSystemTrayIcon *systrayIcon)
{
#ifdef ENABLE_KF5
    Q_UNUSED(systrayIcon)
    return new KNotify(mainWindow);
#else
#if (defined(Q_OS_UNIX) && !defined(Q_OS_MAC)) && defined(QT_DBUS_LIB)
    Q_UNUSED(systrayIcon)
    return new DBusNotifier(parent);
#else
    return new SystemTrayNotifier(parent, systrayIcon);
#endif
#endif
}
