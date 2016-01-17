# Global
TEMPLATE = app
CONFIG += qt thread silent

# C++11 support
CONFIG += c++11
DEFINES += BOOST_NO_CXX11_RVALUE_REFERENCES
greaterThan(QT_MAJOR_VERSION, 4): DEFINES += QBT_USES_QT5

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
    DEFINES += DISABLE_GUI
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
nowebui: DEFINES += DISABLE_WEBUI
strace_win: DEFINES += STACKTRACE_WIN
QT += network xml

# Vars
LANG_PATH = lang

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

win32: DEFINES += NOMINMAX

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
TRANSLATIONS = \
    $$LANG_PATH/qbittorrent_fr.ts \
    $$LANG_PATH/qbittorrent_zh.ts \
    $$LANG_PATH/qbittorrent_zh_TW.ts \
    $$LANG_PATH/qbittorrent_zh_HK.ts \
    $$LANG_PATH/qbittorrent_en.ts \
    $$LANG_PATH/qbittorrent_en_AU.ts \
    $$LANG_PATH/qbittorrent_en_GB.ts \
    $$LANG_PATH/qbittorrent_ca.ts \
    $$LANG_PATH/qbittorrent_es.ts \
    $$LANG_PATH/qbittorrent_eo.ts \
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
    $$LANG_PATH/qbittorrent_id.ts \
    $$LANG_PATH/qbittorrent_it.ts \
    $$LANG_PATH/qbittorrent_sk.ts \
    $$LANG_PATH/qbittorrent_sl.ts \
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
    $$LANG_PATH/qbittorrent_he.ts \
    $$LANG_PATH/qbittorrent_vi.ts \
    $$LANG_PATH/qbittorrent_hi_IN.ts

DESTDIR = .
