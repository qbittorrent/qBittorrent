HEADERS += \
    $$PWD/webui.h \
    $$PWD/btjson.h \
    $$PWD/prefjson.h \
    $$PWD/jsonutils.h \
    $$PWD/extra_translations.h \
    $$PWD/webapplication.h \
    $$PWD/websessiondata.h \
    $$PWD/abstractwebapplication.h

SOURCES += \
    $$PWD/webui.cpp \
    $$PWD/btjson.cpp \
    $$PWD/prefjson.cpp \
    $$PWD/webapplication.cpp \
    $$PWD/abstractwebapplication.cpp

# QJson JSON parser/serializer for using with Qt4
lessThan(QT_MAJOR_VERSION, 5) {
    !usesystemqjson: include(qjson/qjson.pri)
    else: DEFINES += USE_SYSTEM_QJSON
}

RESOURCES += $$PWD/webui.qrc
