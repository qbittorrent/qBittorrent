find_package(Qt5Widgets ${requiredQtVersion}) # to conditionally install desktop-related files

if (APPLE)
    add_subdirectory(mac)
elseif (UNIX)
    add_subdirectory(unix)
elseif (WIN32)
    add_subdirectory(windows)
endif()
