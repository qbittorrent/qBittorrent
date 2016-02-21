# C++11 support
lessThan(QT_MAJOR_VERSION, 5): QMAKE_CXXFLAGS += -std=gnu++11

strace_win{
  contains(QMAKE_HOST.arch, x86) {
    # i686 arch requires frame pointer preservation
    QMAKE_CXXFLAGS_RELEASE += -fno-omit-frame-pointer
    QMAKE_CXXFLAGS_DEBUG += -fno-omit-frame-pointer
  }

  QMAKE_LFLAGS += -Wl,--export-all-symbols

  LIBS += libdbghelp
}

CONFIG(debug, debug|release) {
  # Make sure binary is not relocatable, otherwise debugging will fail
  QMAKE_LFLAGS -= -Wl,--dynamicbase
}

RC_FILE = qbittorrent_mingw.rc

# Adapt the lib names/versions accordingly
CONFIG(debug, debug|release) {
  LIBS += libtorrent-rasterbar \
          libboost_system-mt \
          libboost_filesystem-mt \
          libboost_thread_win32-mt
} else {
  LIBS += libtorrent-rasterbar \
          libboost_system-mt \
          libboost_filesystem-mt \
          libboost_thread_win32-mt
}

LIBS += libadvapi32 libshell32 libuser32
LIBS += libcrypto libssl libwsock32 libws2_32 libz libiconv
LIBS += libpowrprof
