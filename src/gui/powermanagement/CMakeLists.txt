add_library(qbt_powermanagement STATIC
powermanagement.h
powermanagement.cpp
)

target_link_libraries(qbt_powermanagement PUBLIC Qt5::Core)
set_target_properties(qbt_powermanagement PROPERTIES AUTOUIC False AUTORCC False)
target_include_directories(qbt_powermanagement PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

if (UNIX AND Qt5DBus_FOUND)
    find_package(X11)
    if (X11_FOUND)
        target_sources(qbt_powermanagement PRIVATE powermanagement_x11.h powermanagement_x11.cpp)
        target_link_libraries(qbt_powermanagement PRIVATE Qt5::DBus)
    endif (X11_FOUND)
endif (UNIX AND Qt5DBus_FOUND)

if (WIN32)
    target_link_libraries(qbt_powermanagement PRIVATE PowrProf)
endif (WIN32)
