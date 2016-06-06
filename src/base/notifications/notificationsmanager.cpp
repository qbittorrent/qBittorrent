#include "notificationsmanager.h"

#include "base/settingsstorage.h"
#include "dummynotifier.h"
#include "eventsource.h"
#include "notificationrequest.h"

#include <QDebug>
#include <QUrl>
#include <QProcess>

#if (defined(Q_OS_UNIX) && !defined(Q_OS_MAC)) && defined(QT_DBUS_LIB)
#include "base/notifications/dbusnotifier.h"
#endif

namespace
{
#define SETTINGS_GROUP "Notifications/"
#define SETTINGS_KEY(name) SETTINGS_GROUP name
    // Notifications properties keys
    const QString KEY_NOTIFICATIONS_ENABLED = SETTINGS_KEY("Enabled");
}

Notifications::Manager *Notifications::Manager::m_instance = nullptr;

Notifications::Manager::Manager(Notifier *notifier, QObject *parent)
    : QObject {parent}
    , m_eventSource {new CompoundEventsSource()}
{
    resetNotifier(notifier);
}

void Notifications::Manager::setInstance(Notifications::Manager::this_type *ptr)
{
    m_instance = ptr;
}

Notifications::Manager::~Manager() = default;

Notifications::Manager &Notifications::Manager::instance()
{
    return *m_instance;
}

void Notifications::Manager::notificationActionTriggered(const Notifications::Request &request, const QString &actionId) const
{
    if (request.actions().contains(actionId))
        request.actions()[actionId].handler(request, actionId);
}

void Notifications::Manager::notificationClosed(const Notifications::Request &request, Notifications::CloseReason reason)
{
    Q_UNUSED(request)
    Q_UNUSED(reason)
}

bool Notifications::Manager::areNotificationsEnabled()
{
    return SettingsStorage::instance()->loadValue(KEY_NOTIFICATIONS_ENABLED, true).toBool();
}

void Notifications::Manager::setNotificationsEnabled(bool value)
{
    SettingsStorage::instance()->storeValue(KEY_NOTIFICATIONS_ENABLED, value);
}

void Notifications::Manager::openPath(const QString &path) const
{
#if defined Q_OS_MAC
    QProcess::startDetached(QLatin1String("open"), QStringList(path));
#elif defined(Q_OS_UNIX)
    QProcess::startDetached(QLatin1String("xdg-open"), QStringList(path));
#elif defined Q_OS_WIN
    QProcess::startDetached(QLatin1String("start"), QStringList(path));
#endif
}

Notifications::Notifier *Notifications::Manager::createNotifier()
{
#if (defined(Q_OS_UNIX) && !defined(Q_OS_MAC)) && defined(QT_DBUS_LIB)
    if (SettingsStorage::instance()->loadValue(KEY_NOTIFICATIONS_ENABLED).toBool())
        return new Notifications::DBusNotifier(nullptr);
#endif

    return new DummyNotifier(nullptr);
}

void Notifications::Manager::resetNotifier(Notifier *notifier)
{
    if (m_notifier)
        delete m_notifier.data();

    if (notifier) {
        m_notifier = QPointer<Notifier>(notifier);
    }
    else {
        m_notifier = QPointer<Notifier>(createNotifier());

        if (!m_notifier) {     // Ups...
            qDebug() << "createNotifier() returned null pointer. Setting dummy notifier object";
            m_notifier = QPointer<Notifier>(new DummyNotifier(this));
        }
    }

    m_notifier->setParent(this);
    connect(m_notifier.data(), SIGNAL(notificationActionTriggered(const Request&,const QString&)),
            this, SLOT(notificationActionTriggered(const Request&,const QString&)));
}

const Notifications::Notifier *Notifications::Manager::notifier() const
{
    return m_notifier.data();
}

namespace
{
    inline QString notificationKey(const std::string &id)
    {
        return QLatin1String(SETTINGS_GROUP) + QString::fromLatin1(id.c_str(), id.size());
    }
}

void Notifications::Manager::saveNotificationsState()
{
    const EventsSource::StatesList events = m_eventSource->eventsState();
    for (const auto & eventState: events)
        SettingsStorage::instance()->storeValue(notificationKey(eventState.first), eventState.second);
}

void Notifications::Manager::reloadNotificationsState()
{
    const EventsSource::StatesList events = m_eventSource->eventsState();
    for (const auto & eventState: events) {
        m_eventSource->enableEvent(
            eventState.first,
            SettingsStorage::instance()->loadValue(notificationKey(eventState.first),
                                                   eventState.second).toBool());
    }
}

void Notifications::Manager::setNotificationActive(const std::string &id, bool active)
{
    m_eventSource->enableEvent(id, active);
}

std::vector<Notifications::EventDescription> Notifications::Manager::supportedNotifications()
{
    return m_eventSource->supportedEvents();
}

void Notifications::Manager::addEventSource(Notifications::EventsSource *source)
{
    m_eventSource->addSource(source);
    const EventsSource::StatesList events = source->eventsState();
    for (const auto & eventState: events) {
        source->enableEvent(
            eventState.first,
            SettingsStorage::instance()->loadValue(notificationKey(eventState.first),
                                                   eventState.second).toBool());
    }
}

void Notifications::Manager::removeEventSource(Notifications::EventsSource *source)
{
    m_eventSource->removeSource(source);
}
