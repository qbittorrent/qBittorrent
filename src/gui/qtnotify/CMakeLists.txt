add_library(qbt_qtnotify STATIC
# headers
notifications.h

# sources
notifications.cpp
)

set_target_properties(qbt_qtnotify PROPERTIES AUTOUIC False AUTORCC False)
target_link_libraries(qbt_qtnotify PUBLIC Qt5::DBus)
target_include_directories(qbt_qtnotify PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
