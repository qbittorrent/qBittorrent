# Settings for compiling qBittorrent on Windows

# We want to link with static version of
# libtorrent
set(LibtorrentRasterbar_USE_STATIC_LIBS True)
set(LibtorrentRasterbar_CUSTOM_DEFINITIONS 
    -DBOOST_ALL_NO_LIB -DBOOST_ASIO_HASH_MAP_BUCKETS=1021
    -DBOOST_ASIO_SEPARATE_COMPILATION
    -DBOOST_EXCEPTION_DISABLE
    -DBOOST_SYSTEM_STATIC_LINK=1
    -DTORRENT_USE_OPENSSL
    -D__USE_W32_SOCKETS
    -D_FILE_OFFSET_BITS=64)

add_definitions(-DUNICODE
    -D_UNICODE
    -DWIN32
    -D_WIN32
    -DWIN32_LEAN_AND_MEAN
    -D_WIN32_WINNT=0x0501
    -D_WIN32_IE=0x0500
    -D_CRT_SECURE_NO_DEPRECATE
    -D_SCL_SECURE_NO_DEPRECATE
)
# and boost
set(Boost_USE_STATIC_LIBS  True)
# set(Boost_USE_STATIC_RUNTIME True)

# Here we assume that all required libraries are installed into the same prefix
# with usual unix subdirectories (bin, lib, include)
# if so, we just need to set CMAKE_SYSTEM_PREFIX_PATH
# If it is not the case, individual paths need to be specified manually (see below)
set(COMMON_INSTALL_PREFIX "c:/usr" CACHE PATH "Prefix used to install all the required libraries")
list(APPEND CMAKE_SYSTEM_PREFIX_PATH "${COMMON_INSTALL_PREFIX}")

# If two version of Qt are installed, separate prefixes are needed most likely
set(QT4_INSTALL_PREFIX "${COMMON_INSTALL_PREFIX}/lib/qt4" CACHE PATH "Prefix where Qt4 is installed")
set(QT5_INSTALL_PREFIX "${COMMON_INSTALL_PREFIX}/lib/qt5" CACHE PATH "Prefix where Qt5 is installed")

# it is safe to set Qt dirs even if their files are directly in the prefix
# Qt4
if(NOT QT5)
    # for qt 4 we need qmake, Qt5 provides cmake config files
    LIST(APPEND CMAKE_PROGRAM_PATH  "${QT4_INSTALL_PREFIX}/bin/")
endif(NOT QT5)

# Qt5
set(Qt5_DIR "${QT5_INSTALL_PREFIX}/lib/cmake/Qt5")

# And now we can set specific values for the Boost and libtorrent libraries.
# The following values are generated from the paths listed above just for an example
# they have to be set to actual locations

# Boost
# set(BOOST_ROOT "${COMMON_INSTALL_PREFIX}")
# set(Boost_version_suffix "1_59")
# if a link like boost-version/boost -> boost was created or the boost directory was renamed in the same way,
# the following needs adjustment
# set(BOOST_INCLUDEDIR "${COMMON_INSTALL_PREFIX}/include/boost-${Boost_version_suffix}")
# set(BOOST_LIBRARYDIR "${COMMON_INSTALL_PREFIX}/lib/")

# libtorrent

# set(PC_LIBTORRENT_RASTERBAR_INCLUDEDIR "${COMMON_INSTALL_PREFIX}")
# set(PC_LIBTORRENT_RASTERBAR_LIBDIR "${COMMON_INSTALL_PREFIX}/lib")

set(AUTOGEN_TARGETS_FOLDER "generated")

set(CMAKE_INSTALL_BINDIR ".")

# Test 32/64 bits
if("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
    message(STATUS "Target is 64 bits")
    if (WIN32)
        set(WINXXBITS Win64)
    endif(WIN32)
else("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
    message(STATUS "Target is 32 bits")
    if (WIN32)
        set(WINXXBITS Win32)
    endif(WIN32)
endif("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")

if (MSVC)
    include(winconf-msvc)
else (MSVC)
    include(winconf-mingw)
endif (MSVC)
