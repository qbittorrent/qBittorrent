INCLUDEPATH += $$PWD

HEADERS += $$PWD/qbtsession.h \
           $$PWD/qtorrenthandle.h \
           $$PWD/bandwidthscheduler.h \
           $$PWD/trackerinfos.h

SOURCES += $$PWD/qbtsession.cpp \
           $$PWD/qtorrenthandle.cpp

!contains(DEFINES, DISABLE_GUI) {
  HEADERS += $$PWD/torrentmodel.h
  SOURCES += $$PWD/torrentmodel.cpp
}
