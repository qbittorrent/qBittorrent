if (QT6)
    find_package(Qt6 ${minQt6Version} REQUIRED COMPONENTS Widgets Svg)
else()
    find_package(Qt5 ${minQt5Version} REQUIRED COMPONENTS Widgets Svg)
    if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
        find_package(Qt5 ${minQt5Version} REQUIRED COMPONENTS WinExtras)
    endif()
endif()

# CMAKE_AUTO_UI causes unnecessary rebuilds
qt_wrap_ui(UI_HEADERS
    aboutdialog.ui
    addnewtorrentdialog.ui
    autoexpandabledialog.ui
    banlistoptionsdialog.ui
    cookiesdialog.ui
    deletionconfirmationdialog.ui
    downloadfromurldialog.ui
    executionlogwidget.ui
    ipsubnetwhitelistoptionsdialog.ui
    mainwindow.ui
    optionsdialog.ui
    previewselectdialog.ui
    properties/peersadditiondialog.ui
    properties/propertieswidget.ui
    properties/trackersadditiondialog.ui
    rss/automatedrssdownloader.ui
    rss/rsswidget.ui
    search/pluginselectdialog.ui
    search/pluginsourcedialog.ui
    search/searchjobwidget.ui
    search/searchwidget.ui
    shutdownconfirmdialog.ui
    speedlimitdialog.ui
    statsdialog.ui
    torrentcategorydialog.ui
    torrentcreatordialog.ui
    torrentoptionsdialog.ui
    trackerentriesdialog.ui
    watchedfolderoptionsdialog.ui
)

add_library(qbt_gui STATIC
    # headers
    aboutdialog.h
    addnewtorrentdialog.h
    advancedsettings.h
    autoexpandabledialog.h
    banlistoptionsdialog.h
    categoryfiltermodel.h
    categoryfilterproxymodel.h
    categoryfilterwidget.h
    cookiesdialog.h
    cookiesmodel.h
    deletionconfirmationdialog.h
    desktopintegration.h
    downloadfromurldialog.h
    executionlogwidget.h
    fspathedit.h
    fspathedit_p.h
    guiapplicationcomponent.h
    hidabletabwidget.h
    interfaces/iguiapplication.h
    ipsubnetwhitelistoptionsdialog.h
    lineedit.h
    log/logfiltermodel.h
    log/loglistview.h
    log/logmodel.h
    mainwindow.h
    optionsdialog.h
    powermanagement/powermanagement.h
    previewlistdelegate.h
    previewselectdialog.h
    progressbarpainter.h
    properties/downloadedpiecesbar.h
    properties/peerlistsortmodel.h
    properties/peerlistwidget.h
    properties/peersadditiondialog.h
    properties/pieceavailabilitybar.h
    properties/piecesbar.h
    properties/propertieswidget.h
    properties/proplistdelegate.h
    properties/proptabbar.h
    properties/speedplotview.h
    properties/speedwidget.h
    properties/trackerlistwidget.h
    properties/trackersadditiondialog.h
    raisedmessagebox.h
    rss/articlelistwidget.h
    rss/automatedrssdownloader.h
    rss/feedlistwidget.h
    rss/htmlbrowser.h
    rss/rsswidget.h
    search/pluginselectdialog.h
    search/pluginsourcedialog.h
    search/searchjobwidget.h
    search/searchsortmodel.h
    search/searchwidget.h
    shutdownconfirmdialog.h
    speedlimitdialog.h
    statsdialog.h
    statusbar.h
    tagfiltermodel.h
    tagfilterproxymodel.h
    tagfilterwidget.h
    torrentcategorydialog.h
    torrentcontentfiltermodel.h
    torrentcontentmodel.h
    torrentcontentmodelfile.h
    torrentcontentmodelfolder.h
    torrentcontentmodelitem.h
    torrentcontenttreeview.h
    torrentcreatordialog.h
    torrentoptionsdialog.h
    trackerentriesdialog.h
    transferlistdelegate.h
    transferlistfilterswidget.h
    transferlistmodel.h
    transferlistsortmodel.h
    transferlistwidget.h
    tristateaction.h
    tristatewidget.h
    uithememanager.h
    utils.h
    watchedfolderoptionsdialog.h
    watchedfoldersmodel.h

    # sources
    aboutdialog.cpp
    addnewtorrentdialog.cpp
    advancedsettings.cpp
    autoexpandabledialog.cpp
    banlistoptionsdialog.cpp
    categoryfiltermodel.cpp
    categoryfilterproxymodel.cpp
    categoryfilterwidget.cpp
    cookiesdialog.cpp
    cookiesmodel.cpp
    deletionconfirmationdialog.cpp
    desktopintegration.cpp
    downloadfromurldialog.cpp
    executionlogwidget.cpp
    fspathedit.cpp
    fspathedit_p.cpp
    guiapplicationcomponent.cpp
    hidabletabwidget.cpp
    ipsubnetwhitelistoptionsdialog.cpp
    lineedit.cpp
    log/logfiltermodel.cpp
    log/loglistview.cpp
    log/logmodel.cpp
    mainwindow.cpp
    optionsdialog.cpp
    powermanagement/powermanagement.cpp
    previewlistdelegate.cpp
    previewselectdialog.cpp
    progressbarpainter.cpp
    properties/downloadedpiecesbar.cpp
    properties/peerlistsortmodel.cpp
    properties/peerlistwidget.cpp
    properties/peersadditiondialog.cpp
    properties/pieceavailabilitybar.cpp
    properties/piecesbar.cpp
    properties/propertieswidget.cpp
    properties/proplistdelegate.cpp
    properties/proptabbar.cpp
    properties/speedplotview.cpp
    properties/speedwidget.cpp
    properties/trackerlistwidget.cpp
    properties/trackersadditiondialog.cpp
    raisedmessagebox.cpp
    rss/articlelistwidget.cpp
    rss/automatedrssdownloader.cpp
    rss/feedlistwidget.cpp
    rss/htmlbrowser.cpp
    rss/rsswidget.cpp
    search/pluginselectdialog.cpp
    search/pluginsourcedialog.cpp
    search/searchjobwidget.cpp
    search/searchsortmodel.cpp
    search/searchwidget.cpp
    shutdownconfirmdialog.cpp
    speedlimitdialog.cpp
    statsdialog.cpp
    statusbar.cpp
    tagfiltermodel.cpp
    tagfilterproxymodel.cpp
    tagfilterwidget.cpp
    torrentcategorydialog.cpp
    torrentcontentfiltermodel.cpp
    torrentcontentmodel.cpp
    torrentcontentmodelfile.cpp
    torrentcontentmodelfolder.cpp
    torrentcontentmodelitem.cpp
    torrentcontenttreeview.cpp
    torrentcreatordialog.cpp
    torrentoptionsdialog.cpp
    trackerentriesdialog.cpp
    transferlistdelegate.cpp
    transferlistfilterswidget.cpp
    transferlistmodel.cpp
    transferlistsortmodel.cpp
    transferlistwidget.cpp
    tristateaction.cpp
    tristatewidget.cpp
    uithememanager.cpp
    utils.cpp
    watchedfolderoptionsdialog.cpp
    watchedfoldersmodel.cpp

    # generated .ui headers
    ${UI_HEADERS}
)

