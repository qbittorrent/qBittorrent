CMAKE_MINIMUM_REQUIRED ( VERSION 2.8 )

# Find Qt
SET ( QT_USE_QTCORE TRUE )
SET ( QT_USE_QTDBUS TRUE )
SET ( QT_USE_QTGUI TRUE )
SET ( QT_USE_QTNETWORK TRUE )
SET ( QT_USE_QTXML TRUE )
FIND_PACKAGE ( Qt4 4.8 REQUIRED )
INCLUDE ( ${QT_USE_FILE} )

# Find ZLIB
FIND_PACKAGE ( ZLIB REQUIRED )

# Find Boost
FIND_PACKAGE ( Boost 1.51 REQUIRED COMPONENTS system )

# Find libtorrent (sorry, no find script yet)
SET ( libtorrent_LIBRARIES "-ltorrent-rasterbar -lssl -lcrypto" )

ADD_DEFINITIONS ( "-Wextra -Wunused -Wall" )
SET ( CMAKE_CXX_FLAGS "-DTORRENT_DEBUG -DTORRENT_USE_OPENSSL -DWITH_SHIPPED_GEOIP_H -DBOOST_ASIO_HASH_MAP_BUCKETS=1021 -DBOOST_EXCEPTION_DISABLE -DBOOST_ASIO_ENABLE_CANCELIO -DBOOST_ASIO_DYN_LINK")
SET ( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DVERSION=\\\"v3.2.0alpha\\\" -DVERSION_MAJOR=3 -DVERSION_MINOR=2 -DVERSION_BUGFIX=0")
SET ( CMAKE_CXX_FLAGS_DEBUG   "-O0 -g -DDEBUG -fno-inline" )
SET ( CMAKE_CXX_FLAGS_RELEASE "-O2 -s -DNDEBUG -Wl,--as-needed" )

#-I/usr/local/include -I/usr/local/include/libtorrent     -I/usr/include -g -fsanitize=address

SET ( SRC_GUI
        ./src/addnewtorrentdialog.cpp
        ./src/autoexpandabledialog.cpp
        ./src/dnsupdater.cpp
        ./src/downloadthread.cpp
        ./src/executionlog.cpp
        ./src/fs_utils.cpp
        ./src/geoip/geoipmanager.cpp
        ./src/ico.cpp
        ./src/iconprovider.cpp
        ./src/lineedit/src/lineedit.cpp
        ./src/loglistwidget.cpp
        ./src/main.cpp
        ./src/mainwindow.cpp
        ./src/messageboxraised.cpp
        ./src/misc.cpp
        ./src/powermanagement/powermanagement.cpp
        ./src/powermanagement/powermanagement_x11.cpp
        ./src/preferences/options_imp.cpp
        ./src/preferences/preferences.cpp
        ./src/previewselect.cpp
        ./src/programupdater.cpp
        ./src/properties/downloadedpiecesbar.cpp
        ./src/properties/peerlistwidget.cpp
        ./src/properties/pieceavailabilitybar.cpp
        ./src/properties/propertieswidget.cpp
        ./src/properties/proptabbar.cpp
        ./src/properties/trackerlist.cpp
        ./src/qmacapplication.cpp
        ./src/qtlibtorrent/alertdispatcher.cpp
        ./src/qtlibtorrent/qbtsession.cpp
        ./src/qtlibtorrent/qtorrenthandle.cpp
        ./src/qtlibtorrent/shutdownconfirm.cpp
        ./src/qtlibtorrent/torrentmodel.cpp
        ./src/qtlibtorrent/torrentspeedmonitor.cpp
        ./src/qtlibtorrent/torrentstatistics.cpp
        ./src/qtnotify/notifications.cpp
        ./src/qtsingleapp/qtlocalpeer.cpp
        ./src/qtsingleapp/qtsingleapplication.cpp
        ./src/qtsingleapp/qtsinglecoreapplication.cpp
        ./src/rss/automatedrssdownloader.cpp
        ./src/rss/cookiesdlg.cpp
        ./src/rss/feedlistwidget.cpp
        ./src/rss/rss_imp.cpp
        ./src/rss/rssarticle.cpp
        ./src/rss/rssdownloadrule.cpp
        ./src/rss/rssdownloadrulelist.cpp
        ./src/rss/rssfeed.cpp
        ./src/rss/rssfile.cpp
        ./src/rss/rssfolder.cpp
        ./src/rss/rssmanager.cpp
        ./src/rss/rssparser.cpp
        ./src/rss/rsssettingsdlg.cpp
        ./src/scannedfoldersmodel.cpp
        ./src/searchengine/engineselectdlg.cpp
        ./src/searchengine/searchengine.cpp
        ./src/searchengine/searchtab.cpp
        ./src/sessionapplication.cpp
        ./src/smtp.cpp
        ./src/statsdialog.cpp
        ./src/statussortfilterproxymodel.cpp
        ./src/torrentcontentfiltermodel.cpp
        ./src/torrentcontentmodel.cpp
        ./src/torrentcontentmodelfile.cpp
        ./src/torrentcontentmodelfolder.cpp
        ./src/torrentcontentmodelitem.cpp
        ./src/torrentcreator/torrentcreatordlg.cpp
        ./src/torrentcreator/torrentcreatorthread.cpp
        ./src/torrentimportdlg.cpp
        ./src/tracker/qtracker.cpp
        ./src/transferlistwidget.cpp
        ./src/updownratiodlg.cpp
        ./src/webui/btjson.cpp
        ./src/webui/httpconnection.cpp
        ./src/webui/httpheader.cpp
        ./src/webui/httprequestheader.cpp
        ./src/webui/httprequestparser.cpp
        ./src/webui/httpresponsegenerator.cpp
        ./src/webui/httpresponseheader.cpp
        ./src/webui/httpserver.cpp
        ./src/webui/prefjson.cpp
        ./src/webui/qjson/json_scanner.cpp
        ./src/webui/qjson/parser.cpp
        ./src/webui/qjson/serializer.cpp
        ./src/webui/qjson/json_parser.cc # .cc really?
#        ./src/qtsingleapp/qtlockedfile.cpp
#        ./src/qtsingleapp/qtlockedfile_unix.cpp
#        ./src/qtsingleapp/qtlockedfile_win.cpp
)

