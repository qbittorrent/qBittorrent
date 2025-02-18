find_package(Qt6 ${minQt6Version} REQUIRED COMPONENTS Widgets Svg)

# CMAKE_AUTO_UI causes unnecessary rebuilds
qt_wrap_ui(UI_HEADERS
    aboutdialog.ui
    addnewtorrentdialog.ui
    addtorrentparamswidget.ui
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
    torrentsharelimitswidget.ui
    torrenttagsdialog.ui
    trackerentriesdialog.ui
    trackersadditiondialog.ui
    uithemedialog.ui
    watchedfolderoptionsdialog.ui
)

add_library(qbt_gui STATIC
    # headers
    aboutdialog.h
    addnewtorrentdialog.h
    addtorrentparamswidget.h
    advancedsettings.h
    autoexpandabledialog.h
    banlistoptionsdialog.h
    color.h
    colorscheme.h
    cookiesdialog.h
    cookiesmodel.h
    deletionconfirmationdialog.h
    desktopintegration.h
    downloadfromurldialog.h
    executionlogwidget.h
    filterpatternformat.h
    filterpatternformatmenu.h
    flowlayout.h
    fspathedit.h
    fspathedit_p.h
    guiaddtorrentmanager.h
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
    powermanagement/inhibitor.h
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
    properties/proptabbar.h
    properties/speedplotview.h
    properties/speedwidget.h
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
    torrentcategorydialog.h
    torrentcontentfiltermodel.h
    torrentcontentitemdelegate.h
    torrentcontentmodel.h
    torrentcontentmodelfile.h
    torrentcontentmodelfolder.h
    torrentcontentmodelitem.h
    torrentcontentwidget.h
    torrentcreatordialog.h
    torrentoptionsdialog.h
    torrentsharelimitswidget.h
    torrenttagsdialog.h
    trackerentriesdialog.h
    trackerlist/trackerlistitemdelegate.h
    trackerlist/trackerlistmodel.h
    trackerlist/trackerlistsortmodel.h
    trackerlist/trackerlistwidget.h
    trackersadditiondialog.h
    transferlistdelegate.h
    transferlistfilters/basefilterwidget.h
    transferlistfilters/categoryfiltermodel.h
    transferlistfilters/categoryfilterproxymodel.h
    transferlistfilters/categoryfilterwidget.h
    transferlistfilters/statusfilterwidget.h
    transferlistfilters/tagfiltermodel.h
    transferlistfilters/tagfilterproxymodel.h
    transferlistfilters/tagfilterwidget.h
    transferlistfilters/trackersfilterwidget.h
    transferlistfilterswidget.h
    transferlistmodel.h
    transferlistsortmodel.h
    transferlistwidget.h
    tristateaction.h
    tristatewidget.h
    uithemecommon.h
    uithemedialog.h
    uithememanager.h
    uithemesource.h
    utils.h
    watchedfolderoptionsdialog.h
    watchedfoldersmodel.h
    windowstate.h

    # sources
    aboutdialog.cpp
    addnewtorrentdialog.cpp
    addtorrentparamswidget.cpp
    advancedsettings.cpp
    autoexpandabledialog.cpp
    banlistoptionsdialog.cpp
    cookiesdialog.cpp
    cookiesmodel.cpp
    deletionconfirmationdialog.cpp
    desktopintegration.cpp
    downloadfromurldialog.cpp
    executionlogwidget.cpp
    filterpatternformatmenu.cpp
    flowlayout.cpp
    fspathedit.cpp
    fspathedit_p.cpp
    guiaddtorrentmanager.cpp
    hidabletabwidget.cpp
    ipsubnetwhitelistoptionsdialog.cpp
    lineedit.cpp
    log/logfiltermodel.cpp
    log/loglistview.cpp
    log/logmodel.cpp
    mainwindow.cpp
    optionsdialog.cpp
    powermanagement/inhibitor.cpp
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
    properties/proptabbar.cpp
    properties/speedplotview.cpp
    properties/speedwidget.cpp
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
    torrentcategorydialog.cpp
    torrentcontentfiltermodel.cpp
    torrentcontentitemdelegate.cpp
    torrentcontentmodel.cpp
    torrentcontentmodelfile.cpp
    torrentcontentmodelfolder.cpp
    torrentcontentmodelitem.cpp
    torrentcontentwidget.cpp
    torrentcreatordialog.cpp
    torrentoptionsdialog.cpp
    torrentsharelimitswidget.cpp
    torrenttagsdialog.cpp
    trackerentriesdialog.cpp
    trackerlist/trackerlistitemdelegate.cpp
    trackerlist/trackerlistmodel.cpp
    trackerlist/trackerlistsortmodel.cpp
    trackerlist/trackerlistwidget.cpp
    trackersadditiondialog.cpp
    transferlistdelegate.cpp
    transferlistfilters/basefilterwidget.cpp
    transferlistfilters/categoryfiltermodel.cpp
    transferlistfilters/categoryfilterproxymodel.cpp
    transferlistfilters/categoryfilterwidget.cpp
    transferlistfilters/statusfilterwidget.cpp
    transferlistfilters/tagfiltermodel.cpp
    transferlistfilters/tagfilterproxymodel.cpp
    transferlistfilters/tagfilterwidget.cpp
    transferlistfilters/trackersfilterwidget.cpp
    transferlistfilterswidget.cpp
    transferlistmodel.cpp
    transferlistsortmodel.cpp
    transferlistwidget.cpp
    tristateaction.cpp
    tristatewidget.cpp
    uithemedialog.cpp
    uithememanager.cpp
    uithemesource.cpp
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
        powermanagement/inhibitordbus.h
        powermanagement/inhibitordbus.cpp
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

if (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    target_sources(qbt_gui PRIVATE
        macosdockbadge/badger.h
        macosdockbadge/badger.mm
        macosdockbadge/badgeview.h
        macosdockbadge/badgeview.mm
        macosshiftclickhandler.h
        macosshiftclickhandler.cpp
        macutilities.h
        macutilities.mm
        powermanagement/inhibitormacos.h
        powermanagement/inhibitormacos.cpp
        programupdater.h
        programupdater.cpp
    )
    target_link_libraries(qbt_gui PRIVATE objc)
endif()

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_sources(qbt_gui PRIVATE
        powermanagement/inhibitorwindows.h
        powermanagement/inhibitorwindows.cpp
        programupdater.h
        programupdater.cpp
    )
endif()
