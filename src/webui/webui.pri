HEADERS += \
    $$PWD/api/apicontroller.h \
    $$PWD/api/apierror.h \
    $$PWD/api/appcontroller.h \
    $$PWD/api/authcontroller.h \
    $$PWD/api/freediskspacechecker.h \
    $$PWD/api/isessionmanager.h \
    $$PWD/api/logcontroller.h \
    $$PWD/api/rsscontroller.h \
    $$PWD/api/searchcontroller.h \
    $$PWD/api/synccontroller.h \
    $$PWD/api/torrentscontroller.h \
    $$PWD/api/transfercontroller.h \
    $$PWD/api/serialize/serialize_torrent.h \
    $$PWD/webapplication.h \
    $$PWD/webui.h

SOURCES += \
    $$PWD/api/apicontroller.cpp \
    $$PWD/api/apierror.cpp \
    $$PWD/api/appcontroller.cpp \
    $$PWD/api/authcontroller.cpp \
    $$PWD/api/freediskspacechecker.cpp \
    $$PWD/api/logcontroller.cpp \
    $$PWD/api/rsscontroller.cpp \
    $$PWD/api/searchcontroller.cpp \
    $$PWD/api/synccontroller.cpp \
    $$PWD/api/torrentscontroller.cpp \
    $$PWD/api/transfercontroller.cpp \
    $$PWD/api/serialize/serialize_torrent.cpp \
    $$PWD/webapplication.cpp \
    $$PWD/webui.cpp

TS_SOURCES += $$files($$PWD/www/translations/webui_*.ts)

RESOURCES += $$PWD/www/webui.qrc $$PWD/www/translations/webui_translations.qrc
