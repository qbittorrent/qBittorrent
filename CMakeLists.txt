cmake_minimum_required(VERSION 3.16 FATAL_ERROR) # Policies <= CMP0097 default to NEW

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules)

project(qBittorrent
    DESCRIPTION "The qBittorrent BitTorrent client"
    HOMEPAGE_URL "https://www.qbittorrent.org/"
    LANGUAGES CXX
)

# use CONFIG mode first in find_package
set(CMAKE_FIND_PACKAGE_PREFER_CONFIG ON)
# version requirements - older versions may work, but you are on your own
set(minBoostVersion 1.65)
set(minQt5Version 5.15.2)
set(minQt6Version 6.2)
set(minOpenSSLVersion 1.1.1)
set(minLibtorrent1Version 1.2.14)
set(minLibtorrentVersion 2.0.4)
set(minZlibVersion 1.2.11)

# features (some are platform-specific)
include(CheckCXXSourceCompiles) # TODO: migrate to CheckSourceCompiles in CMake >= 3.19
include(FeatureSummary)
include(FeatureOptionsSetup)
feature_option(QT6 "Use Qt6" OFF)
feature_option(STACKTRACE "Enable stacktraces" ON)
feature_option(GUI "Build GUI application" ON)
feature_option(WEBUI "Enables built-in HTTP server for headless use" ON)
feature_option(VERBOSE_CONFIGURE "Show information about PACKAGES_FOUND and PACKAGES_NOT_FOUND in the configure output (only useful for debugging the CMake build scripts)" OFF)
if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    feature_option_dependent(DBUS
        "Enables support for notifications and power-management features on Linux via D-Bus"
        ON "GUI" OFF
    )
    feature_option_dependent(SYSTEMD
        "Install systemd service file to a directory manually overridable with Systemd_SERVICES_INSTALL_DIR"
        OFF "NOT GUI" OFF
    )
    if (STACKTRACE)
        check_cxx_source_compiles(
            "#include <execinfo.h>
            int main(){return 0;}"
            QBITTORRENT_HAS_EXECINFO_H
        )
        if (NOT QBITTORRENT_HAS_EXECINFO_H)
            message(FATAL_ERROR "execinfo.h header file not found.\n"
                "Please either disable the STACKTRACE feature or use a libc that has this header file, such as glibc (GNU libc)."
            )
        endif()
    endif()
elseif (MSVC)
    feature_option(MSVC_RUNTIME_DYNAMIC "Use MSVC dynamic runtime library (-MD) instead of static (-MT)" ON)
endif()

include(GNUInstallDirs)
add_subdirectory(src)
add_subdirectory(dist)

if (VERBOSE_CONFIGURE)
    feature_summary(WHAT ALL)
else()
    feature_summary(WHAT ENABLED_FEATURES DISABLED_FEATURES)
endif()

# Generate version header
configure_file("src/base/version.h.in" "${CMAKE_CURRENT_SOURCE_DIR}/src/base/version.h" @ONLY)
