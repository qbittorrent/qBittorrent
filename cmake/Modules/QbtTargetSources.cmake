# a helper function which appends source to the target
# sources file names are relative to the the target source dir

function (qbt_target_sources _target _scope)
    get_target_property(targetSourceDir ${_target} SOURCE_DIR)
    set(sourcesRelative "")
    foreach(source IN ITEMS ${ARGN})
        if(IS_ABSOLUTE "${source}")
            set(sourceAbsolutePath "${source}")
        else()
            get_filename_component(sourceAbsolutePath "${source}" ABSOLUTE)
        endif()
        file(RELATIVE_PATH sourceRelativePath "${targetSourceDir}" "${sourceAbsolutePath}")
        list(APPEND sourcesRelative "${sourceRelativePath}")
    endforeach()
    target_sources(${_target} ${_scope} "${sourcesRelative}")
endfunction(qbt_target_sources)
