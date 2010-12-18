INCLUDEPATH += $$PWD

HEADERS += $$PWD/qbtsession.h \
           $$PWD/qtorrenthandle.h \
           $$PWD/bandwidthscheduler.h \
           $$PWD/trackerinfos.h \
    qtlibtorrent/torrentspeedmonitor.h

SOURCES += $$PWD/qbtsession.cpp \
           $$PWD/qtorrenthandle.cpp \
    qtlibtorrent/torrentspeedmonitor.cpp

!contains(DEFINES, DISABLE_GUI) {
  HEADERS += $$PWD/torrentmodel.h
  SOURCES += $$PWD/torrentmodel.cpp
}
