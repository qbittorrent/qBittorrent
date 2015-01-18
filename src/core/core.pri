INCLUDEPATH += $$PWD

unix:!macx:dbus: include(qtnotify/qtnotify.pri)

include(qtlibtorrent/qtlibtorrent.pri)
include(tracker/tracker.pri)

HEADERS += \
    $$PWD/misc.h \
    $$PWD/fs_utils.h \
    $$PWD/downloadthread.h \
    $$PWD/torrentpersistentdata.h \
    $$PWD/filesystemwatcher.h \
    $$PWD/scannedfoldersmodel.h \
    $$PWD/qinisettings.h \
    $$PWD/smtp.h \
    $$PWD/dnsupdater.h \
    $$PWD/logger.h \
    $$PWD/httptypes.h \
    $$PWD/httprequestparser.h \
    $$PWD/httpresponsegenerator.h \
    $$PWD/preferences.h

SOURCES += \
    $$PWD/downloadthread.cpp \
    $$PWD/scannedfoldersmodel.cpp \
    $$PWD/torrentpersistentdata.cpp \
    $$PWD/misc.cpp \
    $$PWD/fs_utils.cpp \
    $$PWD/smtp.cpp \
    $$PWD/dnsupdater.cpp \
    $$PWD/logger.cpp \
    $$PWD/httprequestparser.cpp \
    $$PWD/httpresponsegenerator.cpp \
    $$PWD/preferences.cpp
