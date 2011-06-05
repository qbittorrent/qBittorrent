PREFIX = /usr/local
BINDIR = /usr/local/bin
DATADIR = /usr/local/share

INCLUDEPATH += /usr/local/include/libtorrent /usr/include/openssl /usr/include /opt/local/include/boost /opt/local/include
LIBS += -ltorrent-rasterbar -lcrypto -L/opt/local/lib -lboost_system-mt -lboost_filesystem-mt -lboost_thread-mt -framework Cocoa -framework Carbon -framework IOKit

document_icon.path = Contents/Resources
document_icon.files = Icons/qBitTorrentDocument.icns

QMAKE_BUNDLE_DATA += document_icon
ICON = Icons/qbittorrent_mac.icns
QMAKE_INFO_PLIST = Info.plist

DEFINES += WITH_GEOIP_EMBEDDED
message("On Mac OS X, GeoIP database must be embedded.")
