# Compatibility defines for Qt5
contains(DEFINES, Q_OS_WIN): DEFINES += Q_WS_WIN
contains(DEFINES, Q_OS_WINCE): DEFINES += Q_WS_WINCE
contains(DEFINES, Q_OS_MACX): DEFINES += Q_WS_MAC
contains(DEFINES, Q_OS_LINUX) || contains(DEFINES, Q_OS_UNIX): DEFINES += Q_WS_X11
