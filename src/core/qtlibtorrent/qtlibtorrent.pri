INCLUDEPATH += $$PWD

HEADERS += $$PWD/qbtsession.h \
           $$PWD/qtorrenthandle.h \
           $$PWD/bandwidthscheduler.h \
           $$PWD/trackerinfos.h \
           $$PWD/torrentspeedmonitor.h \
           $$PWD/filterparserthread.h \
           $$PWD/torrentstatistics.h

SOURCES += $$PWD/qbtsession.cpp \
           $$PWD/qtorrenthandle.cpp \
           $$PWD/torrentspeedmonitor.cpp \
           $$PWD/torrentstatistics.cpp \
           $$PWD/filterparserthread.cpp

!contains(DEFINES, DISABLE_GUI) {
  HEADERS += $$PWD/torrentmodel.h \
             $$PWD/shutdownconfirm.h

  SOURCES += $$PWD/torrentmodel.cpp \
             $$PWD/shutdownconfirm.cpp
}
