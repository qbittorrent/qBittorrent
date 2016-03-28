set(QBT_QTSINGLEAPPLICATION_HEADERS
qtlocalpeer.h
)

set(QBT_QTSINGLEAPPLICATION_SOURCES
qtlocalpeer.cpp
)

if (GUI)
    list(APPEND QBT_QTSINGLEAPPLICATION_HEADERS qtsingleapplication.h)
    list(APPEND QBT_QTSINGLEAPPLICATION_SOURCES qtsingleapplication.cpp)
else (GUI)
    list(APPEND QBT_QTSINGLEAPPLICATION_HEADERS qtsinglecoreapplication.h)
    list(APPEND QBT_QTSINGLEAPPLICATION_SOURCES qtsinglecoreapplication.cpp)
endif (GUI)

add_library(qtsingleapplication ${QBT_QTSINGLEAPPLICATION_HEADERS} ${QBT_QTSINGLEAPPLICATION_SOURCES})

if (QT4_FOUND)
    target_link_libraries(qtsingleapplication Qt4::QtNetwork)
else (QT4_FOUND)
    target_link_libraries(qtsingleapplication Qt5::Network)
endif (QT4_FOUND)

if (GUI)
    if (QT4_FOUND)
        target_link_libraries(qtsingleapplication Qt4::QtGui)
    else (QT4_FOUND)
        target_link_libraries(qtsingleapplication Qt5::Widgets)
    endif(QT4_FOUND)
endif (GUI)

