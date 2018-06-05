# - Try to find libtorrent-rasterbar
#
# If not using pkg-config, you can pre-set LibtorrentRasterbar_CUSTOM_DEFINITIONS
# for definitions unrelated to Boost's separate compilation (which are already
# decided by the LibtorrentRasterbar_USE_STATIC_LIBS variable).
#
# Once done this will define
#  LibtorrentRasterbar_FOUND - System has libtorrent-rasterbar
#  LibtorrentRasterbar_INCLUDE_DIRS - The libtorrent-rasterbar include directories
#  LibtorrentRasterbar_LIBRARIES - The libraries needed to use libtorrent-rasterbar
#  LibtorrentRasterbar_DEFINITIONS - Compiler switches required for using libtorrent-rasterbar
#  LibtorrentRasterbar_OPENSSL_ENABLED - libtorrent-rasterbar uses and links against OpenSSL

find_package(Threads REQUIRED)
find_package(PkgConfig QUIET)

macro(_detect_boost_components _outComponets librariesList)
    string(REGEX MATCHALL "boost_[a-z_]+[-a-z]*" _boost_libraries "${librariesList}")
    string(REGEX REPLACE "boost_([a-z_]+)[-a-z]*" "\\1" ${_outComponets} "${_boost_libraries}")
endmacro()

if(PKG_CONFIG_FOUND)
    pkg_check_modules(PC_LIBTORRENT_RASTERBAR QUIET libtorrent-rasterbar)
endif()

if(LibtorrentRasterbar_USE_STATIC_LIBS)
    set(LibtorrentRasterbar_ORIG_CMAKE_FIND_LIBRARY_SUFFIXES ${CMAKE_FIND_LIBRARY_SUFFIXES})
    set(CMAKE_FIND_LIBRARY_SUFFIXES ${CMAKE_STATIC_LIBRARY_SUFFIX})
endif()

if(PC_LIBTORRENT_RASTERBAR_FOUND)
    set(LibtorrentRasterbar_DEFINITIONS ${PC_LIBTORRENT_RASTERBAR_CFLAGS})
else()
    if(LibtorrentRasterbar_CUSTOM_DEFINITIONS)
        set(LibtorrentRasterbar_DEFINITIONS ${LibtorrentRasterbar_CUSTOM_DEFINITIONS})
    else()
        # Without pkg-config, we can't possibly figure out the correct build flags.
        # libtorrent is very picky about those. Let's take a set of defaults and
        # hope that they apply. If not, you the user are on your own.
        set(LibtorrentRasterbar_DEFINITIONS
            -DTORRENT_USE_OPENSSL
            -DTORRENT_DISABLE_GEO_IP
            -DBOOST_ASIO_ENABLE_CANCELIO
            -DUNICODE -D_UNICODE -D_FILE_OFFSET_BITS=64)
    endif()

    if(NOT LibtorrentRasterbar_USE_STATIC_LIBS)
        list(APPEND LibtorrentRasterbar_DEFINITIONS
            -DTORRENT_LINKING_SHARED
            -DBOOST_SYSTEM_DYN_LINK -DBOOST_CHRONO_DYN_LINK)
    endif()
endif()

message(STATUS "libtorrent definitions: ${LibtorrentRasterbar_DEFINITIONS}")

find_path(LibtorrentRasterbar_INCLUDE_DIR libtorrent
          HINTS ${PC_LIBTORRENT_RASTERBAR_INCLUDEDIR} ${PC_LIBTORRENT_RASTERBAR_INCLUDE_DIRS}
          PATH_SUFFIXES libtorrent-rasterbar)

find_library(LibtorrentRasterbar_LIBRARY NAMES torrent-rasterbar libtorrent
             HINTS ${PC_LIBTORRENT_RASTERBAR_LIBDIR} ${PC_LIBTORRENT_RASTERBAR_LIBRARY_DIRS})

