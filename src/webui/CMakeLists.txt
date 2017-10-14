set(QBT_WEBUI_HEADERS
api/apicontroller.h
api/apierror.h
api/appcontroller.h
api/isessionmanager.h
api/authcontroller.h
api/logcontroller.h
api/rsscontroller.h
api/synccontroller.h
api/torrentscontroller.h
api/transfercontroller.h
api/serialize/serialize_torrent.h
extra_translations.h
webapplication.h
webui.h
)

set(QBT_WEBUI_SOURCES
api/apicontroller.cpp
api/apierror.cpp
api/appcontroller.cpp
api/authcontroller.cpp
api/logcontroller.cpp
api/rsscontroller.cpp
api/synccontroller.cpp
api/torrentscontroller.cpp
api/transfercontroller.cpp
api/serialize/serialize_torrent.cpp
webapplication.cpp
webui.cpp
)

qbt_target_sources(webui.qrc)

add_library(qbt_webui STATIC ${QBT_WEBUI_HEADERS} ${QBT_WEBUI_SOURCES})
target_link_libraries(qbt_webui qbt_base)

