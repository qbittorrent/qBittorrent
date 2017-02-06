# function for parsing version variables that are set in version.pri file
# the version identifiers there are defined as follows:
# VER_MAJOR = 3
# VER_MINOR = 4
# VER_BUGFIX = 0
# VER_BUILD = 0
# VER_STATUS = alpha

function(read_version priFile outMajor outMinor outBugfix outBuild outStatus)
    file(STRINGS ${priFile} _priFileContents REGEX "^VER_.+")
    # message(STATUS "version.pri version contents: ${_priFileContents}")
    # the _priFileContents variable contains something like the following:
    # VER_MAJOR = 3;VER_MINOR = 4;VER_BUGFIX = 0;VER_BUILD = 0;VER_STATUS = alpha # Should be empty for stable releases!
    set(_regex "VER_MAJOR += +([0-9]+);VER_MINOR += +([0-9]+);VER_BUGFIX += +([0-9]+);VER_BUILD += +([0-9]+);VER_STATUS += +([0-9A-Za-z]+)?")
     # note quotes around _regex, they are needed because the variable contains semicolons
    string(REGEX MATCH "${_regex}" _tmp "${_priFileContents}")
    if (NOT _tmp)
        message(FATAL_ERROR "Could not detect project version number from ${priFile}")
    endif()

    # message(STATUS "Matched version string: ${_tmp}")

    set(${outMajor} ${CMAKE_MATCH_1} PARENT_SCOPE)
    set(${outMinor} ${CMAKE_MATCH_2} PARENT_SCOPE)
    set(${outBugfix} ${CMAKE_MATCH_3} PARENT_SCOPE)
    set(${outBuild} ${CMAKE_MATCH_4} PARENT_SCOPE)
    set(${outStatus} ${CMAKE_MATCH_5} PARENT_SCOPE)
endfunction()
