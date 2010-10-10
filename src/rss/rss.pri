INCLUDEPATH += $$PWD

!contains(DEFINES, DISABLE_GUI) {

  HEADERS +=  $$PWD/rss.h \
              $$PWD/rss_imp.h \
              $$PWD/rsssettings.h \
              $$PWD/feeddownloader.h \
              $$PWD/feedlistwidget.h \
  
  SOURCES += $$PWD/rss.cpp \
             $$PWD/rss_imp.cpp \
             $$PWD/rsssettings.cpp \
             $$PWD/feedlistwidget.cpp

  FORMS +=   $$PWD/ui/rss.ui \
             $$PWD/ui/feeddownloader.ui \
             $$PWD/ui/rsssettings.ui

}
