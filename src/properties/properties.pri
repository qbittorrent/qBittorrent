INCLUDEPATH += $$PWD

!contains(DEFINES, DISABLE_GUI) {
  FORMS += $$PWD/propertieswidget.ui

  HEADERS += $$PWD/propertieswidget.h \
             $$PWD/peerlistwidget.h \
             $$PWD/proplistdelegate.h \
             $$PWD/trackerlist.h \
             $$PWD/downloadedpiecesbar.h \
             $$PWD/peerlistdelegate.h \
             $$PWD/peeraddition.h \
             $$PWD/trackersadditiondlg.h \
             $$PWD/pieceavailabilitybar.h

  SOURCES += $$PWD/propertieswidget.cpp \
             $$PWD/peerlistwidget.cpp \
             $$PWD/trackerlist.cpp

}
