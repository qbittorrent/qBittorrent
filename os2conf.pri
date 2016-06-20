# C++11 support
lessThan(QT_MAJOR_VERSION, 5): QMAKE_CXXFLAGS += -std=c++11

exists(conf.pri) {
    # to the conf.pri goes all system dependent stuff
    include(conf.pri)
}

LIBS += \
    -ltorrent-rasterbar \
    -lboost_thread \
    -lboost_system \
    -lboost_filesystem \
    -lssl -lcrypto -lidn -lpthread -lz

RC_FILE = qbittorrent_os2.rc

# LIBTORRENT DEFINES
DEFINES += BOOST_ASIO_DYN_LINK
