# The first path is used when the source is being build by packagers (pbuilder/sbuild/etc)
# The second path is used when you manually run the configure script in the root folder (eg when using qt creator)
exists($$OUT_PWD/../conf.pri) {
    include($$OUT_PWD/../conf.pri)
}
else {
    include(conf.pri)
}

# Custom function
# Return Qt translations files as list of paths
# It will return .qm files of qt/qtbase that aren't stub files.
defineReplace(qbt_get_qt_translations) {
    # The $$[] syntax queries qmake properties
    TMP_TRANSLATIONS = $$files($$[QT_INSTALL_TRANSLATIONS]/qt_??.qm)
    TMP_TRANSLATIONS += $$files($$[QT_INSTALL_TRANSLATIONS]/qt_??_??.qm)
    TMP_TRANSLATIONS += $$files($$[QT_INSTALL_TRANSLATIONS]/qtbase_??.qm)
    TMP_TRANSLATIONS += $$files($$[QT_INSTALL_TRANSLATIONS]/qtbase_??_??.qm)

   # Consider files less than 10KB as stub translations
   for (TRANSLATION, TMP_TRANSLATIONS) {
      TRANSLATION_SIZE = $$system("stat -f%z $${TRANSLATION}", true, EXIT_STATUS)
      equals(EXIT_STATUS, 0):!lessThan(TRANSLATION_SIZE, 10240): FINAL_TRANSLATIONS += $${TRANSLATION}
   }

   return($$FINAL_TRANSLATIONS)
}

QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.15

DEFINES += _DARWIN_FEATURE_64_BIT_INODE

LIBS += -framework Carbon -framework IOKit -framework AppKit

DIST_PATH = ../dist/mac

document_icon.path = Contents/Resources
document_icon.files = $$DIST_PATH/qBitTorrentDocument.icns
QMAKE_BUNDLE_DATA += document_icon

qt_conf.path = Contents/Resources
qt_conf.files = $$DIST_PATH/qt.conf
QMAKE_BUNDLE_DATA += qt_conf

qt_translations.path = Contents/translations
qt_translations.files =  $$qbt_get_qt_translations()
QMAKE_BUNDLE_DATA += qt_translations

ICON = $$DIST_PATH/qbittorrent_mac.icns
QMAKE_INFO_PLIST = $$DIST_PATH/Info.plist
