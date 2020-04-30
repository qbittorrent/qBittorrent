cmake_minimum_required(VERSION 3.16 FATAL_ERROR)

message(AUTHOR_WARNING "If the build fails, please try the autotools/qmake method.")

if (POLICY CMP0074)
    cmake_policy(SET CMP0074 NEW)
endif()

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules)
include(FunctionReadVersion)

read_version("${CMAKE_CURRENT_SOURCE_DIR}/version.pri" VER_MAJOR VER_MINOR VER_BUGFIX VER_BUILD VER_STATUS)
# message(STATUS "Project version is: ${VER_MAJOR}.${VER_MINOR}.${VER_BUGFIX}.${VER_BUILD} (${VER_STATUS})")

project(qBittorrent VERSION ${VER_MAJOR}.${VER_MINOR}.${VER_BUGFIX}.${VER_BUILD})

# check for invalid compiler version/CXX standard as early as possible
include(FunctionQbtCXXCompilerAndModeCheck)
qbt_minimum_cxx_mode_check(14)
message(STATUS "Building in C++${CMAKE_CXX_STANDARD} mode.\n"
            "Make sure libtorrent was built with the same C++ mode for ABI compatibility.")

set(PROJECT_VERSION "${VER_MAJOR}.${VER_MINOR}.${VER_BUGFIX}")

if (NOT VER_BUILD EQUAL 0)
    set(PROJECT_VERSION "${PROJECT_VERSION}.${VER_BUILD}")
endif()

set(PROJECT_VERSION "${PROJECT_VERSION}${VER_STATUS}")

add_definitions(-DQBT_VERSION_MAJOR=${VER_MAJOR})
add_definitions(-DQBT_VERSION_MINOR=${VER_MINOR})
add_definitions(-DQBT_VERSION_BUGFIX=${VER_BUGFIX})
add_definitions(-DQBT_VERSION_BUILD=${VER_BUILD})

add_definitions(-DQBT_VERSION="v${PROJECT_VERSION}")
add_definitions(-DQBT_VERSION_2="${PROJECT_VERSION}")

include(GNUInstallDirs)
include(FeatureSummary)

# version requirements
set(requiredBoostVersion 1.40)
set(requiredQtVersion 5.9.0)
set(requiredOpensslVersion 1.0)
set(requiredLibtorrentVersion 1.1.10)

if (WIN32)
    include(winconf)
endif()


# we need options here, at the top level, because they are used not only in "src" subdir, but in the "dist" dir too
include(CompileFeature)

optional_compile_definitions(COUNTRIES_RESOLUTION FEATURE DESCRIPTION "Enable resolving peers IP addresses to countries"
    DEFAULT ON DISABLED DISABLE_COUNTRIES_RESOLUTION)
optional_compile_definitions(STACKTRACE FEATURE DESCRIPTION "Enable stacktraces"
    DEFAULT ON ENABLED STACKTRACE)
optional_compile_definitions(WEBUI FEATURE DESCRIPTION "Enables built-in HTTP server for headless use"
    DEFAULT ON DISABLED DISABLE_WEBUI)

add_subdirectory(src)
add_subdirectory(dist)

feature_summary(DESCRIPTION "\nConfiguration results:" WHAT ALL)
