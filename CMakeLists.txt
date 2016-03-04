cmake_minimum_required(VERSION 3.2)
cmake_policy(VERSION 3.2)

project(qBittorrent VERSION 3.4.0.0)

set(VER_MAJOR ${qBittorrent_VERSION_MAJOR})
set(VER_MINOR ${qBittorrent_VERSION_MINOR})
set(VER_BUGFIX ${qBittorrent_VERSION_PATCH})
set(VER_BUILD ${qBittorrent_VERSION_TWEAK})
set(VER_STATUS "alpha") # Should be empty for stable releases!

# Don't touch the rest part
set(PROJECT_VERSION "${VER_MAJOR}.${VER_MINOR}.${VER_BUGFIX}")

if (NOT VER_BUILD EQUAL 0)
    set(PROJECT_VERSION "${PROJECT_VERSION}.${VER_BUILD}")
endif()

set(PROJECT_VERSION "${PROJECT_VERSION}${VER_STATUS}")

add_definitions(-DVERSION_MAJOR=${VER_MAJOR})
add_definitions(-DVERSION_MINOR=${VER_MINOR})
add_definitions(-DVERSION_BUGFIX=${VER_BUGFIX})
add_definitions(-DVERSION_BUILD=${VER_BUILD})

# os2 {
#     DEFINES += VERSION=\'\"v$${PROJECT_VERSION}\"\'
# } else {
add_definitions(-DVERSION="v${PROJECT_VERSION}")
# }
list(APPEND CMAKE_MODULE_PATH ${qBittorrent_SOURCE_DIR}/cmake/Modules)

# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Og")
if (UNIX AND NOT APPLE)
    include(GNUInstallDirs)
endif (UNIX AND NOT APPLE)

if(WIN32)
    include(winconf)
endif(WIN32)

# we need options here, because they are used not only in "src" subdir, but in the "dist" dir too
include(CMakeDependentOption)

option(QT5 "Compile using Qt5" ON)
option(SYSTEM_QTSINGLEAPPLICATION
    "Use the system qtsingleapplication library or shipped one otherwise")
cmake_dependent_option(SYSTEM_QJSON
    "Use the shipped qjson library or the system one (Qt4 only)" OFF "NOT QT5" OFF)

option(GUI "Allows to disable GUI for headless running. Disables QtDBus and the GeoIP Database" ON)

option(WEBUI "Allows to disable the WebUI." ON)

if (WIN32)
    option(STACKTRACE_WIN "")
else (WIN32)
    cmake_dependent_option(SYSTEMD "Install the systemd service file (headless only)" OFF
        "NOT GUI" OFF)
    cmake_dependent_option(DBUS "Enable use of QtDBus (GUI only)" ON "GUI" OFF)
endif(WIN32)


add_subdirectory(src)
add_subdirectory(dist)
