# Adapt these paths on Windows

#Point this to the boost include folder
INCLUDEPATH += $$quote(C:/qBittorrent/boost_1_51_0)
#Point this to the libtorrent include folder
INCLUDEPATH += $$quote(C:/qBittorrent/RC_0_16/include)

#Point this to the boost lib folder
LIBS += $$quote(-LC:/qBittorrent/boost_1_51_0/stage/lib)
#Point this to the libtorrent lib folder
LIBS += $$quote(-LC:/qBittorrent/RC_0_16/bin/<path-according-to-the-build-options-chosen>)

# LIBTORRENT DEFINES
DEFINES += BOOST_ALL_NO_LIB
DEFINES += BOOST_ASIO_HASH_MAP_BUCKETS=1021
DEFINES += BOOST_ASIO_SEPARATE_COMPILATION
DEFINES += BOOST_EXCEPTION_DISABLE
DEFINES += BOOST_SYSTEM_STATIC_LINK=1
DEFINES += TORRENT_USE_OPENSSL
DEFINES += UNICODE
DEFINES += _UNICODE
DEFINES += WIN32
DEFINES += _WIN32
DEFINES += WIN32_LEAN_AND_MEAN
DEFINES += _WIN32_WINNT=0x0500
DEFINES += _WIN32_IE=0x0500
DEFINES += _CRT_SECURE_NO_DEPRECATE
DEFINES += _SCL_SECURE_NO_DEPRECATE
DEFINES += __USE_W32_SOCKETS
DEFINES += _FILE_OFFSET_BITS=64
DEFINES += WITH_SHIPPED_GEOIP_H

CONFIG(debug, debug|release) {
  DEFINES += TORRENT_DEBUG
} else {
  DEFINES += NDEBUG
}

win32-g++ {
  include(winconf-mingw.pri)
}
else {
  include(winconf-msvc.pri)
}

DEFINES += WITH_GEOIP_EMBEDDED
message("On Windows, GeoIP database must be embedded.")
