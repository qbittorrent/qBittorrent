add_library(qbt_rss STATIC
articlelistwidget.h
automatedrssdownloader.h
feedlistwidget.h
htmlbrowser.h
rsswidget.h
articlelistwidget.cpp
automatedrssdownloader.cpp
feedlistwidget.cpp
htmlbrowser.cpp
rsswidget.cpp
automatedrssdownloader.ui
rsswidget.ui
)

target_include_directories(qbt_rss PRIVATE ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(qbt_rss
    PRIVATE
        qbt_gui_headers
    PUBLIC
        qbt_base Qt5::Gui Qt5::Widgets Qt5::Network
)
