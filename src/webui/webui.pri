INCLUDEPATH += $$PWD

HEADERS += $$PWD/httpserver.h \
           $$PWD/httpconnection.h \
           $$PWD/httprequestparser.h \
           $$PWD/httpresponsegenerator.h \
           $$PWD/btjson.h \
           $$PWD/prefjson.h \
           $$PWD/httpheader.h \
           $$PWD/httprequestheader.h \
           $$PWD/httpresponseheader.h \
           $$PWD/jsonutils.h

SOURCES += $$PWD/httpserver.cpp \
           $$PWD/httpconnection.cpp \
           $$PWD/httprequestparser.cpp \
           $$PWD/httpresponsegenerator.cpp \
           $$PWD/btjson.cpp \
           $$PWD/prefjson.cpp \
           $$PWD/httpheader.cpp \
           $$PWD/httprequestheader.cpp \
           $$PWD/httpresponseheader.cpp
           
lessThan(QT_MAJOR_VERSION, 5) {
  HEADERS += $$PWD/json.h
  SOURCES += $$PWD/json.cpp
}

RESOURCES += $$PWD/webui.qrc
