# Generate and configure translation files
# -----------------------------------------------------------------------------
# -----------------------------------------------------------------------------
# Based on https://gist.github.com/giraldeau/546ba5512a74dfe9d8ea0862d66db412
file(GLOB QBT_TS_FILES "${qBittorrent_SOURCE_DIR}/src/lang/*.ts")
set_source_files_properties(${QBT_TS_FILES} PROPERTIES OUTPUT_LOCATION "${qBittorrent_BINARY_DIR}/src/lang")
qt5_add_translation(QBT_QM_FILES ${QBT_TS_FILES})
configure_file("${qBittorrent_SOURCE_DIR}/src/lang/lang.qrc" "${qBittorrent_BINARY_DIR}/src/lang/lang.qrc" COPYONLY)

if (WEBUI)
    file(GLOB QBT_WEBUI_TS_FILES "${qBittorrent_SOURCE_DIR}/src/webui/www/translations/*.ts")
    set_source_files_properties(${QBT_WEBUI_TS_FILES}
        PROPERTIES OUTPUT_LOCATION "${qBittorrent_BINARY_DIR}/src/webui/www/translations")
    qt5_add_translation(QBT_WEBUI_QM_FILES ${QBT_WEBUI_TS_FILES})
    configure_file("${qBittorrent_SOURCE_DIR}/src/webui/www/translations/webui_translations.qrc"
        "${qBittorrent_BINARY_DIR}/src/webui/www/translations/webui_translations.qrc" COPYONLY)
endif()

FILE(GLOB QT_TRANSLATIONS "${qBittorrent_SOURCE_DIR}/dist/qt-translations/qtbase_*.qm")
foreach(EXTRA_TRANSLATION IN ITEMS "fa" "gl" "lt" "pt" "sl" "sv" "zh_CN")
    list(APPEND QT_TRANSLATIONS "${qBittorrent_SOURCE_DIR}/dist/qt-translations/qt_${EXTRA_TRANSLATION}.qm")
endforeach()

# Executable target configuration
# -----------------------------------------------------------------------------
# -----------------------------------------------------------------------------
add_executable(qbt_app)

target_sources(qbt_app
    PRIVATE
        # headers
        application.h
        applicationinstancemanager.h
        cmdoptions.h
        filelogger.h
        qtlocalpeer/qtlocalpeer.h
        $<$<AND:$<NOT:$<PLATFORM_ID:Windows>>,$<BOOL:${STACKTRACE}>>:stacktrace.h>
        $<$<AND:$<PLATFORM_ID:Windows>,$<BOOL:${STACKTRACE}>>:stacktrace_win.h>
        $<$<AND:$<PLATFORM_ID:Windows>,$<BOOL:${STACKTRACE}>,$<BOOL:${GUI}>>:stacktracedialog.h>
        upgrade.h

        # sources
        application.cpp
        applicationinstancemanager.cpp
        cmdoptions.cpp
        filelogger.cpp
        main.cpp
        qtlocalpeer/qtlocalpeer.cpp
        $<$<AND:$<PLATFORM_ID:Windows>,$<BOOL:${STACKTRACE}>,$<BOOL:${GUI}>>:stacktracedialog.cpp>
        upgrade.cpp

        # forms
        $<$<AND:$<PLATFORM_ID:Windows>,$<BOOL:${STACKTRACE}>,$<BOOL:${GUI}>>:stacktracedialog.ui>

        # resources
        "${qBittorrent_SOURCE_DIR}/src/icons/icons.qrc"
        "${qBittorrent_SOURCE_DIR}/src/searchengine/searchengine.qrc"
        ${QBT_QM_FILES}
        $<$<BOOL:${WEBUI}>:${QBT_WEBUI_QM_FILES}>
        # yes, it's supposed to be "*_BINARY_DIR" in both of these entries
        "${qBittorrent_BINARY_DIR}/src/lang/lang.qrc"
        $<$<BOOL:${WEBUI}>:${qBittorrent_BINARY_DIR}/src/webui/www/translations/webui_translations.qrc>

        # misc Windows stuff
        $<$<PLATFORM_ID:Windows>:${qBittorrent_SOURCE_DIR}/src/qbittorrent.exe.manifest>
        $<$<AND:$<PLATFORM_ID:Windows>,$<NOT:$<BOOL:${MINGW}>>>:${qBittorrent_SOURCE_DIR}/src/qbittorrent.rc>
        $<$<AND:$<PLATFORM_ID:Windows>,$<BOOL:${MINGW}>>:${qBittorrent_SOURCE_DIR}/src/qbittorrent_mingw.rc>

        # misc macOS stuff
        $<$<PLATFORM_ID:Darwin>:
            ${QT_TRANSLATIONS}
            ${qBittorrent_SOURCE_DIR}/dist/mac/qt.conf
            ${qBittorrent_SOURCE_DIR}/dist/mac/qBitTorrentDocument.icns
            ${qBittorrent_SOURCE_DIR}/dist/mac/qbittorrent_mac.icns>
)

