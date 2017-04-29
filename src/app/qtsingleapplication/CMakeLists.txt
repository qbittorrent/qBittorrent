project(qtsingleapplication)

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
target_include_directories(qtsingleapplication INTERFACE "${qtsingleapplication_SOURCE_DIR}")
target_link_qt_components(qtsingleapplication Network)

if (GUI)
    target_link_qt_components(qtsingleapplication Widgets)
endif (GUI)

add_library(QtSingleApplication::QtSingleApplication ALIAS qtsingleapplication)
