include_directories("${CMAKE_CURRENT_SOURCE_DIR}")
set(QJSON_HEADERS
FlexLexer.h
stack.hh
position.hh
location.hh
json_parser.hh
json_scanner.h
parser.h
parser_p.h
qjson_debug.h
qjson_export.h
serializer.h
)

set(QJSON_SOURCES
json_parser.cc
json_scanner.cc
json_scanner.cpp
parser.cpp
serializer.cpp
)

add_library(qjson STATIC ${QJSON_HEADERS} ${QJSON_SOURCES})

