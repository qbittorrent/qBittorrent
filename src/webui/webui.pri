INCLUDEPATH += $$PWD

HEADERS += $$PWD/httpserver.h \
           $$PWD/httpconnection.h \
           $$PWD/httprequestparser.h \
           $$PWD/httpresponsegenerator.h \
           $$PWD/eventmanager.h \
           $$PWD/json.h \
           $$PWD/jsonlist.h \
           $$PWD/jsondict.h \
           $$PWD/btjson.h

SOURCES += $$PWD/httpserver.cpp \
           $$PWD/httpconnection.cpp \
           $$PWD/httprequestparser.cpp \
           $$PWD/httpresponsegenerator.cpp \
           $$PWD/eventmanager.cpp \
           $$PWD/jsonlist.cpp \
           $$PWD/jsondict.cpp \
           $$PWD/btjson.cpp \
           $$PWD/json.cpp

RESOURCES += $$PWD/webui.qrc
