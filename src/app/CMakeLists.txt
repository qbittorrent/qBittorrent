include_directories(${CMAKE_CURRENT_BINARY_DIR})

set(QBT_APP_HEADERS
application.h
)

set(QBT_APP_SOURCES
application.cpp
main.cpp
)

# translations
file(GLOB QBT_TS_FILES ../lang/*.ts)
get_filename_component(QBT_QM_FILES_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/../lang" ABSOLUTE)
set_source_files_properties(${QBT_TS_FILES} PROPERTIES OUTPUT_LOCATION "${QBT_QM_FILES_BINARY_DIR}")

if (QT5)
    find_package(Qt5 COMPONENTS LinguistTools REQUIRED)
    qt5_add_translation(QBT_QM_FILES ${QBT_TS_FILES})
else (QT5)
    qt4_add_translation(QBT_QM_FILES ${QBT_TS_FILES})
endif (QT5)

get_filename_component(_lang_qrc_src "${CMAKE_CURRENT_SOURCE_DIR}/../lang.qrc" ABSOLUTE)
get_filename_component(_lang_qrc_dst "${CMAKE_CURRENT_BINARY_DIR}/../lang.qrc" ABSOLUTE)
get_filename_component(_lang_qrc_dst_dir "${CMAKE_CURRENT_BINARY_DIR}/../" ABSOLUTE)

message(STATUS "copying ${_lang_qrc_src} -> ${_lang_qrc_dst}")
file(COPY ${_lang_qrc_src} DESTINATION ${_lang_qrc_dst_dir})

set_source_files_properties("${_lang_qrc_dst}" PROPERTIES GENERATED True)
foreach(qm_file ${QBT_QM_FILES})
    set_source_files_properties("${_lang_qrc_dst}" PROPERTIES OBJECT_DEPENDS ${qm_file})
endforeach()

set(QBT_APP_RESOURCES
../icons.qrc
"${_lang_qrc_dst}"
)

# With AUTORCC rcc is ran by cmake before language files are generated,
# and thus we call rcc explicitly
if (QT5)
    qt5_add_resources(QBT_APP_RESOURCE_SOURCE ${QBT_APP_RESOURCES})
else (QT5)
    qt4_add_resources(QBT_APP_RESOURCE_SOURCE ${QBT_APP_RESOURCES})
endif (QT5)

if (WIN32)
    if (MINGW)
        list (APPEND QBT_APP_SOURCES ../qbittorrent_mingw.rc)
    else (MINGW)
        list (APPEND QBT_APP_SOURCES ../qbittorrent.rc)
    endif (MINGW)
endif (WIN32)

if (UNIX)
    list(APPEND QBT_APP_HEADERS stacktrace.h)
endif (UNIX)

if (STACKTRACE_WIN)
    list(APPEND QBT_APP_HEADERS stacktrace_win.h)
    if (GUI)
        list(APPEND QBT_APP_HEADERS stacktrace_win_dlg.h)
    endif (GUI)
endif (STACKTRACE_WIN)

# usesystemqtsingleapplication {
#     nogui {
#         CONFIG += qtsinglecoreapplication
#     } else {
#         CONFIG += qtsingleapplication
#     }
# } else {
#     nogui {
#         include(qtsingleapplication/qtsinglecoreapplication.pri)
#     } else {
#         include(qtsingleapplication/qtsingleapplication.pri)
#     }
# }

# upgrade code
list(APPEND QBT_APP_HEADERS upgrade.h)
list(APPEND QBT_TARGET_LIBRARIES qbt_base)

if (GUI)
    set(QBT_TARGET_NAME qbittorrent)
    list(APPEND QBT_TARGET_LIBRARIES qbt_searchengine qbt_gui)
    include_directories(../gui
        ${CMAKE_CURRENT_BINARY_DIR}/../gui
    )
else (GUI)
    set(QBT_TARGET_NAME qbittorrent-nox)
endif (GUI)

if (WEBUI)
    list(APPEND QBT_TARGET_LIBRARIES qbt_webui)
endif (WEBUI)

add_executable(${QBT_TARGET_NAME} ${QBT_APP_HEADERS} ${QBT_APP_SOURCES} ${QBT_QM_FILES} ${QBT_APP_RESOURCE_SOURCE})
set_target_properties(${QBT_TARGET_NAME} PROPERTIES AUTOUIC True)

if (GUI)
    if (WIN32)
        set_target_properties(${QBT_TARGET_NAME} PROPERTIES WIN32_EXECUTABLE True)
    endif (WIN32)
    if (APPLE)
        set_target_properties(${QBT_TARGET_NAME} PROPERTIES MACOSX_BUNDLE True)
    endif (APPLE)
endif (GUI)

target_link_libraries(${QBT_TARGET_NAME} ${QBT_TARGET_LIBRARIES})

if (SYSTEM_QTSINGLEAPPLICATION)
    target_link_libraries(${QBT_TARGET_NAME} ${QTSINGLEAPPLICATION_LIBRARIES})
else (SYSTEM_QTSINGLEAPPLICATION)
    add_subdirectory(qtsingleapplication)
    target_link_libraries(${QBT_TARGET_NAME} qtsingleapplication)
endif (SYSTEM_QTSINGLEAPPLICATION)

# installation
install(TARGETS ${QBT_TARGET_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT runtime)
