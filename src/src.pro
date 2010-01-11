# Vars
LANG_PATH = lang
ICONS_PATH = Icons

# Set the following variable to 1 to enable debug
DEBUG_MODE = 0

# Global
TEMPLATE = app
CONFIG += qt \
    thread

# Update this VERSION for each release
DEFINES += VERSION=\\\"v2.1.0rc5\\\"
DEFINES += VERSION_MAJOR=2
DEFINES += VERSION_MINOR=1
DEFINES += VERSION_BUGFIX=0
# NORMAL,ALPHA,BETA,RELEASE_CANDIDATE,DEVEL
DEFINES += VERSION_TYPE=RELEASE_CANDIDATE

# !mac:QMAKE_LFLAGS += -Wl,--as-needed
contains(DEBUG_MODE, 1) { 
    CONFIG += debug
    CONFIG -= release
    message(Debug build!)
}
contains(DEBUG_MODE, 0) { 
    CONFIG -= debug
    CONFIG += release
    DEFINES += QT_NO_DEBUG_OUTPUT
    message(Release build!)
}

# Install
!win32 { 
    # Binary
    exists(../conf.pri) { 
        include(../conf.pri)
        
        # Target
        #target.path = $$BINDIR
        target.path = $$PREFIX/bin/
        INSTALLS += target
    }
    
    # Man page
contains(DEFINES, DISABLE_GUI) {
    man.files = ../doc/qbittorrent-nox.1
} else {
    man.files = ../doc/qbittorrent.1
}
    man.path = $$PREFIX/share/man/man1/
    INSTALLS += man
    
    # Menu Icon
    !contains(DEFINES, DISABLE_GUI) {
      menuicon.files = Icons/qBittorrent.desktop
      menuicon.path = $$PREFIX/share/applications/
      INSTALLS += menuicon
      icon16.files = menuicons/16x16/apps/qbittorrent.png
      icon16.path = $$PREFIX/share/icons/hicolor/16x16/apps/
      icon22.files = menuicons/22x22/apps/qbittorrent.png
      icon22.path = $$PREFIX/share/icons/hicolor/22x22/apps/
      icon24.files = menuicons/24x24/apps/qbittorrent.png
      icon24.path = $$PREFIX/share/icons/hicolor/24x24/apps/
      icon32.files = menuicons/32x32/apps/qbittorrent.png
      icon32.path = $$PREFIX/share/icons/hicolor/32x32/apps/
      icon36.files = menuicons/36x36/apps/qbittorrent.png
      icon36.path = $$PREFIX/share/icons/hicolor/36x36/apps/
      icon48.files = menuicons/48x48/apps/qbittorrent.png
      icon48.path = $$PREFIX/share/icons/hicolor/48x48/apps/
      icon64.files = menuicons/64x64/apps/qbittorrent.png
      icon64.path = $$PREFIX/share/icons/hicolor/64x64/apps/
      icon72.files = menuicons/72x72/apps/qbittorrent.png
      icon72.path = $$PREFIX/share/icons/hicolor/72x72/apps/
      icon96.files = menuicons/96x96/apps/qbittorrent.png
      icon96.path = $$PREFIX/share/icons/hicolor/96x96/apps/
      icon128.files = menuicons/128x128/apps/qbittorrent.png
      icon128.path = $$PREFIX/share/icons/hicolor/128x128/apps/
      icon192.files = menuicons/192x192/apps/qbittorrent.png
      icon192.path = $$PREFIX/share/icons/hicolor/192x192/apps/
      INSTALLS += icon16 \
          icon22 \
          icon24 \
          icon32 \
          icon36 \
          icon48 \
          icon64 \
          icon72 \
          icon96 \
          icon128 \
          icon192
     }
}

contains(DEFINES, DISABLE_GUI) {
  QT=core
  TARGET = qbittorrent-nox
} else {
  TARGET = qbittorrent
}

# QMAKE_CXXFLAGS_RELEASE += -fwrapv
# QMAKE_CXXFLAGS_DEBUG += -fwrapv
unix:QMAKE_LFLAGS_SHAPP += -rdynamic
CONFIG += link_pkgconfig
PKGCONFIG += "libtorrent-rasterbar"
QT += network \
    xml
DEFINES += QT_NO_CAST_TO_ASCII

# Windows
# usually built as static
# win32:LIBS += -ltorrent -lboost_system
# win32:LIBS += -lz ?
win32:LIBS += -lssl32 \
    -lws2_32 \
    -lwsock32 \
    -ladvapi32 \
    -lwinmm

