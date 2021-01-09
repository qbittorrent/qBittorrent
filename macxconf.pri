# The first path is used when the source is being build by packagers (pbuilder/sbuild/etc)
# The second path is used when you manually run the configure script in the root folder (eg when using qt creator)
exists($$OUT_PWD/../conf.pri) {
    include($$OUT_PWD/../conf.pri)
}
else {
    include(conf.pri)
}

QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.14

LIBS += -framework Carbon -framework IOKit -framework AppKit

QT_LANG_PATH = ../dist/qt-translations
DIST_PATH = ../dist/mac

document_icon.path = Contents/Resources
document_icon.files = $$DIST_PATH/qBitTorrentDocument.icns
QMAKE_BUNDLE_DATA += document_icon

qt_conf.path = Contents/Resources
qt_conf.files = $$DIST_PATH/qt.conf
QMAKE_BUNDLE_DATA += qt_conf

qt_translations.path = Contents/translations
qt_translations.files = $$files($$QT_LANG_PATH/qtbase_*.qm)
qt_translations.files += \
    $$QT_LANG_PATH/qt_fa.qm \
    $$QT_LANG_PATH/qt_gl.qm \
    $$QT_LANG_PATH/qt_lt.qm \
    $$QT_LANG_PATH/qt_pt.qm \
    $$QT_LANG_PATH/qt_sl.qm \
    $$QT_LANG_PATH/qt_sv.qm \
    $$QT_LANG_PATH/qt_zh_CN.qm
QMAKE_BUNDLE_DATA += qt_translations

ICON = $$DIST_PATH/qbittorrent_mac.icns
QMAKE_INFO_PLIST = $$DIST_PATH/Info.plist