QT4_WRAP_CPP (
        SRC_GUI_MOC
        ./src/about_imp.h
        ./src/addnewtorrentdialog.h
        ./src/autoexpandabledialog.h
        ./src/deletionconfirmationdlg.h
        ./src/dnsupdater.h
        ./src/downloadfromurldlg.h
        ./src/geoip/geoipmanager.h
        ./src/headlessloader.h
        ./src/lineedit/src/lineedit.h
        ./src/loglistwidget.h
        ./src/mainwindow.h
        ./src/powermanagement/powermanagement.h
        ./src/powermanagement/powermanagement_x11.h
        ./src/preferences/advancedsettings.h
        ./src/preferences/options_imp.h
        ./src/preferences/preferences.h
        ./src/previewlistdelegate.h
        ./src/previewselect.h
        ./src/programupdater.h
        ./src/properties/downloadedpiecesbar.h
        ./src/properties/peeraddition.h
        ./src/properties/peerlistdelegate.h
        ./src/properties/peerlistsortmodel.h
        ./src/properties/peerlistwidget.h
        ./src/properties/pieceavailabilitybar.h
        ./src/properties/propertieswidget.h
        ./src/properties/proplistdelegate.h
        ./src/properties/proptabbar.h
        ./src/properties/trackerlist.h
        ./src/properties/trackersadditiondlg.h
        ./src/qinisettings.h
        ./src/qmacapplication.h
        ./src/qtlibtorrent/alertdispatcher.h
        ./src/qtlibtorrent/bandwidthscheduler.h
        ./src/qtlibtorrent/filterparserthread.h
        ./src/qtlibtorrent/qbtsession.h
        ./src/qtlibtorrent/shutdownconfirm.h
        ./src/qtlibtorrent/torrentmodel.h
        ./src/qtlibtorrent/torrentspeedmonitor.h
        ./src/qtlibtorrent/torrentstatistics.h
        ./src/qtnotify/notifications.h
        ./src/qtsingleapp/qtlocalpeer.h
        ./src/qtsingleapp/qtsingleapplication.h
        ./src/qtsingleapp/qtsinglecoreapplication.h
        ./src/reverseresolution.h
        ./src/rss/automatedrssdownloader.h
        ./src/rss/cookiesdlg.h
        ./src/rss/feedlistwidget.h
        ./src/rss/rss_imp.h
        ./src/rss/rssarticle.h
        ./src/rss/rssfeed.h
        ./src/rss/rssfolder.h
        ./src/rss/rssmanager.h
        ./src/rss/rssparser.h
        ./src/rss/rsssettingsdlg.h
        ./src/searchengine/engineselectdlg.h
        ./src/searchengine/pluginsource.h
        ./src/searchengine/searchengine.h
        ./src/searchengine/searchlistdelegate.h
        ./src/searchengine/searchsortmodel.h
        ./src/searchengine/searchtab.h
        ./src/searchengine/supportedengines.h
        ./src/searchengine/supportedengines.h
        ./src/sessionapplication.h
        ./src/smtp.h
        ./src/speedlimitdlg.h
        ./src/stacktrace_win_dlg.h
        ./src/statsdialog.h
        ./src/statusbar.h
        ./src/statussortfilterproxymodel.h
        ./src/torrentcontentmodel.h
        ./src/tracker/qtracker.h
        ./src/trackerlogin.h
        ./src/transferlistdelegate.h
        ./src/transferlistfilterswidget.h
        ./src/transferlistfilterswidget.h
        ./src/transferlistfilterswidget.h
        ./src/transferlistsortmodel.h
        ./src/transferlistwidget.h
        ./src/updownratiodlg.h
        ./src/downloadthread.h
        ./src/executionlog.h
        ./src/webui/httpconnection.h
        ./src/torrentimportdlg.h
        ./src/torrentcreator/torrentcreatordlg.h
        ./src/scannedfoldersmodel.h
        ./src/messageboxraised.h
        ./src/torrentcreator/torrentcreatorthread.h
        ./src/filesystemwatcher.h
        ./src/torrentcontentfiltermodel.h
        ./src/webui/httpserver.h
)

