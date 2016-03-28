set(QBT_QTNOTIFY_SOURCES
notifications.cpp
)

set(QBT_QTNOTIFY_HEADERS
notifications.h
)

add_library(qbt_qtnotify STATIC ${QBT_QTNOTIFY_SOURCES} ${QBT_QTNOTIFY_HEADERS})
set_target_properties(qbt_qtnotify PROPERTIES AUTOUIC False AUTORCC False)
target_link_qt_components(qbt_qtnotify DBus)
