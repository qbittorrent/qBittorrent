# The first path is used when the source is being build by packagers (pbuilder/sbuild/etc)
# The second path is used when you manually run the configure script in the root folder (eg when using qt creator)
exists($$OUT_PWD/../conf.pri) {
    include($$OUT_PWD/../conf.pri)
}
else {
    include(conf.pri)
}

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
qt_translations.files = \
    $$QT_LANG_PATH/qtbase_ar.qm \
    $$QT_LANG_PATH/qtbase_bg.qm \
    $$QT_LANG_PATH/qtbase_ca.qm \
    $$QT_LANG_PATH/qtbase_cs.qm \
    $$QT_LANG_PATH/qtbase_da.qm \
    $$QT_LANG_PATH/qtbase_de.qm \
    $$QT_LANG_PATH/qtbase_es.qm \
    $$QT_LANG_PATH/qtbase_fi.qm \
    $$QT_LANG_PATH/qtbase_fr.qm \
    $$QT_LANG_PATH/qtbase_gd.qm \
    $$QT_LANG_PATH/qtbase_he.qm \
    $$QT_LANG_PATH/qtbase_hu.qm \
    $$QT_LANG_PATH/qtbase_it.qm \
    $$QT_LANG_PATH/qtbase_ja.qm \
    $$QT_LANG_PATH/qtbase_ko.qm \
    $$QT_LANG_PATH/qtbase_lv.qm \
    $$QT_LANG_PATH/qtbase_pl.qm \
    $$QT_LANG_PATH/qtbase_ru.qm \
    $$QT_LANG_PATH/qtbase_sk.qm \
    $$QT_LANG_PATH/qtbase_uk.qm \
    $$QT_LANG_PATH/qtbase_zh_TW.qm \
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
