# Vars
LANG_PATH = lang
ICONS_PATH = Icons

# Set the following variable to 1 to enable debug
DEBUG_MODE = 1

# Global
TEMPLATE = app
TARGET = qbittorrent
CONFIG += qt \
    thread \
    x11 \
    network

# Update this VERSION for each release
DEFINES += VERSION=\\\"v1.5.0beta3\\\"
DEFINES += VERSION_MAJOR=1
DEFINES += VERSION_MINOR=5
DEFINES += VERSION_BUGFIX=0
!mac:QMAKE_LFLAGS += -Wl,--as-needed
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
        target.path = $$BINDIR
        INSTALLS += target
    }
    
    # Man page
    man.files = ../doc/qbittorrent.1
    man.path = $$PREFIX/share/man/man1/
    INSTALLS += man
    
    # Menu Icon
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

# QMAKE_CXXFLAGS_RELEASE += -fwrapv
# QMAKE_CXXFLAGS_DEBUG += -fwrapv
unix:QMAKE_LFLAGS_SHAPP += -rdynamic
CONFIG += link_pkgconfig
PKGCONFIG += "libtorrent-rasterbar libcurl"
QT += network \
    xml
DEFINES += QT_NO_CAST_TO_ASCII

# QT_NO_CAST_FROM_ASCII
# Windows
# usually built as static
# win32:LIBS += -ltorrent -lcurl -lboost_system
# win32:LIBS += -lz ?
win32:LIBS += -lssl32 \
    -lws2_32 \
    -lwsock32 \
    -ladvapi32 \
    -lwinmm
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
    $$LANG_PATH/qbittorrent_cs.ts

# Source code
HEADERS += GUI.h \
    misc.h \
    options_imp.h \
    about_imp.h \
    properties_imp.h \
    createtorrent_imp.h \
    DLListDelegate.h \
    SearchListDelegate.h \
    PropListDelegate.h \
    previewSelect.h \
    PreviewListDelegate.h \
    trackerLogin.h \
    downloadThread.h \
    downloadFromURLImp.h \
    torrentAddition.h \
    bittorrent.h \
    searchEngine.h \
    rss.h \
    rss_imp.h \
    FinishedTorrents.h \
    allocationDlg.h \
    FinishedListDelegate.h \
    qtorrenthandle.h \
    downloadingTorrents.h \
    engineSelectDlg.h \
    pluginSource.h \
    arborescence.h \
    qgnomelook.h \
    realprogressbar.h \
    realprogressbarthread.h \
    qrealarray.h \
    httpserver.h \
    httpconnection.h \
    httprequestparser.h \
    httpresponsegenerator.h \
    json.h \
    eventmanager.h \
    filterParserThread.h \
    TrackersAdditionDlg.h \
    searchTab.h \
    console_imp.h \
    ico.h \
    stacktrace.h \
    torrentPersistentData.h \
    FeedDownloader.h \
    feedList.h \
    supportedEngines.h
FORMS += MainWindow.ui \
    options.ui \
    about.ui \
    properties.ui \
    createtorrent.ui \
    preview.ui \
    login.ui \
    downloadFromURL.ui \
    addTorrentDialog.ui \
    search.ui \
    rss.ui \
    seeding.ui \
    bandwidth_limit.ui \
    download.ui \
    engineSelect.ui \
    pluginSource.ui \
    trackersAdd.ui \
    console.ui \
    FeedDownloader.ui
SOURCES += GUI.cpp \
    main.cpp \
    options_imp.cpp \
    properties_imp.cpp \
    createtorrent_imp.cpp \
    bittorrent.cpp \
    searchEngine.cpp \
    rss_imp.cpp \
    FinishedTorrents.cpp \
    qtorrenthandle.cpp \
    downloadingTorrents.cpp \
    engineSelectDlg.cpp \
    downloadThread.cpp \
    realprogressbar.cpp \
    realprogressbarthread.cpp \
    qrealarray.cpp \
    httpserver.cpp \
    httpconnection.cpp \
    httprequestparser.cpp \
    httpresponsegenerator.cpp \
    eventmanager.cpp \
    SearchTab.cpp \
    ico.cpp \
    rss.cpp
DESTDIR = .
