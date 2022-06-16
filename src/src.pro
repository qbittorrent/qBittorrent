# Global
TEMPLATE = app
CONFIG += qt thread silent

# Platform specific configuration
win32: include(../winconf.pri)
macx: include(../macxconf.pri)
unix:!macx: include(../unixconf.pri)

QT += network sql xml

macx|*-clang*: QMAKE_CXXFLAGS_WARN_ON += -Wno-range-loop-analysis

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
    win32: lessThan(QT_MAJOR_VERSION, 6) {
        QT += winextras
    }
    macx {
        LIBS += -lobjc
    }
}

nowebui {
    DEFINES += DISABLE_WEBUI
}

stacktrace {
    DEFINES += STACKTRACE

    macx {
        DEFINES += BOOST_STACKTRACE_GNU_SOURCE_NOT_REQUIRED
        QMAKE_LFLAGS += -rdynamic
    }
    unix {
        LIBS += -ldl
        QMAKE_LFLAGS += -rdynamic
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

# Qt defines
DEFINES += \
    QT_DISABLE_DEPRECATED_BEFORE=0x050f02 \
    QT_NO_CAST_FROM_ASCII \
    QT_NO_CAST_TO_ASCII \
    QT_NO_CAST_FROM_BYTEARRAY \
    QT_NO_NARROWING_CONVERSIONS_IN_CONNECT \
    QT_USE_QSTRINGBUILDER \
    QT_STRICT_ITERATORS

INCLUDEPATH += $$PWD

include(app/app.pri)
include(base/base.pri)
!nogui: include(gui/gui.pri)
!nowebui: include(webui/webui.pri)

isEmpty(QMAKE_LRELEASE) {
    win32: QMAKE_LRELEASE = $$[QT_HOST_BINS]/lrelease.exe
    else: QMAKE_LRELEASE = $$[QT_HOST_BINS]/lrelease
    unix {
        equals(QT_MAJOR_VERSION, 5) {
            !exists($$QMAKE_LRELEASE): QMAKE_LRELEASE = lrelease-qt5
        }
    }
    else {
        !exists($$QMAKE_LRELEASE): QMAKE_LRELEASE = lrelease
    }
}
lrelease.input = TS_SOURCES
lrelease.output = ${QMAKE_FILE_PATH}/${QMAKE_FILE_BASE}.qm
lrelease.commands = @echo "lrelease ${QMAKE_FILE_NAME}" && $$QMAKE_LRELEASE -silent ${QMAKE_FILE_NAME} -qm ${QMAKE_FILE_OUT}
lrelease.CONFIG += no_link target_predeps
QMAKE_EXTRA_COMPILERS += lrelease

TRANSLATIONS = $$files($$PWD/lang/qbittorrent_*.ts)
TS_SOURCES += $$TRANSLATIONS

# Resource files
QMAKE_RESOURCE_FLAGS += -compress 9 -threshold 5
RESOURCES += \
    icons/icons.qrc \
    lang/lang.qrc \
    searchengine/searchengine.qrc

DESTDIR = .
