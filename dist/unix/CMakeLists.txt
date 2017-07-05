if (SYSTEMD)
    find_package(Systemd)
    if (SYSTEMD_FOUND)
        set(EXPAND_BINDIR ${CMAKE_INSTALL_FULL_BINDIR})
        configure_file(systemd/qbittorrent-nox@.service.in ${CMAKE_CURRENT_BINARY_DIR}/qbittorrent-nox@.service @ONLY)
        install(FILES ${CMAKE_CURRENT_BINARY_DIR}/qbittorrent-nox@.service
            DESTINATION ${SYSTEMD_SERVICES_INSTALL_DIR}
            COMPONENT data)
    endif(SYSTEMD_FOUND)
endif(SYSTEMD)


if (GUI)
    list(APPEND MAN_FILES ${qBittorrent_SOURCE_DIR}/doc/qbittorrent.1)
else (GUI)
    list(APPEND MAN_FILES ${qBittorrent_SOURCE_DIR}/doc/qbittorrent-nox.1)
endif (GUI)

install(FILES ${MAN_FILES}
        DESTINATION ${CMAKE_INSTALL_MANDIR}/man1
        COMPONENT doc)

if (GUI)
    install(DIRECTORY menuicons/
            DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor
            FILES_MATCHING PATTERN "*.png")

    install(FILES ${qBittorrent_SOURCE_DIR}/src/icons/qbittorrent.png
            DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/pixmaps/
            COMPONENT data)

    install(FILES ${qBittorrent_SOURCE_DIR}/src/icons/qbittorrent.desktop
            DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications/
            COMPONENT data)

    install(FILES qbittorrent.appdata.xml
            DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/appdata/
            COMPONENT data)

    install(FILES
            ${qBittorrent_SOURCE_DIR}/src/icons/skin/qbittorrent-tray-dark.svg
            ${qBittorrent_SOURCE_DIR}/src/icons/skin/qbittorrent-tray-light.svg
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/scalable/status
        COMPONENT data)
endif()
