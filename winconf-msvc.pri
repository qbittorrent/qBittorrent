strace_win:{
  contains(QMAKE_HOST.arch, x86):{
    # i686 arch requires frame pointer preservation
    QMAKE_CXXFLAGS_RELEASE += -Oy-
    QMAKE_CXXFLAGS_DEBUG += -Oy-
  }
  release:{
    QMAKE_CXXFLAGS_RELEASE += -Zi
    QMAKE_LFLAGS += "/DEBUG"
  }
  LIBS += dbghelp.lib
}

winrt {
  QMAKE_CXXFLAGS += -ZW
}

QMAKE_LFLAGS += "/OPT:REF /OPT:ICF"

RC_FILE = qbittorrent.rc

# Enable Wide characters
DEFINES += TORRENT_USE_WPATH

#Adapt the lib names/versions accordingly
CONFIG(staticlibs) {
  message(Building from static libs)
  CONFIG(debug, debug|release) {
    LIBS += libtorrentd.lib \
            libboost_random-vc120-mt-gd-1_57.lib \
            libboost_system-vc120-mt-gd-1_57.lib
  } else {
    LIBS += libtorrent.lib \
            libboost_random-vc120-mt-1_57.lib \
            libboost_system-vc120-mt-1_57.lib
  }
} else {
  message(Building from shared libs)
  CONFIG(debug, debug|release) {
    LIBS += torrentd.lib \
            boost_system-vc120-mt-gd-1_57.lib
  } else {
    LIBS += torrent.lib \
            boost_system-vc120-mt-1_57.lib
  }
}

LIBS += advapi32.lib shell32.lib crypt32.lib User32.lib gdi32.lib ws2_32.lib
LIBS += libeay32.lib ssleay32.lib
LIBS += PowrProf.lib
LIBS += zlib.lib
