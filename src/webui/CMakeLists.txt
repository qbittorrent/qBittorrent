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

if (QT4_FOUND)
    if (NOT SYSTEM_QJSON)
        add_subdirectory(qjson)
    else (NOT SYSTEM_QJSON)
        find_package(QJSON REQUIRED)
        add_definitions(-DUSE_SYSTEM_QJSON)
    endif(NOT SYSTEM_QJSON)
endif (QT4_FOUND)

set(QBT_WEBUI_RESOURCES webui.qrc)
add_library(qbt_webui STATIC ${QBT_WEBUI_HEADERS} ${QBT_WEBUI_SOURCES} ${QBT_WEBUI_RESOURCES})
target_link_libraries(qbt_webui qbt_base)

if (QT4_FOUND)
	target_link_libraries(qbt_webui qjson)
endif (QT4_FOUND)
