project(qtsingleapplication)

set(QBT_QTSINGLEAPPLICATION_HEADERS
qtlocalpeer.h
)

set(QBT_QTSINGLEAPPLICATION_SOURCES
qtlocalpeer.cpp
)

if (Qt5Widgets_FOUND)
    list(APPEND QBT_QTSINGLEAPPLICATION_HEADERS qtsingleapplication.h)
    list(APPEND QBT_QTSINGLEAPPLICATION_SOURCES qtsingleapplication.cpp)
else (Qt5Widgets_FOUND)
    list(APPEND QBT_QTSINGLEAPPLICATION_HEADERS qtsinglecoreapplication.h)
    list(APPEND QBT_QTSINGLEAPPLICATION_SOURCES qtsinglecoreapplication.cpp)
endif (Qt5Widgets_FOUND)

add_library(qtsingleapplication STATIC ${QBT_QTSINGLEAPPLICATION_HEADERS} ${QBT_QTSINGLEAPPLICATION_SOURCES})
target_include_directories(qtsingleapplication INTERFACE "${qtsingleapplication_SOURCE_DIR}")
target_link_libraries(qtsingleapplication PRIVATE Qt5::Network)

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" OR "${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
    target_compile_options(qtsingleapplication PRIVATE "-w") # disable warning for 3rdparty code
endif()

if (Qt5Widgets_FOUND)
    target_link_libraries(qtsingleapplication PRIVATE Qt5::Widgets)
endif (Qt5Widgets_FOUND)

add_library(QtSingleApplication::QtSingleApplication ALIAS qtsingleapplication)
