find_package(ZLIB 1.2.5.2 REQUIRED)

set(QBT_BASE_HEADERS
bittorrent/addtorrentparams.h
bittorrent/cachestatus.h
bittorrent/infohash.h
bittorrent/magneturi.h
bittorrent/peerinfo.h
bittorrent/private/bandwidthscheduler.h
bittorrent/private/filterparserthread.h
bittorrent/private/resumedatasavingmanager.h
bittorrent/private/speedmonitor.h
bittorrent/private/statistics.h
bittorrent/session.h
bittorrent/sessionstatus.h
bittorrent/torrentcreatorthread.h
bittorrent/torrenthandle.h
bittorrent/torrentinfo.h
bittorrent/tracker.h
bittorrent/trackerentry.h
http/connection.h
http/irequesthandler.h
http/requestparser.h
http/responsebuilder.h
http/responsegenerator.h
http/server.h
http/types.h
net/dnsupdater.h
net/downloadhandler.h
net/downloadmanager.h
net/geoipmanager.h
net/portforwarder.h
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
utils/fs.h
utils/gzip.h
utils/misc.h
utils/net.h
utils/random.h
utils/string.h
utils/version.h
asyncfilestorage.h
filesystemwatcher.h
global.h
iconprovider.h
indexrange.h
logger.h
preferences.h
profile.h
scanfoldersmodel.h
searchengine.h
settingsstorage.h
torrentfileguard.h
torrentfilter.h
tristatebool.h
types.h
unicodestrings.h
)

set(QBT_BASE_SOURCES
bittorrent/infohash.cpp
bittorrent/magneturi.cpp
bittorrent/peerinfo.cpp
bittorrent/private/bandwidthscheduler.cpp
bittorrent/private/filterparserthread.cpp
bittorrent/private/resumedatasavingmanager.cpp
bittorrent/private/speedmonitor.cpp
bittorrent/private/statistics.cpp
bittorrent/session.cpp
bittorrent/torrentcreatorthread.cpp
bittorrent/torrenthandle.cpp
bittorrent/torrentinfo.cpp
bittorrent/tracker.cpp
bittorrent/trackerentry.cpp
http/connection.cpp
http/requestparser.cpp
http/responsebuilder.cpp
http/responsegenerator.cpp
http/server.cpp
net/dnsupdater.cpp
net/downloadhandler.cpp
net/downloadmanager.cpp
net/geoipmanager.cpp
net/portforwarder.cpp
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
utils/fs.cpp
utils/gzip.cpp
utils/misc.cpp
utils/net.cpp
utils/random.cpp
utils/string.cpp
asyncfilestorage.cpp
filesystemwatcher.cpp
iconprovider.cpp
logger.cpp
preferences.cpp
profile.cpp
scanfoldersmodel.cpp
searchengine.cpp
settingsstorage.cpp
torrentfileguard.cpp
torrentfilter.cpp
tristatebool.cpp
)

add_library(qbt_base STATIC ${QBT_BASE_HEADERS} ${QBT_BASE_SOURCES})
target_link_libraries(qbt_base PRIVATE ZLIB::ZLIB PUBLIC LibtorrentRasterbar::LibTorrent)
target_link_qt_components(qbt_base PUBLIC Core Network Xml)

if (GUI)
    target_link_libraries(qbt_base PUBLIC Qt5::Gui Qt5::Widgets)
endif (GUI)

if (DBUS)
    target_link_qt_components(qbt_base PRIVATE DBus)
endif ()

if (APPLE)
    find_library(IOKit_LIBRARY IOKit)
    find_library(Carbon_LIBRARY Carbon)
    find_library(AppKit_LIBRARY AppKit)
    target_link_libraries(qbt_base PRIVATE ${Carbon_LIBRARY} ${IOKit_LIBRARY} ${AppKit_LIBRARY})
endif (APPLE)
