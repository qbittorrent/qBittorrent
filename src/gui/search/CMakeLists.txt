add_library(qbt_searchengine STATIC
# headers
pluginselectdialog.h
pluginsourcedialog.h
searchjobwidget.h
searchlistdelegate.h
searchsortmodel.h
searchwidget.h

# sources
pluginselectdialog.cpp
pluginsourcedialog.cpp
searchjobwidget.cpp
searchlistdelegate.cpp
searchsortmodel.cpp
searchwidget.cpp

# forms
pluginselectdialog.ui
pluginsourcedialog.ui
searchwidget.ui
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
