set(CMAKE_AUTORCC True)
set(CMAKE_AUTOUIC True)

include_directories(${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(lineedit)
add_subdirectory(properties)
add_subdirectory(powermanagement)
add_subdirectory(rss)
add_subdirectory(search)
if (UNIX AND NOT APPLE AND DBUS)
   add_subdirectory(qtnotify)
   include_directories(qtnotify)
   list(APPEND QBT_GUI_OPTIONAL_LINK_LIBRARIES qbt_qtnotify)
endif (UNIX AND NOT APPLE AND DBUS)

include_directories(
${CMAKE_CURRENT_BINARY_DIR}/properties
${CMAKE_CURRENT_BINARY_DIR}/rss
${CMAKE_CURRENT_BINARY_DIR}/search
lineedit/src
powermanagement
properties
rss
../app
)

set(QBT_GUI_HEADERS
about_imp.h
addnewtorrentdialog.h
advancedsettings.h
advancedsettings.h
autoexpandabledialog.h
deletionconfirmationdlg.h
downloadfromurldlg.h
executionlog.h
guiiconprovider.h
hidabletabwidget.h
ico.h
loglistwidget.h
mainwindow.h
messageboxraised.h
options_imp.h
previewlistdelegate.h
previewselect.h
scanfoldersdelegate.h
shutdownconfirm.h
speedlimitdlg.h
statsdialog.h
statusbar.h
torrentcontentfiltermodel.h
torrentcontentmodel.h
torrentcontentmodelfile.h
torrentcontentmodelfolder.h
torrentcontentmodelitem.h
torrentcontenttreeview.h
torrentcreatordlg.h
torrentimportdlg.h
torrentmodel.h
trackerlogin.h
transferlistdelegate.h
transferlistfilterswidget.h
transferlistsortmodel.h
transferlistwidget.h
updownratiodlg.h
)

set(QBT_GUI_SOURCES
addnewtorrentdialog.cpp
advancedsettings.cpp
autoexpandabledialog.cpp
executionlog.cpp
guiiconprovider.cpp
ico.cpp
loglistwidget.cpp
mainwindow.cpp
messageboxraised.cpp
options_imp.cpp
previewselect.cpp
scanfoldersdelegate.cpp
shutdownconfirm.cpp
speedlimitdlg.cpp
statsdialog.cpp
statusbar.cpp
torrentcontentfiltermodel.cpp
torrentcontentmodel.cpp
torrentcontentmodelfile.cpp
torrentcontentmodelfolder.cpp
torrentcontentmodelitem.cpp
torrentcontenttreeview.cpp
torrentcreatordlg.cpp
torrentimportdlg.cpp
torrentmodel.cpp
trackerlogin.cpp
transferlistdelegate.cpp
transferlistfilterswidget.cpp
transferlistsortmodel.cpp
transferlistwidget.cpp
updownratiodlg.cpp
)

if (WIN32 OR APPLE)
    list(APPEND QBT_GUI_HEADERS programupdater.h)
    list(APPEND QBT_GUI_SOURCES programupdater.cpp)
endif (WIN32 OR APPLE)

set(QBT_GUI_FORMS
mainwindow.ui
about.ui
preview.ui
login.ui
downloadfromurldlg.ui
bandwidth_limit.ui
updownratiodlg.ui
confirmdeletiondlg.ui
torrentimportdlg.ui
executionlog.ui
addnewtorrentdialog.ui
autoexpandabledialog.ui
statsdialog.ui
options.ui
torrentcreatordlg.ui
)

set(QBT_GUI_RESOURCES about.qrc)

add_library(qbt_gui STATIC ${QBT_GUI_HEADERS} ${QBT_GUI_SOURCES} ${QBT_GUI_RESOURCES})
target_link_libraries(qbt_gui qbt_lineedit qbt_powermanagement qbt_rss qbt_properties qbt_searchengine ${QBT_GUI_OPTIONAL_LINK_LIBRARIES} qbt_base)
