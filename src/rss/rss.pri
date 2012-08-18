INCLUDEPATH += $$PWD

HEADERS +=    $$PWD/rss_imp.h \
              $$PWD/rsssettingsdlg.h \
              $$PWD/feedlistwidget.h \
              $$PWD/rssmanager.h \
              $$PWD/rssfeed.h \
              $$PWD/rssfolder.h \
              $$PWD/rssfile.h \
              $$PWD/rssarticle.h \
              $$PWD/automatedrssdownloader.h \
              $$PWD/rsssettings.h \
              $$PWD/rssdownloadrule.h \
              $$PWD/rssdownloadrulelist.h \
              $$PWD/cookiesdlg.h \
              $$PWD/rssparser.h

SOURCES +=   $$PWD/rss_imp.cpp \
             $$PWD/rsssettingsdlg.cpp \
             $$PWD/feedlistwidget.cpp \
             $$PWD/rssmanager.cpp \
             $$PWD/rssfeed.cpp \
             $$PWD/rssfolder.cpp \
             $$PWD/rssarticle.cpp \
             $$PWD/automatedrssdownloader.cpp \
             $$PWD/rssdownloadrule.cpp \
             $$PWD/rssdownloadrulelist.cpp \
             $$PWD/cookiesdlg.cpp \
             $$PWD/rssfile.cpp \
             $$PWD/rssparser.cpp

FORMS +=   $$PWD/rss.ui \
           $$PWD/rsssettingsdlg.ui \
           $$PWD/automatedrssdownloader.ui \
           $$PWD/cookiesdlg.ui
