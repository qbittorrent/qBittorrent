# Vars
LANG_PATH = lang
ICONS_PATH = Icons
TRAYICON_CPP = trayicon

#Set the following variable to 1 to enable debug
DEBUG_MODE = 1

# Global
TEMPLATE = app
TARGET = qbittorrent
DEPENDPATH += . trayicon
INCLUDEPATH += $$TRAYICON_CPP
CONFIG += qt x11

contains(DEBUG_MODE, 1){
  CONFIG += debug
  message(Debug build!)
}
contains(DEBUG_MODE, 0){
  CONFIG -= debug
  message(Release build!)
}

QMAKE_CXXFLAGS_RELEASE += -fwrapv
QMAKE_CXXFLAGS_DEBUG += -fwrapv
LIBS += -lcurl
CONFIG += link_pkgconfig
PKGCONFIG += libtorrent
QT += xml network

DEFINES += QT_NO_CAST_TO_ASCII

contains(DEBUG_MODE, 0){
  contains(QT_VERSION, 4.2.0) {
    message(Qt 4.2.0 detected : enabling debug output because of a bug in this version of Qt)
  }else{
    contains(QT_VERSION, 4.2.1) {
      message(Qt 4.2.1 detected : enabling debug output because of a bug in this version of Qt)
    }else{
      DEFINES += QT_NO_DEBUG_OUTPUT
    }
  }
  CONFIG += release
}

exists(../conf.pri) {
       include(../conf.pri)
}

# Install
unix {
        # Man page
	man.files = doc/qbittorrent.1.gz
        man.path = $$PREFIX/share/man/man1/
        INSTALLS += man

        # Menu Icon
        menuicon.files = Icons/qBittorrent.desktop
        menuicon.path = $$PREFIX/share/applications/
        INSTALLS += menuicon
        logos.files = menuicons/*
        logos.path = $$PREFIX/share/icons/hicolor/
        INSTALLS += logos
}

target.path = $$PREFIX/bin/
INSTALLS += target

RESOURCES = icons.qrc \
            lang.qrc \
            search.qrc

# Translations
TRANSLATIONS = $$LANG_PATH/qbittorrent_fr.ts \
	       $$LANG_PATH/qbittorrent_zh.ts \
	       $$LANG_PATH/qbittorrent_en.ts \
               $$LANG_PATH/qbittorrent_ca.ts \
	       $$LANG_PATH/qbittorrent_es.ts \
               $$LANG_PATH/qbittorrent_pl.ts \
	       $$LANG_PATH/qbittorrent_ko.ts \
               $$LANG_PATH/qbittorrent_de.ts \
               $$LANG_PATH/qbittorrent_nl.ts \
               $$LANG_PATH/qbittorrent_tr.ts \
               $$LANG_PATH/qbittorrent_sv.ts \
               $$LANG_PATH/qbittorrent_el.ts \
               $$LANG_PATH/qbittorrent_ru.ts \
               $$LANG_PATH/qbittorrent_uk.ts \
               $$LANG_PATH/qbittorrent_bg.ts \
	       $$LANG_PATH/qbittorrent_it.ts \
	       $$LANG_PATH/qbittorrent_zh_HK.ts \
	       $$LANG_PATH/qbittorrent_sk.ts \
               $$LANG_PATH/qbittorrent_ro.ts \
               $$LANG_PATH/qbittorrent_pt.ts \
	       $$LANG_PATH/qbittorrent_nb.ts \
	       $$LANG_PATH/qbittorrent_fi.ts

# Source code
HEADERS += GUI.h misc.h options_imp.h about_imp.h \
           properties_imp.h createtorrent_imp.h \
           DLListDelegate.h SearchListDelegate.h \
	   PropListDelegate.h previewSelect.h \
           PreviewListDelegate.h trackerLogin.h \
           downloadThread.h downloadFromURLImp.h \
           torrentAddition.h deleteThread.h \
           bittorrent.h
FORMS += MainWindow.ui options.ui about.ui \
         properties.ui createtorrent.ui preview.ui \
         login.ui downloadFromURL.ui addTorrentDialog.ui
SOURCES += GUI.cpp \
           main.cpp \
           options_imp.cpp \
	   properties_imp.cpp \
	   createtorrent_imp.cpp \
	   bittorrent.cpp

