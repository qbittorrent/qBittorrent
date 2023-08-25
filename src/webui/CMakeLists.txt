add_library(qbt_webui STATIC
    # headers
    api/apicontroller.h
    api/apierror.h
    api/appcontroller.h
    api/authcontroller.h
    api/isessionmanager.h
    api/logcontroller.h
    api/rsscontroller.h
    api/searchcontroller.h
    api/synccontroller.h
    api/torrentcreatorcontroller.h
    api/torrentscontroller.h
    api/transfercontroller.h
    api/serialize/serialize_torrent.h
    freediskspacechecker.h
    webapplication.h
    webui.h

    # sources
    api/apicontroller.cpp
    api/apierror.cpp
    api/appcontroller.cpp
    api/authcontroller.cpp
    api/logcontroller.cpp
    api/rsscontroller.cpp
    api/searchcontroller.cpp
    api/synccontroller.cpp
    api/torrentcreatorcontroller.cpp
    api/torrentscontroller.cpp
    api/transfercontroller.cpp
    api/serialize/serialize_torrent.cpp
    freediskspacechecker.cpp
    webapplication.cpp
    webui.cpp
)

target_sources(qbt_webui INTERFACE www/webui.qrc)

target_link_libraries(qbt_webui PRIVATE qbt_base)
