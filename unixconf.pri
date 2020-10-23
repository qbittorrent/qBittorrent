# The first path is used when the source is being build by packagers (pbuilder/sbuild/etc)
# The second path is used when you manually run the configure script in the root folder (eg when using qt creator)
exists($$OUT_PWD/../conf.pri) {
    include($$OUT_PWD/../conf.pri)
}
else {
    include(conf.pri)
}

# COMPILATION SPECIFIC
!nogui:dbus: QT += dbus

QMAKE_CXXFLAGS += -Wall -Wextra -Wpedantic -Wformat-security
!haiku: QMAKE_LFLAGS_APP += -rdynamic

# Man page
nogui {
    man.files = ../doc/qbittorrent-nox.1
}
else {
    man.files = ../doc/qbittorrent.1
}

man.path = $$MANPREFIX/man1
INSTALLS += man

DIST_PATH = ../dist/unix

# Systemd Service file
nogui:systemd {
    systemdService.files = $$DIST_PATH/systemd/qbittorrent-nox@.service
    systemdService.path = $$PREFIX/lib/systemd/system/
    INSTALLS += systemdService
}

# Menu Icon
!nogui {
    menuicon.files = $$DIST_PATH/org.qbittorrent.qBittorrent.desktop
    menuicon.path = $$DATADIR/applications/
    INSTALLS += menuicon

    appdata.files = $$DIST_PATH/org.qbittorrent.qBittorrent.appdata.xml
    appdata.path = $$DATADIR/metainfo/
    INSTALLS += appdata

    # Apps icons
    icon16.files = $$DIST_PATH/menuicons/16x16/apps/qbittorrent.png
    icon16.path = $$DATADIR/icons/hicolor/16x16/apps/
    icon22.files = $$DIST_PATH/menuicons/22x22/apps/qbittorrent.png
    icon22.path = $$DATADIR/icons/hicolor/22x22/apps/
    icon24.files = $$DIST_PATH/menuicons/24x24/apps/qbittorrent.png
    icon24.path = $$DATADIR/icons/hicolor/24x24/apps/
    icon32.files = $$DIST_PATH/menuicons/32x32/apps/qbittorrent.png
    icon32.path = $$DATADIR/icons/hicolor/32x32/apps/
    icon36.files = $$DIST_PATH/menuicons/36x36/apps/qbittorrent.png
    icon36.path = $$DATADIR/icons/hicolor/36x36/apps/
    icon48.files = $$DIST_PATH/menuicons/48x48/apps/qbittorrent.png
    icon48.path = $$DATADIR/icons/hicolor/48x48/apps/
    icon64.files = $$DIST_PATH/menuicons/64x64/apps/qbittorrent.png
    icon64.path = $$DATADIR/icons/hicolor/64x64/apps/
    icon72.files = $$DIST_PATH/menuicons/72x72/apps/qbittorrent.png
    icon72.path = $$DATADIR/icons/hicolor/72x72/apps/
    icon96.files = $$DIST_PATH/menuicons/96x96/apps/qbittorrent.png
    icon96.path = $$DATADIR/icons/hicolor/96x96/apps/
    icon128.files = $$DIST_PATH/menuicons/128x128/apps/qbittorrent.png
    icon128.path = $$DATADIR/icons/hicolor/128x128/apps/
    icon192.files = $$DIST_PATH/menuicons/192x192/apps/qbittorrent.png
    icon192.path = $$DATADIR/icons/hicolor/192x192/apps/
    # Status icons
    statusIcon16.files = $$DIST_PATH/menuicons/16x16/status/qbittorrent-tray.png
    statusIcon16.path = $$DATADIR/icons/hicolor/16x16/status/
    statusIcon22.files = $$DIST_PATH/menuicons/22x22/status/qbittorrent-tray.png
    statusIcon22.path = $$DATADIR/icons/hicolor/22x22/status/
    statusIcon24.files = $$DIST_PATH/menuicons/24x24/status/qbittorrent-tray.png
    statusIcon24.path = $$DATADIR/icons/hicolor/24x24/status/
    statusIcon32.files = $$DIST_PATH/menuicons/32x32/status/qbittorrent-tray.png
    statusIcon32.path = $$DATADIR/icons/hicolor/32x32/status/
    statusIcon36.files = $$DIST_PATH/menuicons/36x36/status/qbittorrent-tray.png
    statusIcon36.path = $$DATADIR/icons/hicolor/36x36/status/
    statusIcon48.files = $$DIST_PATH/menuicons/48x48/status/qbittorrent-tray.png
    statusIcon48.path = $$DATADIR/icons/hicolor/48x48/status/
    statusIcon64.files = $$DIST_PATH/menuicons/64x64/status/qbittorrent-tray.png
    statusIcon64.path = $$DATADIR/icons/hicolor/64x64/status/
    statusIcon72.files = $$DIST_PATH/menuicons/72x72/status/qbittorrent-tray.png
    statusIcon72.path = $$DATADIR/icons/hicolor/72x72/status/
    statusIcon96.files = $$DIST_PATH/menuicons/96x96/status/qbittorrent-tray.png
    statusIcon96.path = $$DATADIR/icons/hicolor/96x96/status/
    statusIcon128.files = $$DIST_PATH/menuicons/128x128/status/qbittorrent-tray.png
    statusIcon128.path = $$DATADIR/icons/hicolor/128x128/status/
    statusIcon192.files = $$DIST_PATH/menuicons/192x192/status/qbittorrent-tray.png
    statusIcon192.path = $$DATADIR/icons/hicolor/192x192/status/

    statusIconScalable.files = $$PWD/src/icons/qbittorrent-tray.svg \
                               $$PWD/src/icons/qbittorrent-tray-dark.svg \
                               $$PWD/src/icons/qbittorrent-tray-light.svg
    statusIconScalable.path = $$DATADIR/icons/hicolor/scalable/status/

    INSTALLS += \
        icon16 \
        icon22 \
        icon24 \
        icon32 \
        icon36 \
        icon48 \
        icon64 \
        icon72 \
        icon96 \
        icon128 \
        icon192 \
        statusIcon16 \
        statusIcon22 \
        statusIcon24 \
        statusIcon32 \
        statusIcon36 \
        statusIcon48 \
        statusIcon64 \
        statusIcon72 \
        statusIcon96 \
        statusIcon128 \
        statusIcon192 \
        statusIconScalable

    pixmap.files = $$DIST_PATH/menuicons/128x128/apps/qbittorrent.png
    pixmap.path = $$DATADIR/pixmaps/
    INSTALLS += pixmap
}

# INSTALL
target.path = $$PREFIX/bin/
INSTALLS += target
