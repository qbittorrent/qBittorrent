# - macro similar to target_link_libraries, which links Qt components
# names of the components are pased in Qt4/Qt5 agnostic way (Core, DBus, Xml...)
# and the macro links Qt4 ones if QT4_FOUND is set or Qt5 ones if not

macro (target_link_qt_components target)
if (QT4_FOUND)
    foreach(_cmp ${ARGN})
        list(APPEND _QT_CMPNTS "Qt4::Qt${_cmp}")
    endforeach()
else (QT4_FOUND)
    foreach(_cmp ${ARGN})
        list(APPEND _QT_CMPNTS "Qt5::${_cmp}")
    endforeach()
endif (QT4_FOUND)
    target_link_libraries(${target} ${_QT_CMPNTS})
endmacro()
