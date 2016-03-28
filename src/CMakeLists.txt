set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_STANDARD "11")
add_definitions(-DBOOST_NO_CXX11_RVALUE_REFERENCES)

include(MacroLinkQtComponents)

find_package(LibtorrentRasterbar REQUIRED)
include_directories(SYSTEM ${LibtorrentRasterbar_INCLUDE_DIRS})
add_compile_options(${LibtorrentRasterbar_DEFINITIONS})

# Boost
set(Boost_USE_MULTITHREADED ON)
find_package(Boost 1.35 REQUIRED COMPONENTS system)
include_directories(SYSTEM ${Boost_INCLUDE_DIRS})

# Qt
if (QT5)
    add_definitions(-DQBT_USES_QT5)
    list(APPEND QBT_QT_COMPONENTS Core Network Xml)
    if (GUI)
        list (APPEND QBT_QT_COMPONENTS Concurrent Gui Widgets)
    endif (GUI)
    if (DBUS)
        list (APPEND QBT_QT_COMPONENTS DBus)
    endif (DBUS)
    find_package(Qt5 5.2.0 COMPONENTS ${QBT_QT_COMPONENTS} REQUIRED)
else (QT5)
    list(APPEND QBT_QT_COMPONENTS QtCore QtNetwork QtXml)
    if (GUI)
        list (APPEND QBT_QT_COMPONENTS QtGui)
    endif (GUI)
    if (DBUS)
        list (APPEND QBT_QT_COMPONENTS QtDBus)
    endif (DBUS)
    find_package(Qt4 4.8.0 COMPONENTS ${QBT_QT_COMPONENTS} REQUIRED)
    include(${QT_USE_FILE})
endif (QT5)

set(CMAKE_AUTOMOC True)
list(APPEND CMAKE_AUTORCC_OPTIONS -compress 9 -threshold 5)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# defines
add_definitions(-DQT_NO_CAST_TO_ASCII)
# Fast concatenation (Qt >= 4.6)
add_definitions(-DQT_USE_FAST_CONCATENATION -DQT_USE_FAST_OPERATOR_PLUS)
if (WIN32)
    add_definitions(-DNOMINMAX)
endif (WIN32)

if (NOT GUI)
    add_definitions(-DDISABLE_GUI -DDISABLE_COUNTRIES_RESOLUTION)
endif (NOT GUI)

if (NOT WEBUI)
    add_definitions(-DDISABLE_WEBUI)
endif (NOT WEBUI)

if (STACKTRACE_WIN)
    add_definitions(-DSTACKTRACE_WIN)
endif(STACKTRACE_WIN)
# nogui {
#     TARGET = qbittorrent-nox
# } else {
#     CONFIG(static) {
#         DEFINES += QBT_STATIC_QT
#         QTPLUGIN += qico
#     }
#     TARGET = qbittorrent
# }

if (UNIX AND NOT APPLE)
    add_compile_options(-Wformat -Wformat-security)
endif ()

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Project is built in DEBUG mode.")
else (CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Project is built in RELEASE mode.")
    message(STATUS "Disabling debug output.")
    add_definitions(-DQT_NO_DEBUG_OUTPUT)
endif (CMAKE_BUILD_TYPE STREQUAL "Debug")

set(QBT_USE_GUI ${GUI})
set(QBT_USE_WEBUI ${WEBUI})
set(QBT_USES_QT5 ${QT5})

configure_file(config.h.cmakein ${CMAKE_CURRENT_BINARY_DIR}/config.h)


add_subdirectory(base)

if (SYSTEM_QTSINGLEAPPLICATION)
    find_package(QtSingleApplication REQUIRED)
    include_directories(${QTSINGLEAPPLICATION_INCLUDE_DIR})
else (SYSTEM_QTSINGLEAPPLICATION)
    include_directories(app/qtsingleapplication)
endif (SYSTEM_QTSINGLEAPPLICATION)

if (GUI)
    add_subdirectory(gui)
endif (GUI)

if (WEBUI)
    add_subdirectory(webui)
endif (WEBUI)

add_subdirectory(app)
