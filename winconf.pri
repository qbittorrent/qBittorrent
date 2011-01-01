# Adapt these paths on Windows
INCLUDEPATH += $$quote(C:/qBittorrent/boost_1_44_0)
INCLUDEPATH += $$quote(C:/qBittorrent/RC_0_15/include)
INCLUDEPATH += $$quote(C:/qBittorrent/RC_0_15/zlib)
INCLUDEPATH += $$quote(C:/OpenSSL/include)

LIBS += $$quote(-LC:/OpenSSL/lib/VC)
LIBS += $$quote(-LC:/qBittorrent/libs)

# LIBTORRENT DEFINES
DEFINES += BOOST_ALL_NO_LIB
DEFINES += BOOST_ASIO_HASH_MAP_BUCKETS=1021
DEFINES += BOOST_EXCEPTION_DISABLE
DEFINES += BOOST_SYSTEM_STATIC_LINK=1
DEFINES += BOOST_THREAD_USE_LIB
DEFINES += BOOST_THREAD_USE_LIB=1
DEFINES += TORRENT_USE_OPENSSL
DEFINES += UNICODE
DEFINES += WIN32
DEFINES += WIN32_LEAN_AND_MEAN
DEFINES += _CRT_SECURE_NO_DEPRECATE
DEFINES += _FILE_OFFSET_BITS=64
DEFINES += _SCL_SECURE_NO_DEPRECATE
DEFINES += _UNICODE
DEFINES += _WIN32
DEFINES += _WIN32_WINNT=0x0500
DEFINES += __USE_W32_SOCKETS
DEFINES += WITH_SHIPPED_GEOIP_H

CONFIG(debug, debug|release) {
  DEFINES += TORRENT_DEBUG
} else {
  DEFINES += NDEBUG
}

RC_FILE = qbittorrent.rc

CONFIG(debug, debug|release) {
  LIBS += libtorrentd.lib \
          libboost_system-vc90-mt-gd.lib \
          libboost_filesystem-vc90-mt-gd.lib \
          libboost_thread-vc90-mt-gd.lib
} else {
  LIBS += libtorrent.lib \
          libboost_system-vc90-mt.lib \
          libboost_filesystem-vc90-mt.lib \
          libboost_thread-vc90-mt.lib
}

LIBS += advapi32.lib shell32.lib
LIBS += libeay32MD.lib ssleay32MD.lib
LIBS += PowrProf.lib

DEFINES += WITH_GEOIP_EMBEDDED
message("On Windows, GeoIP database must be embedded.")
