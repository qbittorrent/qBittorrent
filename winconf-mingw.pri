RC_FILE = qbittorrent_mingw.rc

#Adapt the lib names/versions accordingly
CONFIG(debug, debug|release) {
  LIBS += libtorrent \
          libboost_system-mgw45-mt-d-1_46_1 \
          libboost_filesystem-mgw45-mt-d-1_46_1 \
          libboost_thread-mgw45-mt-d-1_46_1
} else {
  LIBS += libtorrent \
          libboost_system-mgw45-mt-1_46_1 \
          libboost_filesystem-mgw45-mt-1_46_1 \
          libboost_thread-mgw45-mt-1_46_1
}

LIBS += libadvapi32 libshell32
LIBS += libcrypto.dll libssl.dll libwsock32 libws2_32 libz libiconv.dll
LIBS += libpowrprof
