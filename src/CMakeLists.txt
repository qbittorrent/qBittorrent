set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_STANDARD "11")
add_definitions(-DBOOST_NO_CXX11_RVALUE_REFERENCES)

include(MacroQbtCompilerSettings)
qbt_set_compiler_options()

include(MacroLinkQtComponents)
include(QbtTargetSources)

find_package(LibtorrentRasterbar REQUIRED)

# Qt
list(APPEND QBT_QT_COMPONENTS Core Network Xml)
if (GUI)
    list (APPEND QBT_QT_COMPONENTS Concurrent Gui Svg Widgets)
    if (WIN32)
        list (APPEND QBT_QT_COMPONENTS WinExtras)
    endif(WIN32)
    if (APPLE)
        list (APPEND QBT_GUI_OPTIONAL_LINK_LIBRARIES objc)
        list (APPEND QBT_QT_COMPONENTS MacExtras)
    endif (APPLE)
endif (GUI)
if (DBUS)
    list (APPEND QBT_QT_COMPONENTS DBus)
endif (DBUS)
find_package(Qt5 5.5.1 COMPONENTS ${QBT_QT_COMPONENTS} REQUIRED)

if (GUI AND APPLE)
    # Fix MOC inability to detect macOS. This seems to only affect cmake.
    # Relevant issue: https://bugreports.qt.io/browse/QTBUG-58325
    set(CMAKE_AUTOMOC_MOC_OPTIONS ${CMAKE_AUTOMOC_MOC_OPTIONS} -DQ_OS_MAC)
endif ()

set(CMAKE_AUTOMOC True)
list(APPEND CMAKE_AUTORCC_OPTIONS -compress 9 -threshold 5)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# defines
add_definitions(-DQT_NO_CAST_TO_ASCII)
# Efficient construction for QString & QByteArray (Qt >= 4.8)
add_definitions(-DQT_USE_QSTRINGBUILDER)

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

configure_file(config.h.cmakein ${CMAKE_CURRENT_BINARY_DIR}/config.h)

if (SYSTEM_QTSINGLEAPPLICATION)
    find_package(QtSingleApplication REQUIRED)
else (SYSTEM_QTSINGLEAPPLICATION)
    add_subdirectory(app/qtsingleapplication)
endif (SYSTEM_QTSINGLEAPPLICATION)

add_subdirectory(app)
add_subdirectory(base)

if (GUI)
    add_subdirectory(gui)
endif (GUI)

if (WEBUI)
    add_subdirectory(webui)
endif (WEBUI)

