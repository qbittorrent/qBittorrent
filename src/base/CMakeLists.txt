find_package(ZLIB REQUIRED)
include_directories(SYSTEM ${ZLIB_INCLUDE_DIRS})

set(QBT_BASE_HEADERS
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
net/reverseresolution.h
net/smtp.h
rss/private/rssparser.h
rss/rssarticle.h
rss/rssdownloadrule.h
rss/rssdownloadrulelist.h
rss/rssfeed.h
rss/rssfile.h
rss/rssfolder.h
rss/rssmanager.h
utils/fs.h
utils/gzip.h
utils/misc.h
utils/string.h
filesystemwatcher.h
iconprovider.h
logger.h
preferences.h
qinisettings.h
scanfoldersmodel.h
searchengine.h
torrentfilter.h
tristatebool.h
types.h
unicodestrings.h
)

set(QBT_BASE_SOURCES
bittorrent/cachestatus.cpp
bittorrent/infohash.cpp
bittorrent/magneturi.cpp
bittorrent/peerinfo.cpp
bittorrent/private/bandwidthscheduler.cpp
bittorrent/private/filterparserthread.cpp
bittorrent/private/resumedatasavingmanager.cpp
bittorrent/private/speedmonitor.cpp
bittorrent/private/statistics.cpp
bittorrent/session.cpp
bittorrent/sessionstatus.cpp
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
net/reverseresolution.cpp
net/smtp.cpp
rss/private/rssparser.cpp
rss/rssarticle.cpp
rss/rssdownloadrule.cpp
rss/rssdownloadrulelist.cpp
rss/rssfeed.cpp
rss/rssfile.cpp
rss/rssfolder.cpp
rss/rssmanager.cpp
utils/fs.cpp
utils/gzip.cpp
utils/misc.cpp
utils/string.cpp
filesystemwatcher.cpp
iconprovider.cpp
logger.cpp
preferences.cpp
scanfoldersmodel.cpp
searchengine.cpp
torrentfilter.cpp
tristatebool.cpp
)

add_library(qbt_base STATIC ${QBT_BASE_HEADERS} ${QBT_BASE_SOURCES})
target_link_libraries(qbt_base ${ZLIB_LIBRARIES} ${LibtorrentRasterbar_LIBRARIES})
target_link_qt_components(qbt_base Core Network Xml)
if (QT4_FOUND)
    if (GUI)
        target_link_libraries(qbt_base Qt4::QtGui)
    endif (GUI)
else (QT4_FOUND)
    if (GUI)
        target_link_libraries(qbt_base Qt5::Gui Qt5::Widgets)
    endif (GUI)
endif (QT4_FOUND)
