list(APPEND QBT_QTNOTIFY_HEADERS
notifications.h
)

list(APPEND QBT_QTNOTIFY_SOURCES
notifications.cpp
)

add_library(qbt_qtnotify STATIC ${QBT_QTNOTIFY_HEADERS} ${QBT_QTNOTIFY_SOURCES})
target_link_qt_components(qbt_qtnotify DBus)
