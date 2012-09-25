PREFIX = /usr/local
BINDIR = /usr/local/bin
DATADIR = /usr/local/share

# Use pkg-config to get all necessary libtorrent DEFINES
CONFIG += link_pkgconfig
PKGCONFIG += libtorrent-rasterbar
DEFINES += BOOST_ASIO_DYN_LINK

# Special include/libs paths (macports)
INCLUDEPATH += /usr/include/openssl /usr/include /opt/local/include/boost /opt/local/include
LIBS += -L/opt/local/lib

# OpenSSL lib
LIBS += -lssl -lcrypto
# Boost system lib
LIBS += -lboost_system-mt
# Boost filesystem lib (Not needed for libtorrent >= 0.16.0)
#LIBS += -lboost_filesystem-mt
# Carbon
LIBS += -framework Carbon -framework IOKit

document_icon.path = Contents/Resources
document_icon.files = mac/qBitTorrentDocument.icns
QMAKE_BUNDLE_DATA += document_icon

qt_conf.path = Contents/Resources
qt_conf.files = mac/qt.conf
QMAKE_BUNDLE_DATA += qt_conf

qt_translations.path = Contents/MacOS/translations
qt_translations.files = qt-translations/qt_ar.qm \
                        qt-translations/qt_bg.qm \
                        qt-translations/qt_ca.qm \
                        qt-translations/qt_cs.qm \
                        qt-translations/qt_da.qm \
                        qt-translations/qt_de.qm \
                        qt-translations/qt_es.qm \
                        qt-translations/qt_fi.qm \
                        qt-translations/qt_fr.qm \
                        qt-translations/qt_gl.qm \
                        qt-translations/qt_he.qm \
                        qt-translations/qt_hu.qm \
                        qt-translations/qt_it.qm \
                        qt-translations/qt_ja.qm \
                        qt-translations/qt_ko.qm \
                        qt-translations/qt_lt.qm \
                        qt-translations/qt_nl.qm \
                        qt-translations/qt_pl.qm \
                        qt-translations/qt_pt.qm \
                        qt-translations/qt_pt_BR.qm \
                        qt-translations/qt_ru.qm \
                        qt-translations/qt_sk.qm \
                        qt-translations/qt_sv.qm \
                        qt-translations/qt_tr.qm \
                        qt-translations/qt_uk.qm \
                        qt-translations/qt_zh_CN.qm \
                        qt-translations/qt_zh_TW.qm
QMAKE_BUNDLE_DATA += qt_translations

ICON = mac/qbittorrent_mac.icns
QMAKE_INFO_PLIST = mac/Info.plist

DEFINES += WITH_GEOIP_EMBEDDED
message("On Mac OS X, GeoIP database must be embedded.")
