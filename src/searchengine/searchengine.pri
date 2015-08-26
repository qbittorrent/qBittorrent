INCLUDEPATH += $$PWD

FORMS += $$PWD/searchwidget.ui \
         $$PWD/engineselectdlg.ui \
         $$PWD/pluginsourcedlg.ui

HEADERS += $$PWD/searchwidget.h \
           $$PWD/searchtab.h \
           $$PWD/engineselectdlg.h \
           $$PWD/pluginsourcedlg.h \
           $$PWD/searchlistdelegate.h \
           $$PWD/supportedengines.h \
           $$PWD/searchsortmodel.h

SOURCES += $$PWD/searchwidget.cpp \
           $$PWD/searchtab.cpp \
           $$PWD/engineselectdlg.cpp

RESOURCES += $$PWD/search.qrc
