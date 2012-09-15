RC_FILE = qbittorrent.rc

# Enable Wide characters
DEFINES += TORRENT_USE_WPATH

#Adapt the lib names/versions accordingly
CONFIG(debug, debug|release) {
  LIBS += libtorrentd.lib \
          libboost_system-vc90-mt-sgd-1_51.lib
} else {
  LIBS += libtorrent.lib \
          libboost_system-vc90-mt-s-1_51.lib
}

LIBS += advapi32.lib shell32.lib crypt32.lib
LIBS += libeay32MD.lib ssleay32MD.lib
LIBS += PowrProf.lib
