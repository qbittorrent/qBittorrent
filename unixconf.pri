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
    systemdService.path = $$PREFIX/lib/systemd/system
    INSTALLS += systemdService
}

# Menu Icon
!nogui {
    desktopEntry.files = $$DIST_PATH/org.qbittorrent.qBittorrent.desktop
    desktopEntry.path = $$DATADIR/applications
    INSTALLS += desktopEntry

    appdata.files = $$DIST_PATH/org.qbittorrent.qBittorrent.appdata.xml
    appdata.path = $$DATADIR/metainfo
    INSTALLS += appdata

    menuicons.files = $$DIST_PATH/menuicons/*
    menuicons.path = $$DATADIR/icons/hicolor
    statusIconScalable.files = $$PWD/src/icons/qbittorrent-tray.svg \
                               $$PWD/src/icons/qbittorrent-tray-dark.svg \
                               $$PWD/src/icons/qbittorrent-tray-light.svg
    statusIconScalable.path = $$DATADIR/icons/hicolor/scalable/status
    INSTALLS += \
        menuicons \
        statusIconScalable
}

# INSTALL
target.path = $$PREFIX/bin
INSTALLS += target
