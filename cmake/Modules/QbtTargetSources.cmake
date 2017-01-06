# a helper function which appends source to the main qBt target
# the target name is read from QBT_TARGET_NAME variable
# sources file names are relative to the the ${qbt_executable_SOURCE_DIR}

function (qbt_target_sources)
    set (_sources_rel "")
    foreach (_source IN ITEMS ${ARGN})
        if (IS_ABSOLUTE "${_source}")
            set(source_abs "${_source}")
        else()
            get_filename_component(_source_abs "${_source}" ABSOLUTE)
        endif()
        file (RELATIVE_PATH _source_rel "${qbt_executable_SOURCE_DIR}" "${_source_abs}")
        list (APPEND _sources_rel "${_source_rel}")
    endforeach()
    target_sources (${QBT_TARGET_NAME} PRIVATE "${_sources_rel}")
endfunction (qbt_target_sources)
