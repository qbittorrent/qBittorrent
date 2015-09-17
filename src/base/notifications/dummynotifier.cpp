#include "dummynotifier.h"

Notifications::DummyNotifier::DummyNotifier(QObject *parent)
    : Notifier(parent)
{
}

void Notifications::DummyNotifier::showNotification(const Notifications::Request&) const
{
}
