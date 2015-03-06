!qtHaveModule(KNotifications):error("Module KNotifications does not exist")
QT += KNotifications

notificationschemes.path = $$PREFIX/share/knotifications5
notificationschemes.files = $$PWD/*.notifyrc

INSTALLS += notificationschemes

HEADERS += \
    $$PWD/knotify.h

SOURCES += \
    $$PWD/knotify.cpp
