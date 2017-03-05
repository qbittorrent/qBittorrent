set(QBT_WEBUI_HEADERS
abstractwebapplication.h
btjson.h
extra_translations.h
jsonutils.h
prefjson.h
webapplication.h
websessiondata.h
webui.h
)

set(QBT_WEBUI_SOURCES
abstractwebapplication.cpp
btjson.cpp
prefjson.cpp
webapplication.cpp
webui.cpp
)

qbt_target_sources(webui.qrc)

add_library(qbt_webui STATIC ${QBT_WEBUI_HEADERS} ${QBT_WEBUI_SOURCES})
target_link_libraries(qbt_webui qbt_base)

