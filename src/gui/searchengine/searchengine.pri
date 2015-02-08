INCLUDEPATH += $$PWD

FORMS += $$PWD/search.ui \
         $$PWD/engineselect.ui \
         $$PWD/pluginsource.ui

HEADERS += $$PWD/searchengine.h \
           $$PWD/searchtab.h \
           $$PWD/engineselectdlg.h \
           $$PWD/pluginsource.h \
           $$PWD/searchlistdelegate.h \
           $$PWD/supportedengines.h \
           $$PWD/searchsortmodel.h

SOURCES += $$PWD/searchengine.cpp \
           $$PWD/searchtab.cpp \
           $$PWD/engineselectdlg.cpp

RESOURCES += $$PWD/search.qrc
