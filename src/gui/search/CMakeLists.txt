include_directories(
${CMAKE_CURRENT_BINARY_DIR}
${CMAKE_CURRENT_SOURCE_DIR}/..
${CMAKE_CURRENT_SOURCE_DIR}/../lineedit/src
)

set(QBT_SEARCHENGINE_FORMS
pluginselectdlg.ui
pluginsourcedlg.ui
searchwidget.ui
)

set(QBT_SEARCHENGINE_HEADERS
pluginselectdlg.h
pluginsourcedlg.h
searchlistdelegate.h
searchsortmodel.h
searchtab.h
searchwidget.h
)

set(QBT_SEARCHENGINE_SOURCES
pluginselectdlg.cpp
pluginsourcedlg.cpp
searchlistdelegate.cpp
searchsortmodel.cpp
searchtab.cpp
searchwidget.cpp
../ico.cpp
)

set(QBT_SEARCHENGINE_RESOURCES
# search.qrc
)

add_library(qbt_searchengine STATIC ${QBT_SEARCHENGINE_HEADERS} ${QBT_SEARCHENGINE_SOURCES} ${QBT_SEARCHENGINE_RESOURCES} ${QBT_SEARCHENGINE_FORMS})
target_link_libraries(qbt_searchengine qbt_base)

