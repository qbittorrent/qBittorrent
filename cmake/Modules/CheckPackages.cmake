# use CONFIG mode first in find_package
set(CMAKE_FIND_PACKAGE_PREFER_CONFIG ON)


include(FetchContent)
# to debug only
# set(FETCHCONTENT_QUIET FALSE)

set(BOOST_DL_VERSION 1.88.0)
set(LIBTORRENT_DL_VERSION 2.0.11)

# headers only, no build
string(REPLACE "." "_" BOOST_DL_VERSION_ ${BOOST_DL_VERSION})
FetchContent_Populate(Boost
    URL https://archives.boost.io/release/${BOOST_DL_VERSION}/source/boost_${BOOST_DL_VERSION_}.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP ON
    SUBBUILD_DIR ${CMAKE_BINARY_DIR}/_deps/boost-subbuild
    SOURCE_DIR ${CMAKE_BINARY_DIR}/_deps/boost-src
)
# ref: https://cmake.org/cmake/help/latest/module/FindBoost.html
set(BOOST_ROOT ${boost_SOURCE_DIR})

FetchContent_Declare(LibtorrentRasterbar
    # CMAKE_ARG "-D BUILD_SHARED_LIBS=OFF"
    URL https://github.com/arvidn/libtorrent/releases/download/v${LIBTORRENT_DL_VERSION}/libtorrent-rasterbar-${LIBTORRENT_DL_VERSION}.tar.gz
    OVERRIDE_FIND_PACKAGE
    DOWNLOAD_EXTRACT_TIMESTAMP ON
)


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
            add_library(LibtorrentRasterbar::torrent-rasterbar ALIAS torrent-rasterbar)
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

# FetchContent doesn't declare PACKAGE_VERSION in <package>-config-version.cmake
# so we need to define it for later use after every find_package call as it will reset it
# ref: https://gitlab.kitware.com/cmake/cmake/-/issues/24636
set(LibtorrentRasterbar_VERSION LIBTORRENT_DL_VERSION)