target_sources(qbt_gui INTERFACE about.qrc)

# UI headers will be generated in ${CMAKE_CURRENT_BINARY_DIR}
target_include_directories(qbt_gui PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

target_link_libraries(qbt_gui
    PRIVATE
        qbt_base
    PUBLIC
        Qt::Gui Qt::Widgets
)

if (DBUS)
    target_sources(qbt_gui PRIVATE
        notifications/dbusnotifier.h
        notifications/dbusnotifier.cpp
        notifications/dbusnotificationsinterface.h
        notifications/dbusnotificationsinterface.cpp
        powermanagement/powermanagement_x11.h
        powermanagement/powermanagement_x11.cpp
    )
endif()

if (STACKTRACE)
    qt_wrap_ui(STACKTRACE_UI_HEADERS stacktracedialog.ui)

    target_sources(qbt_gui PRIVATE
        stacktracedialog.h
        stacktracedialog.cpp
        ${STACKTRACE_UI_HEADERS}
    )
endif()

if ((CMAKE_SYSTEM_NAME STREQUAL "Windows") OR (CMAKE_SYSTEM_NAME STREQUAL "Darwin"))
    target_sources(qbt_gui PRIVATE
        programupdater.h
        programupdater.cpp
    )
endif()

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_link_libraries(qbt_gui PRIVATE PowrProf)
    if (NOT QT6)
        target_link_libraries(qbt_gui PRIVATE Qt5::WinExtras)
    endif()
endif()

if (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    target_sources(qbt_gui PRIVATE
        macutilities.h
        macutilities.mm
    )
    target_link_libraries(qbt_gui PRIVATE objc)
endif()
