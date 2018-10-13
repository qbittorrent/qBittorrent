# macros to handle translation files

# qbt_add_translations(<target> QRC_FILE <filename> TS_FILES <filenames>)
# handles out of source builds for Qt resource files that include translations
# The function generates translations out of the supplied list of .ts files in the build directory,
# copies the .qrc file there, calls qt5_add_resources() adds its output to the target sources list.
function(qbt_add_translations _target)
    set(oneValueArgs QRC_FILE)
    set(multiValueArgs TS_FILES)
    cmake_parse_arguments(QBT_TR "" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    get_target_property(_binaryDir ${_target} BINARY_DIR)

    if (NOT QBT_TR_QRC_FILE)
        message(FATAL_ERROR "QRC file is empty")
    endif()
    if (NOT QBT_TR_TS_FILES)
        message(FATAL_ERROR "TS_FILES files are empty")
    endif()

    if(IS_ABSOLUTE "${QBT_TR_QRC_FILE}")
        file(RELATIVE_PATH _qrcToTs "${CMAKE_CURRENT_SOURCE_DIR}" "${QBT_TR_QRC_FILE}")
    else()
        set(_qrcToTs "${QBT_TR_QRC_FILE}")
    endif()

    get_filename_component(_qrcToTsDir "${_qrcToTs}" DIRECTORY)

    get_filename_component(_qmFilesBinaryDir "${CMAKE_CURRENT_BINARY_DIR}/${_qrcToTsDir}" ABSOLUTE)
    # to make qt5_add_translation() work as we need
    set_source_files_properties(${QBT_TR_TS_FILES} PROPERTIES OUTPUT_LOCATION "${_qmFilesBinaryDir}")
    qt5_add_translation(_qmFiles ${QBT_TR_TS_FILES})

    set(_qrc_dest_dir "${_binaryDir}/${_qrcToTsDir}")
    set(_qrc_dest_file "${_binaryDir}/${QBT_TR_QRC_FILE}")

    message(STATUS "copying ${QBT_TR_QRC_FILE} to ${_qrc_dest_dir}")
    file(COPY ${QBT_TR_QRC_FILE} DESTINATION ${_qrc_dest_dir})

    set_source_files_properties("${_qrc_dest_file}" PROPERTIES
        GENERATED True
        OBJECT_DEPENDS "${_qmFiles}")

    # With AUTORCC enabled rcc is ran by cmake before language files are generated,
    # and thus we call rcc explicitly
    qt5_add_resources(_resources "${_qrc_dest_file}")
    target_sources(${_target} PRIVATE "${_resources}")
endfunction()
