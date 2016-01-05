INCLUDEPATH += $$PWD

HEADERS +=    $$PWD/rss_imp.h \
              $$PWD/rsssettingsdlg.h \
              $$PWD/feedlistwidget.h \
              $$PWD/automatedrssdownloader.h \
              $$PWD/cookiesdlg.h \
              $$PWD/htmlbrowser.h

SOURCES +=   $$PWD/rss_imp.cpp \
             $$PWD/rsssettingsdlg.cpp \
             $$PWD/feedlistwidget.cpp \
             $$PWD/automatedrssdownloader.cpp \
             $$PWD/cookiesdlg.cpp \
             $$PWD/htmlbrowser.cpp

FORMS +=   $$PWD/rss.ui \
           $$PWD/rsssettingsdlg.ui \
           $$PWD/automatedrssdownloader.ui \
           $$PWD/cookiesdlg.ui
