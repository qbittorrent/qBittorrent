# use CONFIG mode first in find_package
set(CMAKE_FIND_PACKAGE_PREFER_CONFIG ON)

CPMAddPackage("gh:arvidn/libtorrent@2.0.10")

# force variable type so that it always shows up in ccmake/cmake-gui frontends
find_package(Boost ${minBoostVersion} REQUIRED)
find_package(OpenSSL ${minOpenSSLVersion} REQUIRED)
find_package(ZLIB ${minZlibVersion} REQUIRED)
find_package(Qt6 ${minQt6Version} REQUIRED COMPONENTS Core Network Sql Xml LinguistTools)
if (DBUS)
    find_package(Qt6 ${minQt6Version} REQUIRED COMPONENTS DBus)
    set_package_properties(Qt6DBus PROPERTIES
        DESCRIPTION "Qt6 module for inter-process communication over the D-Bus protocol"
        PURPOSE "Required by the DBUS feature"
    )
endif()
