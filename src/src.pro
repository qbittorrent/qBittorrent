# Global
TEMPLATE = app
CONFIG += qt thread silent

# C++11 support
CONFIG += c++11
DEFINES += BOOST_NO_CXX11_RVALUE_REFERENCES

# Windows specific configuration
win32: include(../winconf.pri)

# Mac specific configuration
macx: include(../macxconf.pri)

# Unix specific configuration
unix:!macx: include(../unixconf.pri)

# eCS(OS/2) specific configuration
os2: include(../os2conf.pri)

nogui {
    QT -= gui
    TARGET = qbittorrent-nox
} else {
    QT += xml
    greaterThan(QT_MAJOR_VERSION, 4): QT += concurrent widgets
    CONFIG(static) {
        DEFINES += QBT_STATIC_QT
        QTPLUGIN += qico
    }
    TARGET = qbittorrent
}
strace_win {
    DEFINES += STACKTRACE_WIN
    DEFINES += STACKTRACE_WIN_PROJECT_PATH=$$PWD
    DEFINES += STACKTRACE_WIN_MAKEFILE_PATH=$$OUT_PWD
}
QT += network xml


CONFIG(debug, debug|release): message(Project is built in DEBUG mode.)
CONFIG(release, debug|release): message(Project is built in RELEASE mode.)

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

win32 {
    DEFINES += NOMINMAX
    QBT_USES_QT5 = 0
    QBT_NOGUI = 0
    QBT_NOWEBUI = 0
    greaterThan(QT_MAJOR_VERSION, 4): greaterThan(QT_MINOR_VERSION, 1): QBT_USES_QT5 = 1
    nogui: QBT_NOGUI = 1
    nowebui: QBT_NOWEBUI = 1
    configh.input = config.h.win.in
    configh.output = config.h
    QMAKE_SUBSTITUTES += configh
}

INCLUDEPATH += $$PWD

include(app/app.pri)
include(base/base.pri)
!nowebui: include(webui/webui.pri)
!nogui: include(gui/gui.pri)

# Resource files
QMAKE_RESOURCE_FLAGS += -compress 9 -threshold 5
RESOURCES += \
    icons.qrc \
    lang.qrc \
    searchengine.qrc

# Translations
TRANSLATIONS += $$files(lang/qbittorrent_*.ts)

DESTDIR = .
