INCLUDEPATH += $$PWD

HEADERS += \
    $$PWD/webui.h \
    $$PWD/btjson.h \
    $$PWD/prefjson.h \
    $$PWD/jsonutils.h \
    $$PWD/extra_translations.h \
    $$PWD/webapplication.h \
    $$PWD/qtorrentfilter.h \
    $$PWD/websessiondata.h \
    $$PWD/abstractwebapplication.h

SOURCES += \
    $$PWD/webui.cpp \
    $$PWD/btjson.cpp \
    $$PWD/prefjson.cpp \
    $$PWD/webapplication.cpp \
    $$PWD/qtorrentfilter.cpp \
    $$PWD/abstractwebapplication.cpp

# QJson JSON parser/serializer for using with Qt4
lessThan(QT_MAJOR_VERSION, 5): include(qjson/qjson.pri)

RESOURCES += $$PWD/webui.qrc
