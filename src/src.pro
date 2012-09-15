# Global
TEMPLATE = app
CONFIG += qt thread

# Windows specific configuration
win32 {
  include(../winconf.pri)  
}

# Mac specific configuration
macx {
  include(../macxconf.pri)
}

# Unix specific configuration
unix:!macx {
  include(../unixconf.pri)
}

# eCS(OS/2) specific configuration
os2 {
  include(../os2conf.pri)
}

nox {
  QT -= gui
  TARGET = qbittorrent-nox
  DEFINES += DISABLE_GUI
} else {
  QT += xml
  TARGET = qbittorrent
}
QT += network

# Vars
LANG_PATH = lang
ICONS_PATH = Icons

CONFIG(debug, debug|release):message(Project is built in DEBUG mode.)
CONFIG(release, debug|release):message(Project is built in RELEASE mode.)

# Disable debug output in release mode
CONFIG(release, debug|release) {
   message(Disabling debug output.)
   DEFINES += QT_NO_DEBUG_OUTPUT
}

# VERSION DEFINES
include(../version.pri)

DEFINES += QT_NO_CAST_TO_ASCII
# Fast concatenation (Qt >= 4.6)
DEFINES += QT_USE_FAST_CONCATENATION QT_USE_FAST_OPERATOR_PLUS

# Fixes compilation with Boost >= v1.46 where boost
# filesystem v3 is the default.
DEFINES += BOOST_FILESYSTEM_VERSION=2

INCLUDEPATH += $$PWD


# Resource files
RESOURCES += icons.qrc \
            lang.qrc \
            about.qrc

# Source code
usesystemqtsingleapplication {
  nox {
    CONFIG += qtsinglecoreapplication
  } else {
    CONFIG += qtsingleapplication
  }
} else {
  nox {
    include(qtsingleapp/qtsinglecoreapplication.pri)
  } else {
    include(qtsingleapp/qtsingleapplication.pri)
  }
}

include(qtlibtorrent/qtlibtorrent.pri)
include(webui/webui.pri)
include(tracker/tracker.pri)
include (preferences/preferences.pri)

!nox {
  include(lineedit/lineedit.pri)
  include(properties/properties.pri)
  include(searchengine/searchengine.pri)
  include(rss/rss.pri)
  include(torrentcreator/torrentcreator.pri)
  include(geoip/geoip.pri)
  include(powermanagement/powermanagement.pri)
}

HEADERS += misc.h \
           fs_utils.h \
           downloadthread.h \
           stacktrace.h \
           torrentpersistentdata.h \
           filesystemwatcher.h \
           scannedfoldersmodel.h \
           qinisettings.h \
           smtp.h \
           dnsupdater.h


SOURCES += main.cpp \
           downloadthread.cpp \
           scannedfoldersmodel.cpp \
           misc.cpp \
           fs_utils.cpp \
           smtp.cpp \
           dnsupdater.cpp

nox {
  HEADERS += headlessloader.h
} else {
  HEADERS +=  mainwindow.h\
              transferlistwidget.h \
              transferlistdelegate.h \
              transferlistfilterswidget.h \
              torrentcontentmodel.h \
              torrentcontentmodelitem.h \
              torrentcontentmodelfolder.h \
              torrentcontentmodelfile.h \
              torrentcontentfiltermodel.h \
              deletionconfirmationdlg.h \
              statusbar.h \
              reverseresolution.h \
              ico.h \
              speedlimitdlg.h \
              about_imp.h \
              previewselect.h \
              previewlistdelegate.h \
              downloadfromurldlg.h \
              trackerlogin.h \
              hidabletabwidget.h \
              sessionapplication.h \
              torrentimportdlg.h \
              executionlog.h \
              iconprovider.h \
              updownratiodlg.h \
              loglistwidget.h \
              addnewtorrentdialog.h

  SOURCES += mainwindow.cpp \
             ico.cpp \
             transferlistwidget.cpp \
             torrentcontentmodel.cpp \
             torrentcontentmodelitem.cpp \
             torrentcontentmodelfolder.cpp \
             torrentcontentmodelfile.cpp \
             torrentcontentfiltermodel.cpp \
             sessionapplication.cpp \
             torrentimportdlg.cpp \
             executionlog.cpp \
             previewselect.cpp \
             iconprovider.cpp \
             updownratiodlg.cpp \
             loglistwidget.cpp \
             addnewtorrentdialog.cpp

  win32 {
    HEADERS += programupdater.h
    SOURCES += programupdater.cpp
  }

  macx {
    HEADERS += qmacapplication.h \
               programupdater.h

    SOURCES += qmacapplication.cpp \
               programupdater.cpp
  }

  FORMS += mainwindow.ui \
           about.ui \
           preview.ui \
           login.ui \
           downloadfromurldlg.ui \
           bandwidth_limit.ui \
           updownratiodlg.ui \
           confirmdeletiondlg.ui \
           torrentimportdlg.ui \
           executionlog.ui \
           addnewtorrentdialog.ui
}

DESTDIR = .

# OS specific config
OTHER_FILES += ../winconf.pri ../macxconf.pri ../unixconf.pri ../os2conf.pri
# compiler specific config
OTHER_FILES += ../winconf-mingw.pri ../winconf-msvc.pri
# version file
OTHER_FILES += ../version.pri

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
               $$LANG_PATH/qbittorrent_sr.ts \
               $$LANG_PATH/qbittorrent_ar.ts \
               $$LANG_PATH/qbittorrent_hr.ts \
               $$LANG_PATH/qbittorrent_gl.ts \
               $$LANG_PATH/qbittorrent_hy.ts \
               $$LANG_PATH/qbittorrent_lt.ts \
               $$LANG_PATH/qbittorrent_ka.ts \
               $$LANG_PATH/qbittorrent_be.ts \
               $$LANG_PATH/qbittorrent_eu.ts \
               $$LANG_PATH/qbittorrent_he.ts