target_compile_definitions(qbt_app PRIVATE $<$<BOOL:${STACKTRACE}>:STACKTRACE>)

target_compile_options(qbt_app
    PRIVATE
        $<$<AND:$<PLATFORM_ID:Windows>,$<BOOL:${STACKTRACE}>,$<BOOL:${MSVC}>>:/Zi>
        # i686 arch on Windows requires frame pointer preservation
        $<$<AND:$<PLATFORM_ID:Windows>,$<BOOL:${STACKTRACE}>,$<EQUAL:${CMAKE_SIZEOF_VOID_P},4>,$<BOOL:${MSVC}>>:/Oy->
        $<$<AND:$<PLATFORM_ID:Windows>,$<BOOL:${STACKTRACE}>,$<EQUAL:${CMAKE_SIZEOF_VOID_P},4>,$<NOT:$<BOOL:${MSVC}>>>:-fno-omit-frame-pointer>
)

target_link_libraries(qbt_app
    PRIVATE
        qbt_base
        $<$<BOOL:${GUI}>:qbt_gui>
        $<$<BOOL:${WEBUI}>:qbt_webui>
        qbt_version_definitions
    PUBLIC
        $<$<AND:$<PLATFORM_ID:Windows>,$<BOOL:${STACKTRACE}>>:dbghelp>
)

target_link_options(qbt_app
    PUBLIC
        $<$<AND:$<PLATFORM_ID:Windows>,$<BOOL:${STACKTRACE}>,$<BOOL:${MSVC}>>:LINKER:/DEBUG>
)

set_target_properties(qbt_app
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY  "${CMAKE_BINARY_DIR}"
        OUTPUT_NAME               $<IF:$<BOOL:${GUI}>,qbittorrent,qbittorrent-nox>
)

# Additional macOS/Windows specific configuration
# -----------------------------------------------------------------------------
# -----------------------------------------------------------------------------
set_source_files_properties(${QT_TRANSLATIONS} PROPERTIES MACOSX_PACKAGE_LOCATION translations)
set_source_files_properties(
    "${qBittorrent_SOURCE_DIR}/dist/mac/qt.conf"
    "${qBittorrent_SOURCE_DIR}/dist/mac/qBitTorrentDocument.icns"
    "${qBittorrent_SOURCE_DIR}/dist/mac/qbittorrent_mac.icns"
        PROPERTIES
            MACOSX_PACKAGE_LOCATION Resources
)

if (CMAKE_SYSTEM_NAME MATCHES "Darwin")
    # substitute @EXECUTABLE@ in dist/mac/Info.plist
    get_target_property(EXECUTABLE qbt_app OUTPUT_NAME)
    configure_file(${qBittorrent_SOURCE_DIR}/dist/mac/Info.plist
        ${qBittorrent_BINARY_DIR}/dist/mac/pregen.plist @ONLY)
    file(GENERATE
        OUTPUT ${qBittorrent_BINARY_DIR}/dist/mac/Info.plist
        INPUT ${qBittorrent_BINARY_DIR}/dist/mac/pregen.plist
    )
    set_target_properties(qbt_app
        PROPERTIES
            MACOSX_BUNDLE             ON
            MACOSX_BUNDLE_BUNDLE_NAME $<GENEX_EVAL:$<TARGET_PROPERTY:OUTPUT_NAME>>
            MACOSX_BUNDLE_INFO_PLIST  ${qBittorrent_BINARY_DIR}/dist/mac/Info.plist
    )
elseif (CMAKE_SYSTEM_NAME MATCHES "Windows")
    set_target_properties(qbt_app PROPERTIES WIN32_EXECUTABLE ON)
endif()

if (GUI AND ((CMAKE_SYSTEM_NAME MATCHES "Windows") OR (CMAKE_SYSTEM_NAME MATCHES "Darwin")))
    qt5_import_plugins(qbt_app
        INCLUDE Qt5::QSvgIconPlugin
        INCLUDE Qt5::QSvgPlugin
    )
endif()

# Installation
# -----------------------------------------------------------------------------
# -----------------------------------------------------------------------------
install(TARGETS qbt_app
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    BUNDLE  DESTINATION .
    COMPONENT runtime
)

if (MSVC)
    install(FILES $<TARGET_PDB_FILE:qbt_app>
        DESTINATION ${CMAKE_INSTALL_BINDIR}
        OPTIONAL
    )
endif()
