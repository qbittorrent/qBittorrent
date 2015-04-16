include(qtlibtorrent/qtlibtorrent.pri)

HEADERS += \
    $$PWD/misc.h \
    $$PWD/fs_utils.h \
    $$PWD/downloadthread.h \
    $$PWD/torrentpersistentdata.h \
    $$PWD/filesystemwatcher.h \
    $$PWD/scannedfoldersmodel.h \
    $$PWD/qinisettings.h \
    $$PWD/logger.h \
    $$PWD/preferences.h \
    $$PWD/qtracker.h \
    $$PWD/http/irequesthandler.h \
    $$PWD/http/connection.h \
    $$PWD/http/requestparser.h \
    $$PWD/http/responsegenerator.h \
    $$PWD/http/server.h \
    $$PWD/http/types.h \
    $$PWD/http/responsebuilder.h \
    $$PWD/net/dnsupdater.h \
    $$PWD/net/reverseresolution.h \
    $$PWD/net/smtp.h

SOURCES += \
    $$PWD/downloadthread.cpp \
    $$PWD/scannedfoldersmodel.cpp \
    $$PWD/torrentpersistentdata.cpp \
    $$PWD/filesystemwatcher.cpp \
    $$PWD/misc.cpp \
    $$PWD/fs_utils.cpp \
    $$PWD/logger.cpp \
    $$PWD/preferences.cpp \
    $$PWD/qtracker.cpp \
    $$PWD/http/connection.cpp \
    $$PWD/http/requestparser.cpp \
    $$PWD/http/responsegenerator.cpp \
    $$PWD/http/server.cpp \
    $$PWD/http/responsebuilder.cpp \
    $$PWD/net/dnsupdater.cpp \
    $$PWD/net/reverseresolution.cpp \
    $$PWD/net/smtp.cpp
