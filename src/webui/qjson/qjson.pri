INCLUDEPATH += $$PWD

HEADERS += $$PWD/FlexLexer.h \
           $$PWD/stack.hh \
           $$PWD/position.hh \
           $$PWD/location.hh \
           $$PWD/json_parser.hh \
           $$PWD/json_scanner.h \
           $$PWD/parser.h \
           $$PWD/parser_p.h \
           $$PWD_debug.h \
           $$PWD_export.h \
           $$PWD/serializer.h

SOURCES += $$PWD/json_parser.cc \
           $$PWD/json_scanner.cc \
           $$PWD/json_scanner.cpp \
           $$PWD/parser.cpp \
           $$PWD/serializer.cpp
