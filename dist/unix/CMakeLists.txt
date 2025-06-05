if (SYSTEMD)
    if (NOT SYSTEMD_SERVICES_INSTALL_DIR)
        find_package(Systemd)
        if (NOT SYSTEMD_FOUND)
            message(
                FATAL_ERROR
                "Could not locate systemd services install dir."
                " Either pass the -DSYSTEMD_SERVICES_INSTALL_DIR=/path/to/systemd/services option"
                " or install systemd pkg-config"
            )
        endif()
    endif()
    set(EXPAND_BINDIR ${CMAKE_INSTALL_FULL_BINDIR})
    configure_file(systemd/qbittorrent-nox@.service.in ${CMAKE_CURRENT_BINARY_DIR}/qbittorrent-nox@.service @ONLY)
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/qbittorrent-nox@.service"
        DESTINATION ${SYSTEMD_SERVICES_INSTALL_DIR}
        COMPONENT data
    )
endif()

set(manPageLanguages
    en
    ru
)

foreach(manPageLanguage ${manPageLanguages})
    install(FILES ${PROJECT_SOURCE_DIR}/doc/${manPageLanguage}/$<IF:$<BOOL:${GUI}>,qbittorrent.1,qbittorrent-nox.1>
        # English man pages are installed into ${CMAKE_INSTALL_MANDIR}/man1, while man pages
        # in other languages are installed into ${CMAKE_INSTALL_MANDIR}/${manPageLanguage}/man1.
        DESTINATION ${CMAKE_INSTALL_MANDIR}/$<$<NOT:$<STREQUAL:${manPageLanguage},en>>:${manPageLanguage}/>man1
        COMPONENT doc
    )
endforeach()

if (GUI)
    install(FILES org.qbittorrent.qBittorrent.desktop
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications/
        COMPONENT data
    )

    install(FILES org.qbittorrent.qBittorrent.metainfo.xml
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/metainfo/
        COMPONENT data
    )

    install(DIRECTORY menuicons/
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor
        COMPONENT data
    )

    install(FILES
        ${PROJECT_SOURCE_DIR}/src/icons/qbittorrent-tray.svg
        ${PROJECT_SOURCE_DIR}/src/icons/qbittorrent-tray-dark.svg
        ${PROJECT_SOURCE_DIR}/src/icons/qbittorrent-tray-light.svg
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/scalable/status
        COMPONENT data
    )
endif()
