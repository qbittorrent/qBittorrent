CONFIG += c++17

# After 1.55 some Windows users reported regular UI freezes.
# This makes ASIO use the pre-1.56 way of doing things. See issue #2003
DEFINES += BOOST_ASIO_DISABLE_CONNECTEX
DEFINES += BOOST_EXCEPTION_DISABLE

DEFINES += TORRENT_USE_LIBCRYPTO
DEFINES += TORRENT_USE_OPENSSL

DEFINES += NTDDI_VERSION=0x06010000
DEFINES += _WIN32_WINNT=0x0601
DEFINES += _WIN32_IE=0x0601
DEFINES += UNICODE
DEFINES += _UNICODE
DEFINES += WIN32
DEFINES += _WIN32
DEFINES += WIN32_LEAN_AND_MEAN
DEFINES += NOMINMAX

CONFIG(debug, debug|release) {
    DEFINES += TORRENT_DEBUG
}
else {
    DEFINES += NDEBUG
}

win32-g++*|win32-clang-g++* {
    CONFIG(debug, debug|release) {
        # Make sure binary is not relocatable, otherwise debugging will fail
        QMAKE_LFLAGS -= -Wl,--dynamicbase
    }

    DEFINES += _FILE_OFFSET_BITS=64
    DEFINES += __USE_W32_SOCKETS

    RC_FILE = qbittorrent_mingw.rc

    LIBS += libadvapi32 libiphlpapi libole32 libpowrprof libshell32 libuser32 libwsock32 libws2_32
}
else:win32-msvc* {
    CONFIG -= embed_manifest_exe

    QMAKE_CXXFLAGS += /utf-8
    QMAKE_LFLAGS += "/MANIFEST:EMBED /MANIFESTINPUT:$$quote($${PWD}/src/qbittorrent.exe.manifest) /STACK:0x800000"

    RC_FILE = qbittorrent.rc

    LIBS += advapi32.lib crypt32.lib Iphlpapi.lib ole32.lib PowrProf.lib shell32.lib User32.lib
}

# See an example build configuration in "conf.pri.windows"
exists(conf.pri) {
    include(conf.pri)
}
else {
    error("'conf.pri' does not exist. See an example configuration in 'conf.pri.windows'.")
}

# Stack trace support can be enabled in 'conf.pri'
stacktrace {
    win32-g++*|win32-clang-g++* {
        QMAKE_LFLAGS += -Wl,--export-dynamic

        contains(QMAKE_HOST.arch, x86) {
            # i686 arch requires frame pointer preservation
            QMAKE_CXXFLAGS += -fno-omit-frame-pointer
        }
    }
    else:win32-msvc* {
        QMAKE_CXXFLAGS += /Zi
        QMAKE_LFLAGS += "/DEBUG /PDBALTPATH:%_PDB%"

        contains(QMAKE_HOST.arch, x86) {
            # i686 arch requires frame pointer preservation
            QMAKE_CXXFLAGS += /Oy-
        }
    }
}
