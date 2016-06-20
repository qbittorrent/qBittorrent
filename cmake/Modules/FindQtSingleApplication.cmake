# - Try to find the QtSingleApplication includes and library
# which defines
#
# QTSINGLEAPPLICATION_FOUND - system has QtSingleApplication
# QTSINGLEAPPLICATION_INCLUDE_DIR - where to find header QtSingleApplication
# QTSINGLEAPPLICATION_LIBRARIES - the libraries to link against to use QtSingleApplication
# QTSINGLEAPPLICATION_LIBRARY - where to find the QtSingleApplication library (not for general use)

# copyright (c) 2013 TI_Eugene ti.eugene@gmail.com
#
# Redistribution and use is allowed according to the terms of the FreeBSD license.

SET(QTSINGLEAPPLICATION_FOUND FALSE)

IF(QT4_FOUND)
    message(STATUS "Looking for Qt4 single application library")
    FIND_PATH(QTSINGLEAPPLICATION_INCLUDE_DIR QtSingleApplication
                # standard locations
                /usr/include
                /usr/include/QtSolutions
                # qt4 location except mac's frameworks
                "${QT_INCLUDE_DIR}/QtSolutions"
                # mac's frameworks
                ${FRAMEWORK_INCLUDE_DIR}/QtSolutions
    )

    SET(QTSINGLEAPPLICATION_NAMES ${QTSINGLEAPPLICATION_NAMES}
        QtSolutions_SingleApplication-2.6 libQtSolutions_SingleApplication-2.6)
    FIND_LIBRARY(QTSINGLEAPPLICATION_LIBRARY
        NAMES ${QTSINGLEAPPLICATION_NAMES}
        PATHS ${QT_LIBRARY_DIR}
    )
ELSEIF(Qt5Widgets_FOUND)
    message(STATUS "Looking for Qt5 single application library")
    FOREACH(TOP_INCLUDE_PATH in ${Qt5Widgets_INCLUDE_DIRS} ${FRAMEWORK_INCLUDE_DIR})
        FIND_PATH(QTSINGLEAPPLICATION_INCLUDE_DIR QtSingleApplication ${TOP_INCLUDE_PATH}/QtSolutions)

        IF(QTSINGLEAPPLICATION_INCLUDE_DIR)
            BREAK()
        ENDIF()
    ENDFOREACH()

    SET(QTSINGLEAPPLICATION_NAMES ${QTSINGLEAPPLICATION_NAMES}
        Qt5Solutions_SingleApplication-2.6 libQt5Solutions_SingleApplication-2.6
        QtSolutions_SingleApplication-2.6 libQtSolutions_SingleApplication-2.6)
    GET_TARGET_PROPERTY(QT5_WIDGETSLIBRARY Qt5::Widgets LOCATION)
    GET_FILENAME_COMPONENT(QT5_WIDGETSLIBRARYPATH ${QT5_WIDGETSLIBRARY} PATH)

    FIND_LIBRARY(QTSINGLEAPPLICATION_LIBRARY
        NAMES ${QTSINGLEAPPLICATION_NAMES}
        PATHS ${QT5_WIDGETSLIBRARYPATH}
    )
ENDIF()

IF (QTSINGLEAPPLICATION_LIBRARY AND QTSINGLEAPPLICATION_INCLUDE_DIR)

    SET(QTSINGLEAPPLICATION_LIBRARIES ${QTSINGLEAPPLICATION_LIBRARY})
    SET(QTSINGLEAPPLICATION_FOUND TRUE)

    IF (CYGWIN)
        IF(BUILD_SHARED_LIBS)
        # No need to define QTSINGLEAPPLICATION_USE_DLL here, because it's default for Cygwin.
        ELSE(BUILD_SHARED_LIBS)
        SET (QTSINGLEAPPLICATION_DEFINITIONS -DQTSINGLEAPPLICATION_STATIC)
        ENDIF(BUILD_SHARED_LIBS)
    ENDIF (CYGWIN)

ENDIF (QTSINGLEAPPLICATION_LIBRARY AND QTSINGLEAPPLICATION_INCLUDE_DIR)

IF (QTSINGLEAPPLICATION_FOUND)
  IF (NOT QtSingleApplication_FIND_QUIETLY)
    MESSAGE(STATUS "Found QtSingleApplication: ${QTSINGLEAPPLICATION_LIBRARY}")
    MESSAGE(STATUS "         includes: ${QTSINGLEAPPLICATION_INCLUDE_DIR}")
  ENDIF (NOT QtSingleApplication_FIND_QUIETLY)
ELSE (QTSINGLEAPPLICATION_FOUND)
  IF (QtSingleApplication_FIND_REQUIRED)
    MESSAGE(FATAL_ERROR "Could not find QtSingleApplication library")
  ENDIF (QtSingleApplication_FIND_REQUIRED)
ENDIF (QTSINGLEAPPLICATION_FOUND)

MARK_AS_ADVANCED(QTSINGLEAPPLICATION_INCLUDE_DIR QTSINGLEAPPLICATION_LIBRARY)

if(NOT TARGET QtSingleApplication::QtSingleApplication)
    add_library(QtSingleApplication::QtSingleApplication UNKNOWN IMPORTED)
    set_target_properties(QtSingleApplication::QtSingleApplication PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${QTSINGLEAPPLICATION_INCLUDE_DIR}"
        INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${QTSINGLEAPPLICATION_INCLUDE_DIR}"
    )
    if(EXISTS "${QTSINGLEAPPLICATION_LIBRARY}")
    set_target_properties(QtSingleApplication::QtSingleApplication PROPERTIES
        IMPORTED_LINK_INTERFACE_LANGUAGES "CXX"
        IMPORTED_LOCATION "${QTSINGLEAPPLICATION_LIBRARY}")
    endif()
endif(NOT TARGET QtSingleApplication::QtSingleApplication)
