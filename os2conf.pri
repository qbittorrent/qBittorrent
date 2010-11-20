LIBS += -ltorrent-rasterbar \
        -lboost_thread \
        -lboost_system \
        -lboost_filesystem \
        -lssl -lcrypto -lidn -lpthread

RC_FILE = qbittorrent_os2.rc

DEFINES += WITH_GEOIP_EMBEDDED
message("On eCS(OS/2), GeoIP database must be embedded.")
