#ifndef DUMMYNOTIFIER_H
#define DUMMYNOTIFIER_H

#include "notifier.h"

namespace Notifications
{
    class DummyNotifier: public Notifier
    {
    public:
        DummyNotifier(QObject* parent);
        // Notifier interface
        void showNotification(const Request &) const override;
    };
}

#endif // DUMMYNOTIFIER_H
