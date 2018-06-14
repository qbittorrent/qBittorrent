add_library(qbt_properties STATIC
# headers
downloadedpiecesbar.h
peerlistdelegate.h
peerlistsortmodel.h
peerlistwidget.h
peersadditiondialog.h
pieceavailabilitybar.h
piecesbar.h
propertieswidget.h
proplistdelegate.h
proptabbar.h
speedplotview.h
speedwidget.h
trackerlistwidget.h
trackersadditiondialog.h

# sources
downloadedpiecesbar.cpp
peerlistwidget.cpp
peersadditiondialog.cpp
pieceavailabilitybar.cpp
piecesbar.cpp
propertieswidget.cpp
proplistdelegate.cpp
proptabbar.cpp
speedplotview.cpp
speedwidget.cpp
trackerlistwidget.cpp
trackersadditiondialog.cpp

# forms
peersadditiondialog.ui
propertieswidget.ui
trackersadditiondialog.ui
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
