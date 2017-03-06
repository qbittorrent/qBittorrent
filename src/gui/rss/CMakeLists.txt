include_directories(${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR})

set(QBT_RSS_HEADERS
automatedrssdownloader.h
feedlistwidget.h
htmlbrowser.h
rss_imp.h
rsssettingsdlg.h
)

set(QBT_RSS_SOURCE
automatedrssdownloader.cpp
feedlistwidget.cpp
htmlbrowser.cpp
rss_imp.cpp
rsssettingsdlg.cpp
)

set(QBT_RSS_FORMS
automatedrssdownloader.ui
rss.ui
rsssettingsdlg.ui
)

add_library(qbt_rss STATIC ${QBT_RSS_HEADERS} ${QBT_RSS_SOURCE} ${QBT_RSS_FORMS})
target_link_libraries(qbt_rss qbt_base)
target_link_libraries(qbt_rss Qt5::Gui Qt5::Widgets Qt5::Network)
