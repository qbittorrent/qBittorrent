# use CONFIG mode first in find_package
set(CMAKE_FIND_PACKAGE_PREFER_CONFIG ON)

macro(find_libtorrent version)
    if (UNIX AND (NOT APPLE) AND (NOT CYGWIN))
        find_package(LibtorrentRasterbar QUIET ${version} COMPONENTS torrent-rasterbar)
        if (NOT LibtorrentRasterbar_FOUND)
            include(FindPkgConfig)
            pkg_check_modules(LibtorrentRasterbar IMPORTED_TARGET GLOBAL "libtorrent-rasterbar>=${version}")
            if (NOT LibtorrentRasterbar_FOUND)
                message(
                    FATAL_ERROR
                    "Package LibtorrentRasterbar >= ${version} not found"
                    " with CMake or pkg-config.\n- Set LibtorrentRasterbar_DIR to a directory containing"
                    " a LibtorrentRasterbarConfig.cmake file or add the installation prefix of LibtorrentRasterbar"
                    " to CMAKE_PREFIX_PATH.\n- Alternatively, make sure there is a valid libtorrent-rasterbar.pc"
                    " file in your system's pkg-config search paths (use the system environment variable PKG_CONFIG_PATH"
                    " to specify additional search paths if needed)."
                )
            endif()
            add_library(LibtorrentRasterbar::torrent-rasterbar ALIAS PkgConfig::LibtorrentRasterbar)
            # force a fake package to show up in the feature summary
            set_property(GLOBAL APPEND PROPERTY
                PACKAGES_FOUND
                "LibtorrentRasterbar via pkg-config (version >= ${version})"
            )
            set_package_properties("LibtorrentRasterbar via pkg-config (version >= ${version})"
                PROPERTIES
                TYPE REQUIRED
            )
        else()
            set_package_properties(LibtorrentRasterbar PROPERTIES TYPE REQUIRED)
        endif()
    else()
        find_package(LibtorrentRasterbar ${version} REQUIRED COMPONENTS torrent-rasterbar)
    endif()
endmacro()

find_libtorrent(${minLibtorrent1Version})
if (LibtorrentRasterbar_FOUND AND (LibtorrentRasterbar_VERSION VERSION_GREATER_EQUAL 2.0))
    find_libtorrent(${minLibtorrentVersion})
endif()

# force variable type so that it always shows up in ccmake/cmake-gui frontends
set_property(CACHE LibtorrentRasterbar_DIR PROPERTY TYPE PATH)
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
