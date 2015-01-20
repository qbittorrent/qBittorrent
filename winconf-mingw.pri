strace_win:{
  contains(QMAKE_HOST.arch, x86):{
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

#You need to link with libtorrent > 0.15.5 (or svn) and you must
#configure libtorrent to use iconv in the building process. This is
#needed for correct Unicode support.

#Adapt the lib names/versions accordingly
CONFIG(debug, debug|release) {
  LIBS += libtorrent \
          libboost_system-mgw45-mt-d-1_47 \
          libboost_filesystem-mgw45-mt-d-1_47 \
          libboost_thread-mgw45-mt-d-1_47
} else {
  LIBS += libtorrent \
          libboost_system-mgw45-mt-1_47 \
          libboost_filesystem-mgw45-mt-1_47 \
          libboost_thread-mgw45-mt-1_47
}

LIBS += libadvapi32 libshell32 libuser32
LIBS += libcrypto.dll libssl.dll libwsock32 libws2_32 libz libiconv.dll
LIBS += libpowrprof
