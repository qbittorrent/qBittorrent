set(QBT_POWERMANAGEMENT_HEADERS
powermanagement.h
)

set(QBT_POWERMANAGEMENT_SOURCES
powermanagement.cpp
)

if (UNIX AND DBUS)
    find_package(X11)
    if (X11_FOUND)
        list(APPEND QBT_POWERMANAGEMENT_HEADERS powermanagement_x11.h)
        list(APPEND QBT_POWERMANAGEMENT_SOURCES powermanagement_x11.cpp)
    endif (X11_FOUND)
endif (UNIX AND DBUS)

add_library(qbt_powermanagement STATIC ${QBT_POWERMANAGEMENT_SOURCES} ${QBT_POWERMANAGEMENT_HEADERS})
set_target_properties(qbt_powermanagement PROPERTIES AUTOUIC False AUTORCC False)
target_link_qt_components(qbt_powermanagement Core)
if (X11_FOUND)
    target_link_qt_components(qbt_powermanagement DBus)
endif (X11_FOUND)
if (WIN32)
    target_link_libraries(qbt_powermanagement PowrProf)
endif (WIN32)
