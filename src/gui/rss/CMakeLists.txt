include_directories(${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR})

set(QBT_RSS_HEADERS
articlelistwidget.h
automatedrssdownloader.h
feedlistwidget.h
htmlbrowser.h
rsswidget.h
)

set(QBT_RSS_SOURCE
articlelistwidget.cpp
automatedrssdownloader.cpp
feedlistwidget.cpp
htmlbrowser.cpp
rsswidget.cpp
)

set(QBT_RSS_FORMS
automatedrssdownloader.ui
rsswidget.ui
)

add_library(qbt_rss STATIC ${QBT_RSS_HEADERS} ${QBT_RSS_SOURCE} ${QBT_RSS_FORMS})
target_link_libraries(qbt_rss qbt_base)
target_link_libraries(qbt_rss Qt5::Gui Qt5::Widgets Qt5::Network)