if(LibtorrentRasterbar_USE_STATIC_LIBS)
    set(CMAKE_FIND_LIBRARY_SUFFIXES ${LibtorrentRasterbar_ORIG_CMAKE_FIND_LIBRARY_SUFFIXES})
endif()

set(LibtorrentRasterbar_LIBRARIES ${LibtorrentRasterbar_LIBRARY} ${CMAKE_THREAD_LIBS_INIT})
set(LibtorrentRasterbar_INCLUDE_DIRS ${LibtorrentRasterbar_INCLUDE_DIR})

# Without pkg-config, we can't possibly figure out the correct boost dependencies
if (LibtorrentRasterbar_CUSTOM_BOOST_DEPENDENCIES)
    set(_boost_components "${LibtorrentRasterbar_CUSTOM_BOOST_DEPENDENCIES}")
else(LibtorrentRasterbar_CUSTOM_BOOST_DEPENDENCIES)
    if(PC_LIBTORRENT_RASTERBAR_FOUND)
        _detect_boost_components(_boost_components "${PC_LIBTORRENT_RASTERBAR_LIBRARIES}")
    else()
        # all possible boost dependencies
        set(_boost_components
            date_time
            system
            chrono
            random
            thread
        )
    endif()
endif(LibtorrentRasterbar_CUSTOM_BOOST_DEPENDENCIES)

list(SORT _boost_components)
message(STATUS "Libtorrent Boost dependencies: ${_boost_components}")
find_package(Boost REQUIRED COMPONENTS ${_boost_components})
set(LibtorrentRasterbar_LIBRARIES ${LibtorrentRasterbar_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
foreach(_boost_cmpnt IN LISTS _boost_components)
    list(APPEND LibtorrentRasterbar_LIBRARIES "Boost::${_boost_cmpnt}")
endforeach(_boost_cmpnt)

set(LibtorrentRasterbar_INCLUDE_DIRS ${LibtorrentRasterbar_INCLUDE_DIRS})

list(FIND LibtorrentRasterbar_DEFINITIONS -DTORRENT_USE_OPENSSL LibtorrentRasterbar_ENCRYPTION_INDEX)
if(LibtorrentRasterbar_ENCRYPTION_INDEX GREATER -1)
    find_package(OpenSSL REQUIRED)
    set(LibtorrentRasterbar_LIBRARIES ${LibtorrentRasterbar_LIBRARIES} OpenSSL::SSL OpenSSL::Crypto)
    list(APPEND LibtorrentRasterbar_INCLUDE_DIRS "${OPENSSL_INCLUDE_DIR}")
    set(LibtorrentRasterbar_OPENSSL_ENABLED ON)
endif()

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set LibtorrentRasterbar_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(LibtorrentRasterbar DEFAULT_MSG
                                  LibtorrentRasterbar_LIBRARY
                                  LibtorrentRasterbar_INCLUDE_DIR)

mark_as_advanced(LibtorrentRasterbar_INCLUDE_DIR LibtorrentRasterbar_LIBRARY
    LibtorrentRasterbar_ORIG_CMAKE_FIND_LIBRARY_SUFFIXES
    LibtorrentRasterbar_ENCRYPTION_INDEX)

if (LibtorrentRasterbar_FOUND AND NOT TARGET LibtorrentRasterbar::torrent-rasterbar)
    add_library(LibtorrentRasterbar::torrent-rasterbar UNKNOWN IMPORTED)

    set_target_properties(LibtorrentRasterbar::torrent-rasterbar PROPERTIES
        IMPORTED_LINK_INTERFACE_LANGUAGES "CXX"
        IMPORTED_LOCATION "${LibtorrentRasterbar_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${LibtorrentRasterbar_INCLUDE_DIRS}"
        INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${LibtorrentRasterbar_INCLUDE_DIRS}"
        INTERFACE_LINK_LIBRARIES "${LibtorrentRasterbar_LIBRARIES}"
        INTERFACE_COMPILE_OPTIONS "${LibtorrentRasterbar_DEFINITIONS}"
    )
endif()
