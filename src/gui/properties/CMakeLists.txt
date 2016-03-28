include_directories(
${CMAKE_CURRENT_BINARY_DIR}
${CMAKE_CURRENT_SOURCE_DIR}
../lineedit/src/
)

set(QBT_PROPERTIES_FORMS
propertieswidget.ui
trackersadditiondlg.ui
peersadditiondlg.ui
)

set(QBT_PROPERTIES_HEADERS
propertieswidget.h
peerlistwidget.h
proplistdelegate.h
trackerlist.h
downloadedpiecesbar.h
peerlistdelegate.h
peerlistsortmodel.h
peersadditiondlg.h
trackersadditiondlg.h
pieceavailabilitybar.h
proptabbar.h
speedwidget.h
speedplotview.h
)

set(QBT_PROPERTIES_SOURCES
propertieswidget.cpp
proplistdelegate.cpp
peerlistwidget.cpp
trackerlist.cpp
peersadditiondlg.cpp
downloadedpiecesbar.cpp
trackersadditiondlg.cpp
pieceavailabilitybar.cpp
proptabbar.cpp
speedwidget.cpp
speedplotview.cpp
)

add_library(qbt_properties STATIC ${QBT_PROPERTIES_HEADERS} ${QBT_PROPERTIES_SOURCES})
target_link_libraries(qbt_properties qbt_base)
if (NOT QT4_FOUND)
    target_link_libraries(qbt_properties Qt5::Widgets Qt5::Concurrent)
endif (NOT QT4_FOUND)
