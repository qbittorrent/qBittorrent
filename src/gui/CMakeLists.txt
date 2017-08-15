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
banlistoptions.h
categoryfiltermodel.h
categoryfilterproxymodel.h
categoryfilterwidget.h
cookiesdialog.h
cookiesmodel.h
deletionconfirmationdlg.h
downloadfromurldlg.h
executionlog.h
fspathedit.h
fspathedit_p.h
guiiconprovider.h
hidabletabwidget.h
loglistwidget.h
mainwindow.h
messageboxraised.h
optionsdlg.h
previewlistdelegate.h
previewselect.h
scanfoldersdelegate.h
shutdownconfirmdlg.h
speedlimitdlg.h
statsdialog.h
statusbar.h
tagfiltermodel.h
tagfilterproxymodel.h
tagfilterwidget.h
torrentcontentfiltermodel.h
torrentcontentmodel.h
torrentcontentmodelfile.h
torrentcontentmodelfolder.h
torrentcontentmodelitem.h
torrentcontenttreeview.h
torrentcreatordlg.h
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
banlistoptions.cpp
categoryfiltermodel.cpp
categoryfilterproxymodel.cpp
categoryfilterwidget.cpp
cookiesdialog.cpp
cookiesmodel.cpp
executionlog.cpp
fspathedit.cpp
fspathedit_p.cpp
guiiconprovider.cpp
loglistwidget.cpp
mainwindow.cpp
messageboxraised.cpp
optionsdlg.cpp
previewselect.cpp
scanfoldersdelegate.cpp
shutdownconfirmdlg.cpp
speedlimitdlg.cpp
statsdialog.cpp
statusbar.cpp
tagfiltermodel.cpp
tagfilterproxymodel.cpp
tagfilterwidget.cpp
torrentcontentfiltermodel.cpp
torrentcontentmodel.cpp
torrentcontentmodelfile.cpp
torrentcontentmodelfolder.cpp
torrentcontentmodelitem.cpp
torrentcontenttreeview.cpp
torrentcreatordlg.cpp
torrentmodel.cpp
trackerlogin.cpp
transferlistdelegate.cpp
transferlistfilterswidget.cpp
transferlistsortmodel.cpp
transferlistwidget.cpp
updownratiodlg.cpp
)

if (APPLE)
    list(APPEND QBT_GUI_HEADERS macutilities.h)
    list(APPEND QBT_GUI_SOURCES macutilities.mm)
endif (APPLE)

if (WIN32 OR APPLE)
    list(APPEND QBT_GUI_HEADERS programupdater.h)
    list(APPEND QBT_GUI_SOURCES programupdater.cpp)
endif (WIN32 OR APPLE)

set(QBT_GUI_FORMS
mainwindow.ui
about.ui
banlistoptions.ui
cookiesdialog.ui
preview.ui
login.ui
downloadfromurldlg.ui
bandwidth_limit.ui
updownratiodlg.ui
confirmdeletiondlg.ui
executionlog.ui
addnewtorrentdialog.ui
autoexpandabledialog.ui
statsdialog.ui
optionsdlg.ui
torrentcreatordlg.ui
shutdownconfirmdlg.ui
)

qbt_target_sources(about.qrc)

add_library(qbt_gui STATIC ${QBT_GUI_HEADERS} ${QBT_GUI_SOURCES} ${QBT_GUI_FORMS})
target_link_libraries(qbt_gui qbt_lineedit qbt_powermanagement qbt_rss qbt_properties qbt_searchengine
    ${QBT_GUI_OPTIONAL_LINK_LIBRARIES} qbt_base
    QtSingleApplication::QtSingleApplication
)
if(WIN32)
    target_link_libraries(qbt_gui Qt5::WinExtras)
endif(WIN32)

if (APPLE)
    target_link_libraries(qbt_gui Qt5::MacExtras)
endif()
