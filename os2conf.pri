exists(conf.pri) {
    # to the conf.pri goes all system dependent stuff
    include(conf.pri)
}

LIBS += -ltorrent-rasterbar \
        -lboost_thread \
        -lboost_system \
        -lboost_filesystem \
        -lssl -lcrypto -lidn -lpthread

RC_FILE = qbittorrent_os2.rc

# LIBTORRENT DEFINES
DEFINES += WITH_SHIPPED_GEOIP_H
DEFINES += BOOST_ASIO_DYN_LINK

DEFINES += WITH_GEOIP_EMBEDDED
message("On eCS(OS/2), GeoIP database must be embedded.")
