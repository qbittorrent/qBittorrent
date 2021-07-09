if (UNIX AND (NOT APPLE) AND (NOT CYGWIN))
    find_package(LibtorrentRasterbar QUIET ${minLibtorrentVersion} COMPONENTS torrent-rasterbar)
    if (NOT LibtorrentRasterbar_FOUND)
        include(FindPkgConfig)
        pkg_check_modules(LIBTORRENT_RASTERBAR IMPORTED_TARGET GLOBAL "libtorrent-rasterbar>=${minLibtorrentVersion}")
        if (NOT LIBTORRENT_RASTERBAR_FOUND)
            message(
                FATAL_ERROR
                "Package LibtorrentRasterbar >= ${minLibtorrentVersion} not found"
                " with CMake or pkg-config.\n- Set LibtorrentRasterbar_DIR to a directory containing"
                " a LibtorrentRasterbarConfig.cmake file or add the installation prefix of LibtorrentRasterbar"
                " to CMAKE_PREFIX_PATH.\n- Alternatively, make sure there is a valid libtorrent-rasterbar.pc"
                " file in your system's pkg-config search paths (use the system environment variable PKG_CONFIG_PATH"
                " to specify additional search paths if needed)."
            )
        endif()
        add_library(LibtorrentRasterbar::torrent-rasterbar ALIAS PkgConfig::LIBTORRENT_RASTERBAR)
        # force a fake package to show up in the feature summary
        set_property(GLOBAL APPEND PROPERTY
            PACKAGES_FOUND
            "LibtorrentRasterbar via pkg-config (version >= ${minLibtorrentVersion})"
        )
        set_package_properties("LibtorrentRasterbar via pkg-config (version >= ${minLibtorrentVersion})"
            PROPERTIES
            TYPE REQUIRED
        )
    else()
        set_package_properties(LibtorrentRasterbar PROPERTIES TYPE REQUIRED)
    endif()
else()
    find_package(LibtorrentRasterbar ${minLibtorrentVersion} REQUIRED COMPONENTS torrent-rasterbar)
endif()
# force variable type so that it always shows up in ccmake/cmake-gui frontends
set_property(CACHE LibtorrentRasterbar_DIR PROPERTY TYPE PATH)
find_package(Boost ${minBoostVersion} REQUIRED)
find_package(OpenSSL ${minOpenSSLVersion} REQUIRED)
find_package(ZLIB ${minZlibVersion} REQUIRED)
find_package(Qt6 ${minQtVersion} REQUIRED COMPONENTS Core Network Sql Xml LinguistTools)
if (DBUS)
    find_package(Qt6 ${minQtVersion} REQUIRED COMPONENTS DBus)
    set_package_properties(Qt6DBus PROPERTIES
        DESCRIPTION "Qt6 module for inter-process communication over the D-Bus protocol"
        PURPOSE "Required by the DBUS feature"
    )
endif()

# automatically call Qt moc, rcc and uic as needed for all targets by default
set(CMAKE_AUTORCC_OPTIONS --compress 9 --threshold 5)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# create interface-only target libraries with common compile options/definitions to link to
include(MacroQbtCommonConfig)
qbt_common_config()

# include directories - ideally, would be done per target instead of global directory scope
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(base)

if (GUI)
    find_package(Qt6 ${minQtVersion} REQUIRED COMPONENTS Widgets Svg)
    add_subdirectory(gui)
endif()

if (WEBUI)
    add_subdirectory(webui)
endif()

add_subdirectory(app)
