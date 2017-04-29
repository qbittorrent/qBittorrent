cmake_minimum_required(VERSION 3.5)
cmake_policy(VERSION 3.5)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules)
include(FunctionReadVersion)

read_version("${CMAKE_CURRENT_SOURCE_DIR}/version.pri" VER_MAJOR VER_MINOR VER_BUGFIX VER_BUILD VER_STATUS)
# message(STATUS "Project version is: ${VER_MAJOR}.${VER_MINOR}.${VER_BUGFIX}.${VER_BUILD} (${VER_STATUS})")

project(qBittorrent VERSION ${VER_MAJOR}.${VER_MINOR}.${VER_BUGFIX}.${VER_BUILD})

set(PROJECT_VERSION "${VER_MAJOR}.${VER_MINOR}.${VER_BUGFIX}")

if (NOT VER_BUILD EQUAL 0)
    set(PROJECT_VERSION "${PROJECT_VERSION}.${VER_BUILD}")
endif()

set(PROJECT_VERSION "${PROJECT_VERSION}${VER_STATUS}")

add_definitions(-DQBT_VERSION_MAJOR=${VER_MAJOR})
add_definitions(-DQBT_VERSION_MINOR=${VER_MINOR})
add_definitions(-DQBT_VERSION_BUGFIX=${VER_BUGFIX})
add_definitions(-DQBT_VERSION_BUILD=${VER_BUILD})

# os2 {
#     DEFINES += DQBT_VERSION=\'\"v$${PROJECT_VERSION}\"\'
#     DEFINES += DQBT_VERSION_2=\'\"$${PROJECT_VERSION}\"\'
# } else {
add_definitions(-DQBT_VERSION="v${PROJECT_VERSION}")
add_definitions(-DQBT_VERSION_2="${PROJECT_VERSION}")
# }

# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Og")
if (UNIX AND NOT APPLE)
    include(GNUInstallDirs)
endif (UNIX AND NOT APPLE)

if(WIN32)
    include(winconf)
endif(WIN32)

# we need options here, because they are used not only in "src" subdir, but in the "dist" dir too
include(CMakeDependentOption)

option(SYSTEM_QTSINGLEAPPLICATION
    "Use the system qtsingleapplication library or shipped one otherwise")

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