QT4_WRAP_UI (
        SRC_GUI_UI
        ./src/about.ui
        ./src/addnewtorrentdialog.ui
        ./src/autoexpandabledialog.ui
        ./src/bandwidth_limit.ui
        ./src/confirmdeletiondlg.ui
        ./src/downloadfromurldlg.ui
        ./src/executionlog.ui
        ./src/login.ui
        ./src/mainwindow.ui
        ./src/preferences/options.ui
        ./src/preview.ui
        ./src/properties/peer.ui
        ./src/properties/propertieswidget.ui
        ./src/properties/trackersadditiondlg.ui
        ./src/rss/automatedrssdownloader.ui
        ./src/rss/cookiesdlg.ui
        ./src/rss/rss.ui
        ./src/rss/rsssettingsdlg.ui
        ./src/searchengine/engineselect.ui
        ./src/searchengine/pluginsource.ui
        ./src/searchengine/search.ui
        ./src/stacktrace_win_dlg.ui
        ./src/statsdialog.ui
        ./src/torrentcreator/createtorrent.ui
        ./src/torrentimportdlg.ui
        ./src/updownratiodlg.ui
)

QT4_ADD_RESOURCES(
        SRC_GUI_QRC
        ./src/icons.qrc
        ./src/webui/webui.qrc
        ./src/lineedit/resources/lineeditimages.qrc
        ./src/lang.qrc
        ./src/about.qrc
        ./src/searchengine/search.qrc
#        ./src/geoip/geoip.qrc
)

INCLUDE_DIRECTORIES (
        "src"
        "src/geoip"
        "src/lineedit/src"
        "src/powermanagement"
        "src/preferences"
        "src/properties"
        "src/qtlibtorrent"
        "src/qtnotify"
        "src/qtsingleapp"
        "src/rss"
        "src/searchengine"
        "src/torrentcreator"
        "src/tracker"
        "src/webui"
        ${CMAKE_CURRENT_BINARY_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
)

ADD_EXECUTABLE (
        qbittorrent
        ${SRC_GUI}
        ${SRC_GUI_MOC}
        ${SRC_GUI_UI}
        ${SRC_GUI_QRC}
)

TARGET_LINK_LIBRARIES (
        qbittorrent
        ${QT_LIBRARIES}
        ${ZLIB_LIBRARIES}
        ${Boost_LIBRARIES}
        ${libtorrent_LIBRARIES}
        "-lpthread"
)
