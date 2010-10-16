INCLUDEPATH += $$PWD

!contains(DEFINES, DISABLE_GUI) {

  HEADERS +=  $$PWD/rss_imp.h \
              $$PWD/rsssettings.h \
              $$PWD/feeddownloader.h \
              $$PWD/feedlistwidget.h \
              $$PWD/rssmanager.h \
              $$PWD/rssfeed.h \
              $$PWD/rssfolder.h \
              $$PWD/rssfile.h \
              $$PWD/rssarticle.h
  
  SOURCES += $$PWD/rss_imp.cpp \
             $$PWD/rsssettings.cpp \
             $$PWD/feedlistwidget.cpp \
             $$PWD/rssmanager.cpp \
             $$PWD/rssfeed.cpp \
             $$PWD/rssfolder.cpp \
             $$PWD/rssarticle.cpp

  FORMS +=   $$PWD/ui/rss.ui \
             $$PWD/ui/feeddownloader.ui \
             $$PWD/ui/rsssettings.ui
}
