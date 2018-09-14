# - macro similar to target_link_libraries, which links Qt components
# names of the components are passed in Qt4/Qt5 agnostic way (Core, DBus, Xml...)
# and the macro links Qt4 ones if QT4_FOUND is set or Qt5 ones if not

macro (target_link_qt_components target)
if (QT4_FOUND)
    foreach(_cmp ${ARGN})
        if ("${_cmp}" STREQUAL "PRIVATE" OR
            "${_cmp}" STREQUAL "PUBLIC" OR
            "${_cmp}" STREQUAL "INTERFACE")
             list(APPEND _QT_CMPNTS "${_cmp}")
        else()
            list(APPEND _QT_CMPNTS "Qt4::Qt${_cmp}")
        endif()
    endforeach()
else (QT4_FOUND)
    foreach(_cmp ${ARGN})
        if ("${_cmp}" STREQUAL "PRIVATE" OR
            "${_cmp}" STREQUAL "PUBLIC" OR
            "${_cmp}" STREQUAL "INTERFACE")
             list(APPEND _QT_CMPNTS "${_cmp}")
        else()
            list(APPEND _QT_CMPNTS "Qt5::${_cmp}")
        endif()
    endforeach()
endif (QT4_FOUND)
    target_link_libraries(${target} ${_QT_CMPNTS})
endmacro()
