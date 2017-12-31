set(QBT_LINEEDIT_SOURCES
src/lineedit.cpp
)

set(QBT_LINEEDIT_HEADERS
src/lineedit.h
)

add_library(qbt_lineedit STATIC ${QBT_LINEEDIT_SOURCES} ${QBT_LINEEDIT_HEADERS})
target_link_libraries(qbt_lineedit Qt5::Widgets)
