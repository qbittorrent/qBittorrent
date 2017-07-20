strace_win {
  contains(QMAKE_HOST.arch, x86) {
    # i686 arch requires frame pointer preservation
    QMAKE_CXXFLAGS_RELEASE += -Oy-
    QMAKE_CXXFLAGS_DEBUG += -Oy-
  }
  release {
    QMAKE_CXXFLAGS_RELEASE += -Zi
    QMAKE_LFLAGS += "/DEBUG"
  }
  LIBS += dbghelp.lib
}

CONFIG -= embed_manifest_exe
QMAKE_LFLAGS += "/MANIFEST:EMBED /MANIFESTINPUT:$$quote($${PWD}/src/qbittorrent.exe.manifest) /STACK:0x800000"
QMAKE_LFLAGS_RELEASE += "/OPT:REF /OPT:ICF"

RC_FILE = qbittorrent.rc

# Adapt the lib names/versions accordingly
CONFIG(debug, debug|release) {
  LIBS += libtorrentd.lib \
          libboost_system-vc140-mt-s-1_64.lib
} else {
  LIBS += libtorrent.lib \
          libboost_system-vc140-mt-s-1_64.lib
}

LIBS += advapi32.lib shell32.lib crypt32.lib User32.lib
LIBS += libeay32.lib ssleay32.lib
LIBS += PowrProf.lib
LIBS += zlib.lib
