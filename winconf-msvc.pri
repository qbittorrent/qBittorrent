RC_FILE = qbittorrent.rc

#Adapt the lib names/versions accordingly
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
