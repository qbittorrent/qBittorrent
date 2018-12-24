# Global
TEMPLATE = app
CONFIG += qt thread silent

# Platform specific configuration
win32: include(../winconf.pri)
macx: include(../macxconf.pri)
unix:!macx: include(../unixconf.pri)

QT += network xml

nogui {
    TARGET = qbittorrent-nox
    QT -= gui
    DEFINES += DISABLE_GUI
} else {
    TARGET = qbittorrent
    QT += xml svg widgets

    CONFIG(static) {
        DEFINES += QBT_STATIC_QT
        QTPLUGIN += qico
    }
    win32 {
        QT += winextras
    }
    macx {
        QT += macextras
        LIBS += -lobjc
    }
}

nowebui {
    DEFINES += DISABLE_WEBUI
}

stacktrace {
    DEFINES += STACKTRACE
    win32 {
        DEFINES += STACKTRACE_WIN_PROJECT_PATH=$$PWD
        DEFINES += STACKTRACE_WIN_MAKEFILE_PATH=$$OUT_PWD
    }
}

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
# Efficient construction for QString & QByteArray (Qt >= 4.8)
DEFINES += QT_USE_QSTRINGBUILDER
DEFINES += QT_STRICT_ITERATORS

INCLUDEPATH += $$PWD

include(app/app.pri)
include(base/base.pri)
!nogui: include(gui/gui.pri)
!nowebui: include(webui/webui.pri)

# Translations
CONFIG += lrelease embed_translations
QMAKE_LRELEASE_FLAGS = -silent
QM_FILES_RESOURCE_PREFIX = /lang
TRANSLATIONS = $$files($$PWD/lang/qbittorrent_*.ts)

equals(QT_MAJOR_VERSION, 5) {  # this include is not needed for Qt >= 5.12.0
    equals(QT_MINOR_VERSION, 1) | equals(QT_MINOR_VERSION, 2) | equals(QT_MINOR_VERSION, 3) \
        | equals(QT_MINOR_VERSION, 4) | equals(QT_MINOR_VERSION, 5) | equals(QT_MINOR_VERSION, 6) \
        | equals(QT_MINOR_VERSION, 7) | equals(QT_MINOR_VERSION, 8) | equals(QT_MINOR_VERSION, 9) \
        | equals(QT_MINOR_VERSION, 10) | equals(QT_MINOR_VERSION, 11) {
        # include() must come after setting the variables
        include(../qmake/lrelease.prf)
    }
}

# Resource files
QMAKE_RESOURCE_FLAGS += -compress 9 -threshold 5
RESOURCES += \
    icons/icons.qrc \
    searchengine/searchengine.qrc

DESTDIR = .
