add_library(qbt_webui STATIC
    # headers
    api/apicontroller.h
    api/apierror.h
    api/apistatus.h
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
    webapplication.cpp
    webui.cpp
)

# Build TypeScript files before processing webui.qrc
find_program(NPM_EXECUTABLE npm)
if(NOT NPM_EXECUTABLE)
    message(FATAL_ERROR "npm is required to build the WebUI. Please install Node.js and npm.")
endif()

message(STATUS "Found npm: ${NPM_EXECUTABLE}")

# Find all TypeScript source files
file(GLOB_RECURSE WEBUI_TS_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/www/private/scripts/*.ts"
)
# Exclude .d.ts files from dependencies
list(FILTER WEBUI_TS_FILES EXCLUDE REGEX ".*\\.d\\.ts$")

# Generate list of output JS files
set(WEBUI_JS_FILES)
foreach(ts_file ${WEBUI_TS_FILES})
    string(REPLACE ".ts" ".js" js_file ${ts_file})
    list(APPEND WEBUI_JS_FILES ${js_file})
endforeach()

# Custom command to build TypeScript
add_custom_command(
    OUTPUT ${WEBUI_JS_FILES}
    COMMAND ${NPM_EXECUTABLE} run build
    DEPENDS ${WEBUI_TS_FILES}
            ${CMAKE_CURRENT_SOURCE_DIR}/www/tsconfig.json
            ${CMAKE_CURRENT_SOURCE_DIR}/www/package.json
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/www
    COMMENT "Compiling TypeScript files to JavaScript"
    VERBATIM
)

# Create a custom target for TypeScript compilation
add_custom_target(qbt_webui_typescript
    DEPENDS ${WEBUI_JS_FILES}
)

# Make sure TypeScript is built before the webui library
add_dependencies(qbt_webui qbt_webui_typescript)

target_sources(qbt_webui INTERFACE www/webui.qrc)

target_link_libraries(qbt_webui PRIVATE qbt_base)
