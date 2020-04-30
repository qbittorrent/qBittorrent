target_sources(qbt_gui PRIVATE powermanagement.h powermanagement.cpp)

if (UNIX AND Qt5DBus_FOUND)
    find_package(X11)
    if (X11_FOUND)
        target_sources(qbt_gui PRIVATE powermanagement_x11.h powermanagement_x11.cpp)
    endif()
endif()
