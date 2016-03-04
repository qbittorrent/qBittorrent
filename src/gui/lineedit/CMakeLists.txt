set(QBT_LINEEDIT_SOURCES
src/lineedit.cpp
)

set(QBT_LINEEDIT_HEADERS
src/lineedit.h
)

set(QBT_LINEEDIT_RESOURCES
resources/lineeditimages.qrc
)

add_library(qbt_lineedit STATIC ${QBT_LINEEDIT_SOURCES} ${QBT_LINEEDIT_HEADERS} ${QBT_LINEEDIT_RESOURCES})
if (QT4_FOUND)
    target_link_libraries(qbt_lineedit Qt4::QtGui)
else (QT4_FOUND)
    target_link_libraries(qbt_lineedit Qt5::Widgets)
endif (QT4_FOUND)
