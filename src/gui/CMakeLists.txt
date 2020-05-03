set(CMAKE_AUTORCC True)
set(CMAKE_AUTOUIC True)

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
    downloadfromurldialog.h
    executionlogwidget.h
    fspathedit.h
    hidabletabwidget.h
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
    private/fspathedit_p.h
    private/tristatewidget.h
    properties/downloadedpiecesbar.h
    properties/peerlistdelegate.h
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
    scanfoldersdelegate.h
    search/pluginselectdialog.h
    search/pluginsourcedialog.h
    search/searchjobwidget.h
    search/searchlistdelegate.h
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
    trackerentriesdialog.h
    transferlistdelegate.h
    transferlistfilterswidget.h
    transferlistmodel.h
    transferlistsortmodel.h
    transferlistwidget.h
    tristateaction.h
    uithememanager.h
    updownratiodialog.h
    utils.h

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
    downloadfromurldialog.cpp
    executionlogwidget.cpp
    fspathedit.cpp
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
    private/fspathedit_p.cpp
    private/tristatewidget.cpp
    properties/downloadedpiecesbar.cpp
    properties/peerlistdelegate.cpp
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
    scanfoldersdelegate.cpp
    search/pluginselectdialog.cpp
    search/pluginsourcedialog.cpp
    search/searchjobwidget.cpp
    search/searchlistdelegate.cpp
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
    trackerentriesdialog.cpp
    transferlistdelegate.cpp
    transferlistfilterswidget.cpp
    transferlistmodel.cpp
    transferlistsortmodel.cpp
    transferlistwidget.cpp
    tristateaction.cpp
    uithememanager.cpp
    updownratiodialog.cpp
    utils.cpp

    # forms
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
    search/searchwidget.ui
    shutdownconfirmdialog.ui
    speedlimitdialog.ui
    statsdialog.ui
    torrentcategorydialog.ui
    torrentcreatordialog.ui
    trackerentriesdialog.ui
    updownratiodialog.ui

    # resources
    about.qrc
)

if (WIN32 OR APPLE)
    target_sources(qbt_gui PRIVATE programupdater.h programupdater.cpp)
endif()

if (UNIX AND Qt5DBus_FOUND)
    target_link_libraries(qbt_gui PRIVATE Qt5::DBus)

    target_sources(qbt_gui PRIVATE
        qtnotify/notifications.h
        qtnotify/notifications.cpp
    )

    find_package(X11)
    if (X11_FOUND)
        target_sources(qbt_gui PRIVATE
            powermanagement/powermanagement_x11.h
            powermanagement/powermanagement_x11.cpp
        )
    endif()
endif()

target_include_directories(qbt_gui PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(qbt_gui
    PRIVATE
        qbt_base
    PUBLIC
        Qt5::Gui Qt5::Widgets
)

if (APPLE)
    target_sources(qbt_gui PRIVATE macutilities.h macutilities.mm)
    find_package(Qt5 ${requiredQtVersion} REQUIRED COMPONENTS MacExtras)
    target_link_libraries(qbt_gui PRIVATE Qt5::MacExtras objc)
endif()

if (WIN32)
    find_package(Qt5 ${requiredQtVersion} REQUIRED COMPONENTS WinExtras)
    target_link_libraries(qbt_gui PRIVATE Qt5::WinExtras PowrProf)
endif()
