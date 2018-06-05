add_library(qbt_properties STATIC
propertieswidget.ui
trackersadditiondlg.ui
peersadditiondlg.ui
propertieswidget.h
peerlistwidget.h
proplistdelegate.h
trackerlist.h
downloadedpiecesbar.h
piecesbar.h
peerlistdelegate.h
peerlistsortmodel.h
peersadditiondlg.h
trackersadditiondlg.h
pieceavailabilitybar.h
proptabbar.h
speedwidget.h
speedplotview.h
propertieswidget.cpp
proplistdelegate.cpp
peerlistwidget.cpp
trackerlist.cpp
peersadditiondlg.cpp
downloadedpiecesbar.cpp
piecesbar.cpp
trackersadditiondlg.cpp
pieceavailabilitybar.cpp
proptabbar.cpp
speedwidget.cpp
speedplotview.cpp
)

target_link_libraries(qbt_properties
    PRIVATE
        qbt_gui_headers
    PUBLIC
        qbt_base qbt_lineedit Qt5::Widgets
)

target_include_directories(qbt_properties
    PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)
