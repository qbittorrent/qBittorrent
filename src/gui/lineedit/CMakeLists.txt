add_library(qbt_lineedit STATIC
src/lineedit.cpp
src/lineedit.h
)

target_link_libraries(qbt_lineedit
    PRIVATE
        qbt_gui_headers
    PUBLIC
        Qt5::Widgets
)

target_include_directories(qbt_lineedit PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
