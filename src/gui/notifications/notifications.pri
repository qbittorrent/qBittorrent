INCLUDEPATH += $$PWD

unix:!macx:dbus: include(qtnotify/qtnotify.pri)
!unix: include(systray/systraynotify.pri)

HEADERS += \
    $$PWD/notifier.h

SOURCES += \
    $$PWD/notifier.cpp
