# Adapt these paths on Windows
INCLUDEPATH += $$quote($$PWD/boost_1_42_0)
INCLUDEPATH += $$quote($$PWD/libtorrent-rasterbar-0.14.10/include)
INCLUDEPATH += $$quote($$PWD/libtorrent-rasterbar-0.14.10/zlib)
INCLUDEPATH += $$quote(C:/OpenSSL/include)

LIBS += $$quote(-LC:/OpenSSL/lib/VC)
LIBS += $$quote(-L$$PWD/libs)
