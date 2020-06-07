if (NOT Qt5Widgets_FOUND)
    feature_option(SYSTEMD "Install systemd service file (headless only)" OFF)
    if (SYSTEMD)
        if (NOT Systemd_SERVICES_INSTALL_DIR)
            find_package(Systemd)
            if (NOT Systemd_FOUND)
                message(FATAL_ERROR "Could not locate systemd services install dir."
                " Either pass -DSystemd_SERVICES_INSTALL_DIR=/path/to/systemd/services option or install systemd pkg-config")
            endif(NOT Systemd_FOUND)
        endif(NOT Systemd_SERVICES_INSTALL_DIR)
        set(EXPAND_BINDIR ${CMAKE_INSTALL_FULL_BINDIR})
        configure_file(systemd/qbittorrent-nox@.service.in ${CMAKE_CURRENT_BINARY_DIR}/qbittorrent-nox@.service @ONLY)
        install(FILES ${CMAKE_CURRENT_BINARY_DIR}/qbittorrent-nox@.service
            DESTINATION ${Systemd_SERVICES_INSTALL_DIR}
            COMPONENT data)
    endif(SYSTEMD)
endif()

if (Qt5Widgets_FOUND)
    list(APPEND MAN_FILES ${qBittorrent_SOURCE_DIR}/doc/qbittorrent.1)
else (Qt5Widgets_FOUND)
    list(APPEND MAN_FILES ${qBittorrent_SOURCE_DIR}/doc/qbittorrent-nox.1)
endif (Qt5Widgets_FOUND)

install(FILES ${MAN_FILES}
        DESTINATION ${CMAKE_INSTALL_MANDIR}/man1
        COMPONENT doc)

if (Qt5Widgets_FOUND)
    install(DIRECTORY menuicons/
            DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor
            FILES_MATCHING PATTERN "*.png")

    install(FILES org.qbittorrent.qBittorrent.desktop
            DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications/
            COMPONENT data)

    install(FILES org.qbittorrent.qBittorrent.appdata.xml
            DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/metainfo/
            COMPONENT data)

    install(FILES
            ${qBittorrent_SOURCE_DIR}/src/icons/qbittorrent-tray.svg
            ${qBittorrent_SOURCE_DIR}/src/icons/qbittorrent-tray-dark.svg
            ${qBittorrent_SOURCE_DIR}/src/icons/qbittorrent-tray-light.svg
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/scalable/status
        COMPONENT data)
endif()
