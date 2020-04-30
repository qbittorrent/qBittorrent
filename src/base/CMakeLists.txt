find_package(ZLIB 1.2.5.2 REQUIRED)

add_library(qbt_base STATIC
    # headers
    bittorrent/addtorrentparams.h
    bittorrent/cachestatus.h
    bittorrent/downloadpriority.h
    bittorrent/infohash.h
    bittorrent/magneturi.h
    bittorrent/peeraddress.h
    bittorrent/peerinfo.h
    bittorrent/private/bandwidthscheduler.h
    bittorrent/private/filterparserthread.h
    bittorrent/private/ltunderlyingtype.h
    bittorrent/private/nativesessionextension.h
    bittorrent/private/nativetorrentextension.h
    bittorrent/private/portforwarderimpl.h
    bittorrent/private/resumedatasavingmanager.h
    bittorrent/private/speedmonitor.h
    bittorrent/private/statistics.h
    bittorrent/session.h
    bittorrent/sessionstatus.h
    bittorrent/torrentcreatorthread.h
    bittorrent/torrenthandle.h
    bittorrent/torrenthandleimpl.h
    bittorrent/torrentinfo.h
    bittorrent/tracker.h
    bittorrent/trackerentry.h
    http/connection.h
    http/httperror.h
    http/irequesthandler.h
    http/requestparser.h
    http/responsebuilder.h
    http/responsegenerator.h
    http/server.h
    http/types.h
    net/dnsupdater.h
    net/downloadmanager.h
    net/geoipmanager.h
    net/portforwarder.h
    net/private/downloadhandlerimpl.h
    net/private/geoipdatabase.h
    net/proxyconfigurationmanager.h
    net/reverseresolution.h
    net/smtp.h
    private/profile_p.h
    rss/private/rss_parser.h
    rss/rss_article.h
    rss/rss_autodownloader.h
    rss/rss_autodownloadrule.h
    rss/rss_feed.h
    rss/rss_folder.h
    rss/rss_item.h
    rss/rss_session.h
    search/searchdownloadhandler.h
    search/searchhandler.h
    search/searchpluginmanager.h
    utils/bytearray.h
    utils/foreignapps.h
    utils/fs.h
    utils/gzip.h
    utils/io.h
    utils/misc.h
    utils/net.h
    utils/password.h
    utils/random.h
    utils/string.h
    utils/version.h
    algorithm.h
    asyncfilestorage.h
    exceptions.h
    filesystemwatcher.h
    global.h
    iconprovider.h
    indexrange.h
    logger.h
    preferences.h
    profile.h
    scanfoldersmodel.h
    settingsstorage.h
    torrentfileguard.h
    torrentfilter.h
    tristatebool.h
    types.h
    unicodestrings.h

    # sources
    bittorrent/downloadpriority.cpp
    bittorrent/infohash.cpp
    bittorrent/magneturi.cpp
    bittorrent/peeraddress.cpp
    bittorrent/peerinfo.cpp
    bittorrent/private/bandwidthscheduler.cpp
    bittorrent/private/filterparserthread.cpp
    bittorrent/private/nativesessionextension.cpp
    bittorrent/private/nativetorrentextension.cpp
    bittorrent/private/portforwarderimpl.cpp
    bittorrent/private/resumedatasavingmanager.cpp
    bittorrent/private/speedmonitor.cpp
    bittorrent/private/statistics.cpp
    bittorrent/session.cpp
    bittorrent/torrentcreatorthread.cpp
    bittorrent/torrenthandle.cpp
    bittorrent/torrenthandleimpl.cpp
    bittorrent/torrentinfo.cpp
    bittorrent/tracker.cpp
    bittorrent/trackerentry.cpp
    http/connection.cpp
    http/httperror.cpp
    http/requestparser.cpp
    http/responsebuilder.cpp
    http/responsegenerator.cpp
    http/server.cpp
    net/dnsupdater.cpp
    net/downloadmanager.cpp
    net/geoipmanager.cpp
    net/portforwarder.cpp
    net/private/downloadhandlerimpl.cpp
    net/private/geoipdatabase.cpp
    net/proxyconfigurationmanager.cpp
    net/reverseresolution.cpp
    net/smtp.cpp
    private/profile_p.cpp
    rss/private/rss_parser.cpp
    rss/rss_article.cpp
    rss/rss_autodownloader.cpp
    rss/rss_autodownloadrule.cpp
    rss/rss_feed.cpp
    rss/rss_folder.cpp
    rss/rss_item.cpp
    rss/rss_session.cpp
    search/searchdownloadhandler.cpp
    search/searchhandler.cpp
    search/searchpluginmanager.cpp
    utils/bytearray.cpp
    utils/foreignapps.cpp
    utils/fs.cpp
    utils/gzip.cpp
    utils/io.cpp
    utils/misc.cpp
    utils/net.cpp
    utils/password.cpp
    utils/random.cpp
    utils/string.cpp
    asyncfilestorage.cpp
    exceptions.cpp
    filesystemwatcher.cpp
    iconprovider.cpp
    logger.cpp
    preferences.cpp
    profile.cpp
    scanfoldersmodel.cpp
    settingsstorage.cpp
    torrentfileguard.cpp
    torrentfilter.cpp
    tristatebool.cpp
)

target_link_libraries(qbt_base
    PRIVATE
        ZLIB::ZLIB
    PUBLIC
        LibtorrentRasterbar::torrent-rasterbar
        Qt5::Core Qt5::Network Qt5::Xml
)

if (Qt5DBus_FOUND)
    target_link_libraries(qbt_base PRIVATE Qt5::DBus)
endif()

if (APPLE)
    find_library(IOKit_LIBRARY IOKit)
    find_library(Carbon_LIBRARY Carbon)
    find_library(AppKit_LIBRARY AppKit)
    target_link_libraries(qbt_base PRIVATE ${Carbon_LIBRARY} ${IOKit_LIBRARY} ${AppKit_LIBRARY})
endif()
