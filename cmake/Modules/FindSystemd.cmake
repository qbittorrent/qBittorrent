#######
# Find systemd service dir
# sets variables
# SYSTEMD_FOUND
# SYSTEMD_SERVICES_INSTALL_DIR

find_package(PkgConfig QUIET REQUIRED)

if (NOT SYSTEMD_FOUND)
    pkg_check_modules(SYSTEMD "systemd")
endif()

if (SYSTEMD_FOUND AND "${SYSTEMD_SERVICES_INSTALL_DIR}" STREQUAL "")
    execute_process(COMMAND ${PKG_CONFIG_EXECUTABLE}
        --variable=systemdsystemunitdir systemd
        OUTPUT_VARIABLE SYSTEMD_SERVICES_INSTALL_DIR)
    string(REGEX REPLACE "[ \t\n]+" "" SYSTEMD_SERVICES_INSTALL_DIR
        "${SYSTEMD_SERVICES_INSTALL_DIR}")
elseif (NOT SYSTEMD_FOUND AND SYSTEMD_SERVICES_INSTALL_DIR)
    message (FATAL_ERROR "Variable SYSTEMD_SERVICES_INSTALL_DIR is\
        defined, but we can't find systemd using pkg-config")
endif()

if (SYSTEMD_FOUND)
    message(STATUS "systemd services install dir: ${SYSTEMD_SERVICES_INSTALL_DIR}")
endif()
