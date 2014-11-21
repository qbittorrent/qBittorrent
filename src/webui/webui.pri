INCLUDEPATH += $$PWD

HEADERS += $$PWD/httpserver.h \
           $$PWD/httpconnection.h \
           $$PWD/httprequestparser.h \
           $$PWD/httpresponsegenerator.h \
           $$PWD/btjson.h \
           $$PWD/prefjson.h \
           $$PWD/jsonutils.h \
           $$PWD/httptypes.h \
           $$PWD/extra_translations.h \
           $$PWD/webapplication.h \
           $$PWD/abstractrequesthandler.h \
           $$PWD/requesthandler.h \
           $$PWD/qtorrentfilter.h

SOURCES += $$PWD/httpserver.cpp \
           $$PWD/httpconnection.cpp \
           $$PWD/httprequestparser.cpp \
           $$PWD/httpresponsegenerator.cpp \
           $$PWD/btjson.cpp \
           $$PWD/prefjson.cpp \
           $$PWD/webapplication.cpp \
           $$PWD/abstractrequesthandler.cpp \
           $$PWD/requesthandler.cpp \
           $$PWD/qtorrentfilter.cpp

# QJson JSON parser/serializer for using with Qt4
lessThan(QT_MAJOR_VERSION, 5) {
  include(qjson/qjson.pri)
}

RESOURCES += $$PWD/webui.qrc
