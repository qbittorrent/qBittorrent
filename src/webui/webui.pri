INCLUDEPATH += $$PWD

HEADERS += $$PWD/httpserver.h \
           $$PWD/httpconnection.h \
           $$PWD/httprequestparser.h \
           $$PWD/httpresponsegenerator.h \
           $$PWD/json.h \
           $$PWD/jsonlist.h \
           $$PWD/jsondict.h \
           $$PWD/btjson.h \
           $$PWD/prefjson.h

SOURCES += $$PWD/httpserver.cpp \
           $$PWD/httpconnection.cpp \
           $$PWD/httprequestparser.cpp \
           $$PWD/httpresponsegenerator.cpp \
           $$PWD/jsonlist.cpp \
           $$PWD/jsondict.cpp \
           $$PWD/btjson.cpp \
           $$PWD/json.cpp \
           $$PWD/prefjson.cpp

RESOURCES += $$PWD/webui.qrc
