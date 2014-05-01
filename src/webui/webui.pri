INCLUDEPATH += $$PWD

HEADERS += $$PWD/httpserver.h \
           $$PWD/httpconnection.h \
           $$PWD/httprequestparser.h \
           $$PWD/httpresponsegenerator.h \
           $$PWD/json.h \
           $$PWD/jsonlist.h \
           $$PWD/jsondict.h \
           $$PWD/btjson.h \
           $$PWD/prefjson.h \
           $$PWD/httpheader.h \
           $$PWD/httprequestheader.h \
           $$PWD/httpresponseheader.h

SOURCES += $$PWD/httpserver.cpp \
           $$PWD/httpconnection.cpp \
           $$PWD/httprequestparser.cpp \
           $$PWD/httpresponsegenerator.cpp \
           $$PWD/jsonlist.cpp \
           $$PWD/jsondict.cpp \
           $$PWD/btjson.cpp \
           $$PWD/json.cpp \
           $$PWD/prefjson.cpp \
           $$PWD/httpheader.cpp \
           $$PWD/httprequestheader.cpp \
           $$PWD/httpresponseheader.cpp

RESOURCES += $$PWD/webui.qrc
