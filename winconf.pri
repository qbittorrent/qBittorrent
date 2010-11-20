# Adapt these paths on Windows
INCLUDEPATH += $$quote($$PWD/boost_1_42_0)
INCLUDEPATH += $$quote($$PWD/libtorrent-rasterbar-0.14.10/include)
INCLUDEPATH += $$quote($$PWD/libtorrent-rasterbar-0.14.10/zlib)
INCLUDEPATH += $$quote(C:/OpenSSL/include)

LIBS += $$quote(-LC:/OpenSSL/lib/VC)
LIBS += $$quote(-L$$PWD/libs)

# LIBTORRENT DEFINES
DEFINES += BOOST_ALL_NO_LIB BOOST_ASIO_HASH_MAP_BUCKETS=1021 BOOST_EXCEPTION_DISABLE
DEFINES += BOOST_FILESYSTEM_STATIC_LINK=1 BOOST_MULTI_INDEX_DISABLE_SERIALIZATION
DEFINES += BOOST_SYSTEM_STATIC_LINK=1 BOOST_THREAD_USE_LIB BOOST_THREAD_USE_LIB=1
DEFINES += TORRENT_USE_OPENSSL UNICODE WIN32 WIN32_LEAN_AND_MEAN
DEFINES += _CRT_SECURE_NO_DEPRECATE _FILE_OFFSET_BITS=64 _SCL_SECURE_NO_DEPRECATE
DEFINES += _UNICODE _WIN32 _WIN32_WINNT=0x0500 __USE_W32_SOCKETS

debug {
  DEFINES += TORRENT_DEBUG
} else {
  DEFINES += NDEBUG
}

RC_FILE = qbittorrent.rc

debug {
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

DEFINES += WITH_GEOIP_EMBEDDED
message("On Windows, GeoIP database must be embedded.")
