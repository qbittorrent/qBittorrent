# - Try to find the QtSingleApplication includes and library
# which defines
#
# QtSingleApplication_FOUND - system has QtSingleApplication
# QtSingleApplication_INCLUDE_DIR - where to find header QtSingleApplication
# QtSingleApplication_LIBRARIES - the libraries to link against to use QtSingleApplication
# QtSingleApplication_LIBRARY - where to find the QtSingleApplication library (not for general use)

# copyright (c) 2013 TI_Eugene ti.eugene@gmail.com
#
# Redistribution and use is allowed according to the terms of the FreeBSD license.

SET(QtSingleApplication_FOUND FALSE)

if (Qt5Widgets_FOUND)
    set(_includeFileName qtsingleapplication.h)
else()
    set(_includeFileName qtsinglecoreapplication.h)
endif()

FOREACH(TOP_INCLUDE_PATH in ${Qt5Core_INCLUDE_DIRS} ${FRAMEWORK_INCLUDE_DIR})
    FIND_PATH(QtSingleApplication_INCLUDE_DIR ${_includeFileName} ${TOP_INCLUDE_PATH}/QtSolutions)

    IF(QtSingleApplication_INCLUDE_DIR)
        BREAK()
    ENDIF()
ENDFOREACH()

SET(QtSingleApplication_NAMES ${QtSingleApplication_NAMES}
    Qt5Solutions_SingleApplication-2.6 libQt5Solutions_SingleApplication-2.6
    QtSolutions_SingleApplication-2.6 libQtSolutions_SingleApplication-2.6)
GET_TARGET_PROPERTY(_QT5_CORELIBRARY Qt5::Core LOCATION)
GET_FILENAME_COMPONENT(_QT5_CORELIBRARYPATH ${_QT5_CORELIBRARY} PATH)

FIND_LIBRARY(QtSingleApplication_LIBRARY
    NAMES ${QtSingleApplication_NAMES}
    PATHS ${_QT5_CORELIBRARYPATH}
)

IF (QtSingleApplication_LIBRARY AND QtSingleApplication_INCLUDE_DIR)

    SET(QtSingleApplication_LIBRARIES ${QtSingleApplication_LIBRARY})
    SET(QtSingleApplication_FOUND TRUE)

    IF (CYGWIN)
        IF(BUILD_SHARED_LIBS)
        # No need to define QtSingleApplication_USE_DLL here, because it's default for Cygwin.
        ELSE(BUILD_SHARED_LIBS)
        SET (QtSingleApplication_DEFINITIONS -DQTSINGLEAPPLICATION_STATIC)
        ENDIF(BUILD_SHARED_LIBS)
    ENDIF (CYGWIN)

ENDIF (QtSingleApplication_LIBRARY AND QtSingleApplication_INCLUDE_DIR)

IF (QtSingleApplication_FOUND)
    IF (NOT QtSingleApplication_FIND_QUIETLY)
        MESSAGE(STATUS "Found QtSingleApplication: ${QtSingleApplication_LIBRARY}")
        MESSAGE(STATUS "         includes: ${QtSingleApplication_INCLUDE_DIR}")
    ENDIF (NOT QtSingleApplication_FIND_QUIETLY)
    if(NOT TARGET QtSingleApplication::QtSingleApplication)
        add_library(QtSingleApplication::QtSingleApplication UNKNOWN IMPORTED)
        set_target_properties(QtSingleApplication::QtSingleApplication PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${QtSingleApplication_INCLUDE_DIR}"
            INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${QtSingleApplication_INCLUDE_DIR}"
        )
        if(EXISTS "${QtSingleApplication_LIBRARY}")
        set_target_properties(QtSingleApplication::QtSingleApplication PROPERTIES
            IMPORTED_LINK_INTERFACE_LANGUAGES "CXX"
            IMPORTED_LOCATION "${QtSingleApplication_LIBRARY}")
        endif()
    endif(NOT TARGET QtSingleApplication::QtSingleApplication)

ELSE (QtSingleApplication_FOUND)
  IF (QtSingleApplication_FIND_REQUIRED)
    MESSAGE(FATAL_ERROR "Could not find QtSingleApplication library")
  ENDIF (QtSingleApplication_FIND_REQUIRED)
ENDIF (QtSingleApplication_FOUND)

MARK_AS_ADVANCED(QtSingleApplication_INCLUDE_DIR QtSingleApplication_LIBRARY)
