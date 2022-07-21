cmake_minimum_required(VERSION 3.16 FATAL_ERROR) # Policies <= CMP0097 default to NEW

project(qBittorrent
    DESCRIPTION "The qBittorrent BitTorrent client"
    HOMEPAGE_URL "https://www.qbittorrent.org/"
    LANGUAGES CXX
)

# version requirements - older versions may work, but you are on your own
set(minBoostVersion 1.71)
set(minQt5Version 5.15.2)
set(minQt6Version 6.2)
set(minOpenSSLVersion 1.1.1)
set(minLibtorrent1Version 1.2.14)
set(minLibtorrentVersion 2.0.7)
set(minZlibVersion 1.2.11)

include(CheckCXXSourceCompiles) # TODO: migrate to CheckSourceCompiles in CMake >= 3.19
include(GNUInstallDirs)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules)
include(FeatureSummary)
include(FeatureOptionsSetup)

# features, list is loosely sorted by user's interests
feature_option(GUI "Build GUI application" ON)
feature_option(WEBUI "Enable built-in HTTP server for remote control" ON)
feature_option(QT6 "Build with Qt 6 instead of Qt 5" OFF)
feature_option(STACKTRACE "Enable stacktrace support" ON)
feature_option(TESTING "Build internal testing suite" OFF)
feature_option(VERBOSE_CONFIGURE "Show information about PACKAGES_FOUND and PACKAGES_NOT_FOUND in the configure output (only useful for debugging the CMake build scripts)" OFF)

if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    feature_option_dependent(DBUS
        "Enable support for notifications and power-management features via D-Bus on Linux"
        ON "GUI" OFF
    )
    feature_option_dependent(SYSTEMD
        "Install systemd service file. Target directory is overridable with `SYSTEMD_SERVICES_INSTALL_DIR` variable"
        OFF "NOT GUI" OFF
    )
elseif (MSVC)
    feature_option(MSVC_RUNTIME_DYNAMIC "Use MSVC dynamic runtime library (-MD) instead of static (-MT)" ON)
endif()

if (VERBOSE_CONFIGURE)
    feature_summary(WHAT ALL)
else()
    feature_summary(WHAT ENABLED_FEATURES DISABLED_FEATURES)
endif()

# go check the packages
include(CheckPackages)
# configure for specific platform
include(CommonConfig)

# Generate version header
configure_file("src/base/version.h.in" "${CMAKE_CURRENT_SOURCE_DIR}/src/base/version.h" @ONLY)

add_subdirectory(src)
add_subdirectory(dist)

if (TESTING)
    add_subdirectory(test)
endif()
