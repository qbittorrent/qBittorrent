add_library(qbt_base STATIC
    # headers
    3rdparty/expected.hpp
    algorithm.h
    asyncfilestorage.h
    bittorrent/abstractfilestorage.h
    bittorrent/addtorrentparams.h
    bittorrent/bandwidthscheduler.h
    bittorrent/bencoderesumedatastorage.h
    bittorrent/cachestatus.h
    bittorrent/categoryoptions.h
    bittorrent/common.h
    bittorrent/customstorage.h
    bittorrent/dbresumedatastorage.h
    bittorrent/downloadpriority.h
    bittorrent/filesearcher.h
    bittorrent/filterparserthread.h
    bittorrent/infohash.h
    bittorrent/loadtorrentparams.h
    bittorrent/ltqbitarray.h
    bittorrent/ltqhash.h
    bittorrent/lttypecast.h
    bittorrent/magneturi.h
    bittorrent/nativesessionextension.h
    bittorrent/nativetorrentextension.h
    bittorrent/peeraddress.h
    bittorrent/peerinfo.h
    bittorrent/portforwarderimpl.h
    bittorrent/resumedatastorage.h
    bittorrent/session.h
    bittorrent/sessionstatus.h
    bittorrent/speedmonitor.h
    bittorrent/statistics.h
    bittorrent/torrent.h
    bittorrent/torrentcontentlayout.h
    bittorrent/torrentcreatorthread.h
    bittorrent/torrentimpl.h
    bittorrent/torrentinfo.h
    bittorrent/tracker.h
    bittorrent/trackerentry.h
    digest32.h
    exceptions.h
    global.h
    http/connection.h
    http/httperror.h
    http/irequesthandler.h
    http/requestparser.h
    http/responsebuilder.h
    http/responsegenerator.h
    http/server.h
    http/types.h
    iconprovider.h
    indexrange.h
    logger.h
    net/dnsupdater.h
    net/downloadhandlerimpl.h
    net/downloadmanager.h
    net/geoipdatabase.h
    net/geoipmanager.h
    net/portforwarder.h
    net/proxyconfigurationmanager.h
    net/reverseresolution.h
    net/smtp.h
    orderedset.h
    path.h
    pathfwd.h
    preferences.h
    profile.h
    profile_p.h
    rss/rss_article.h
    rss/rss_autodownloader.h
    rss/rss_autodownloadrule.h
    rss/rss_feed.h
    rss/rss_folder.h
    rss/rss_item.h
    rss/rss_parser.h
    rss/rss_session.h
    search/searchdownloadhandler.h
    search/searchhandler.h
    search/searchpluginmanager.h
    settingsstorage.h
    tagset.h
    torrentfileguard.h
    torrentfileswatcher.h
    torrentfilter.h
    types.h
    unicodestrings.h
    utils/bytearray.h
    utils/compare.h
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
    version.h

    # sources
    asyncfilestorage.cpp
    bittorrent/abstractfilestorage.cpp
    bittorrent/bandwidthscheduler.cpp
    bittorrent/bencoderesumedatastorage.cpp
    bittorrent/categoryoptions.cpp
    bittorrent/customstorage.cpp
    bittorrent/dbresumedatastorage.cpp
    bittorrent/downloadpriority.cpp
    bittorrent/filesearcher.cpp
    bittorrent/filterparserthread.cpp
    bittorrent/infohash.cpp
    bittorrent/ltqbitarray.cpp
    bittorrent/magneturi.cpp
    bittorrent/nativesessionextension.cpp
    bittorrent/nativetorrentextension.cpp
    bittorrent/peeraddress.cpp
    bittorrent/peerinfo.cpp
    bittorrent/portforwarderimpl.cpp
    bittorrent/session.cpp
    bittorrent/speedmonitor.cpp
    bittorrent/statistics.cpp
    bittorrent/torrent.cpp
    bittorrent/torrentcontentlayout.cpp
    bittorrent/torrentcreatorthread.cpp
    bittorrent/torrentimpl.cpp
    bittorrent/torrentinfo.cpp
    bittorrent/tracker.cpp
    bittorrent/trackerentry.cpp
    exceptions.cpp
    http/connection.cpp
    http/httperror.cpp
    http/requestparser.cpp
    http/responsebuilder.cpp
    http/responsegenerator.cpp
    http/server.cpp
    iconprovider.cpp
    logger.cpp
    net/dnsupdater.cpp
    net/downloadhandlerimpl.cpp
    net/downloadmanager.cpp
    net/geoipdatabase.cpp
    net/geoipmanager.cpp
    net/portforwarder.cpp
    net/proxyconfigurationmanager.cpp
    net/reverseresolution.cpp
    net/smtp.cpp
    path.cpp
    preferences.cpp
    profile.cpp
    profile_p.cpp
    rss/rss_article.cpp
    rss/rss_autodownloader.cpp
    rss/rss_autodownloadrule.cpp
    rss/rss_feed.cpp
    rss/rss_folder.cpp
    rss/rss_item.cpp
    rss/rss_parser.cpp
    rss/rss_session.cpp
    search/searchdownloadhandler.cpp
    search/searchhandler.cpp
    search/searchpluginmanager.cpp
    settingsstorage.cpp
    tagset.cpp
    torrentfileguard.cpp
    torrentfileswatcher.cpp
    torrentfilter.cpp
    utils/bytearray.cpp
    utils/compare.cpp
    utils/foreignapps.cpp
    utils/fs.cpp
    utils/gzip.cpp
    utils/io.cpp
    utils/misc.cpp
    utils/net.cpp
    utils/password.cpp
    utils/random.cpp
    utils/string.cpp
)

target_link_libraries(qbt_base
    PRIVATE
        OpenSSL::Crypto OpenSSL::SSL
        ZLIB::ZLIB
    PUBLIC
        LibtorrentRasterbar::torrent-rasterbar
        Qt::Core Qt::Network Qt::Sql Qt::Xml
        qbt_common_cfg
)

if (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    find_library(IOKit_LIBRARY IOKit)
    find_library(Carbon_LIBRARY Carbon)
    find_library(AppKit_LIBRARY AppKit)

    target_link_libraries(qbt_base PRIVATE
        ${AppKit_LIBRARY}
        ${Carbon_LIBRARY}
        ${IOKit_LIBRARY}
    )
elseif (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_link_libraries(qbt_base PRIVATE Iphlpapi)
endif()

if (NOT GUI)
    target_compile_definitions(qbt_base PUBLIC DISABLE_GUI)
endif()

if (NOT WEBUI)
    target_compile_definitions(qbt_base PUBLIC DISABLE_WEBUI)
endif()

if (DBUS)
    target_link_libraries(qbt_base PUBLIC Qt::DBus)
endif()
