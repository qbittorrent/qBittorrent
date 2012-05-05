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
LIBS += -lboost_filesystem-mt
# Carbon
LIBS += -framework Carbon -framework IOKit

document_icon.path = Contents/Resources
document_icon.files = Icons/qBitTorrentDocument.icns

QMAKE_BUNDLE_DATA += document_icon
ICON = Icons/qbittorrent_mac.icns
QMAKE_INFO_PLIST = Info.plist

DEFINES += WITH_GEOIP_EMBEDDED
message("On Mac OS X, GeoIP database must be embedded.")
