set(ADDITIONAL_THEME_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/theme/Classic.colortheme
    ${CMAKE_CURRENT_SOURCE_DIR}/theme/AlternativeDark.colortheme
    ${CMAKE_CURRENT_SOURCE_DIR}/theme/AlternativeLight.colortheme
)

if (APPLE)
    add_subdirectory(mac)
else (APPLE)
    if (UNIX)
        add_subdirectory(unix)
    endif (UNIX)
    if (WIN32)
        add_subdirectory(windows)
    endif (WIN32)
endif (APPLE)
