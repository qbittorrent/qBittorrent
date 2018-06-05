add_library(qbt_searchengine STATIC
pluginselectdlg.ui
pluginsourcedlg.ui
searchwidget.ui
pluginselectdlg.h
pluginsourcedlg.h
searchlistdelegate.h
searchsortmodel.h
searchtab.h
searchwidget.h
pluginselectdlg.cpp
pluginsourcedlg.cpp
searchlistdelegate.cpp
searchsortmodel.cpp
searchtab.cpp
searchwidget.cpp
)

set(QBT_SEARCHENGINE_RESOURCES
# search.qrc
)

target_link_libraries(qbt_searchengine
    PUBLIC
        qbt_base
    PRIVATE
        qbt_lineedit qbt_gui_headers
)
