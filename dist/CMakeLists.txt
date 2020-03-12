find_package(Qt5Widgets ${requiredQtVersion}) # to conditionally install desktop-related files

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