!contains(DEFINES, DISABLE_GUI) {
  win32 {
    DEFINES += WITH_GEOIP_EMBEDDED
    message("On Windows, GeoIP database must be embedded.")
  }

  macx {
    DEFINES += WITH_GEOIP_EMBEDDED
    message("On Mac OS X, GeoIP database must be embedded.")
  }

  unix:!macx {
    contains(DEFINES, WITH_GEOIP_EMBEDDED) {
      message("You chose to embed GeoIP database in qBittorrent executable.")
    }
  }

  # Add GeoIP resource file if the GeoIP database
  # should be embedded in qBittorrent executable
  contains(DEFINES, WITH_GEOIP_EMBEDDED) {
      exists("geoip/GeoIP.dat") {
         message("GeoIP.dat was found in src/geoip/.")
         RESOURCES += geoip.qrc
      } else {
        DEFINES -= WITH_GEOIP_EMBEDDED
        error("GeoIP.dat was not found in src/geoip/ folder, please follow instructions in src/geoip/README.")
      }
  } else {
    message("GeoIP database will not be embedded in qBittorrent executable.")
  }
}

# Resource files
RESOURCES = icons.qrc \
    lang.qrc \
    search.qrc \
    webui.qrc

# Translations
TRANSLATIONS = $$LANG_PATH/qbittorrent_fr.ts \
    $$LANG_PATH/qbittorrent_zh.ts \
    $$LANG_PATH/qbittorrent_zh_TW.ts \
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
    $$LANG_PATH/qbittorrent_sk.ts \
    $$LANG_PATH/qbittorrent_ro.ts \
    $$LANG_PATH/qbittorrent_pt.ts \
    $$LANG_PATH/qbittorrent_nb.ts \
    $$LANG_PATH/qbittorrent_fi.ts \
    $$LANG_PATH/qbittorrent_da.ts \
    $$LANG_PATH/qbittorrent_ja.ts \
    $$LANG_PATH/qbittorrent_hu.ts \
    $$LANG_PATH/qbittorrent_pt_BR.ts \
    $$LANG_PATH/qbittorrent_cs.ts \
    $$LANG_PATH/qbittorrent_sr.ts

# Source code
HEADERS += misc.h \
    downloadthread.h \
    bittorrent.h \
    qtorrenthandle.h \
    httpserver.h \
    httpconnection.h \
    httprequestparser.h \
    httpresponsegenerator.h \
    json.h \
    eventmanager.h \
    filterparserthread.h \
    stacktrace.h \
    torrentpersistentdata.h \
    filesystemwatcher.h \
    preferences.h

contains(DEFINES, DISABLE_GUI) {
    HEADERS += headlessloader.h
} else {
	HEADERS += GUI.h \
                 feedList.h \
                 supportedengines.h \
                 transferlistwidget.h \
                 transferlistdelegate.h \
                 transferlistfilterswidget.h \
                 propertieswidget.h \
                 torrentfilesmodel.h \
                 geoip.h \
                 peeraddition.h \
                 deletionconfirmationdlg.h \
                 statusbar.h \
                 trackerlist.h \
                 downloadedpiecesbar.h \
                 peerlistwidget.h \
                 peerlistdelegate.h \
                 reverseresolution.h \
                 feeddownloader.h \
                 trackersadditiondlg.h \
                 searchtab.h \
                 console_imp.h \
                 ico.h \
                 engineselectdlg.h \
                 pluginsource.h \
                 qgnomelook.h \
                 searchEngine.h \
                 rss.h \
                 rss_imp.h \
                 speedlimitdlg.h \
                 options_imp.h \
                 about_imp.h \
                 createtorrent_imp.h \
                 searchlistdelegate.h \
                 proplistdelegate.h \
                 previewselect.h \
                 previewlistdelegate.h \
                 downloadfromurldlg.h \
                 torrentadditiondlg.h \
                 trackerlogin.h \
                 pieceavailabilitybar.h
}

!contains(DEFINES, DISABLE_GUI) {
	FORMS += ui/mainwindow.ui \
	    ui/options.ui \
	    ui/about.ui \
	    ui/createtorrent.ui \
	    ui/preview.ui \
	    ui/login.ui \
	    ui/downloadfromurldlg.ui \
	    ui/torrentadditiondlg.ui \
	    ui/search.ui \
	    ui/rss.ui \
	    ui/bandwidth_limit.ui \
	    ui/engineselect.ui \
	    ui/pluginsource.ui \
	    ui/trackersadditiondlg.ui \
	    ui/console.ui \
	    ui/feeddownloader.ui \
	    ui/propertieswidget.ui \
	    ui/peer.ui \
	    ui/confirmdeletiondlg.ui
}

SOURCES += main.cpp \ 
    bittorrent.cpp \
    qtorrenthandle.cpp \
    downloadthread.cpp \
    httpserver.cpp \
    httpconnection.cpp \
    httprequestparser.cpp \
    httpresponsegenerator.cpp \
    eventmanager.cpp

!contains(DEFINES, DISABLE_GUI) {
	SOURCES += GUI.cpp \
                   options_imp.cpp \
                   createtorrent_imp.cpp \
                   searchengine.cpp \
                   rss_imp.cpp \
                   engineselectdlg.cpp \
                   searchtab.cpp \
                   ico.cpp \
		   rss.cpp \
                   transferlistwidget.cpp \
                   propertieswidget.cpp \
                   peerlistwidget.cpp
}

DESTDIR = .
