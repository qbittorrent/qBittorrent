INCLUDEPATH += $$PWD

HEADERS +=    $$PWD/rss_imp.h \
              $$PWD/rsssettingsdlg.h \
              $$PWD/automatedrssdownloader.h \
              $$PWD/htmlbrowser.h \
              $$PWD/rssfeedlistwidget.h \
              $$PWD/rssfeedlistwidgetitem.h

SOURCES +=   $$PWD/rss_imp.cpp \
             $$PWD/rsssettingsdlg.cpp \
             $$PWD/automatedrssdownloader.cpp \
             $$PWD/htmlbrowser.cpp \
             $$PWD/rssfeedlistwidget.cpp \
             $$PWD/rssfeedlistwidgetitem.cpp

FORMS +=   $$PWD/rss.ui \
           $$PWD/rsssettingsdlg.ui \
           $$PWD/automatedrssdownloader.ui
