# Native macOS user interface: an Objective-C++ bridge over the core,
# and a SwiftUI front-end that imports the bridge as a Clang module.

add_library(qbt_macos_bridge STATIC
    bridge/bridgeutils.h
    bridge/mainloopdispatcher.h
    bridge/qbtaddtorrent.h
    bridge/qbtadvancedsettings.h
    bridge/qbtbehaviorsettings.h
    bridge/qbtbittorrentsettings.h
    bridge/qbtbridge.h
    bridge/qbtconnectionsettings.h
    bridge/qbtdesktop.h
    bridge/qbtdetails.h
    bridge/qbtdownloadsettings.h
    bridge/qbtentry.h
    bridge/qbtrss.h
    bridge/qbtrssprivate.h
    bridge/qbtrssrules.h
    bridge/qbtsearch.h
    bridge/qbtsettings.h
    bridge/qbtspeedsettings.h
    bridge/qbttorrentoptions.h
    bridge/qbtwebuisettings.h
    bridge/settingsmacros.h
    bridge/mainloopdispatcher.mm
    bridge/qbtaddtorrent.mm
    bridge/qbtaddtorrentdefaults.mm
    bridge/qbtadvancedsettings.mm
    bridge/qbtbehaviorsettings.mm
    bridge/qbtbittorrentsettings.mm
    bridge/qbtbridge.mm
    bridge/qbtconnectionsettings.mm
    bridge/qbtdesktop.mm
    bridge/qbtdetails.mm
    bridge/qbtdownloadsettings.mm
    bridge/qbtrss.mm
    bridge/qbtrssrules.mm
    bridge/qbtsearch.mm
    bridge/qbtsettings.mm
    bridge/qbtspeedsettings.mm
    bridge/qbttorrentoptions.mm
    bridge/qbtwebuisettings.mm
)
target_link_libraries(qbt_macos_bridge
    PUBLIC Qt::CorePrivate
    PRIVATE qbt_base "-framework AppKit"
)
target_compile_options(qbt_macos_bridge PRIVATE -fobjc-arc)

# Kept apart from qbt_base on purpose: the C++ compile options that qbt_base
# exports would otherwise be handed to swiftc.
add_library(qbt_macos_ui STATIC
    ui/AddTorrentContent.swift
    ui/AddTorrentDialog.swift
    ui/AddTorrentFileList.swift
    ui/AddTorrentForm.swift
    ui/AddTorrentWindows.swift
    ui/AppDelegate.swift
    ui/AppRegistration.swift
    ui/DesktopIntegration.swift
    ui/details/DetailsSplit.swift
    ui/details/GeneralTab.swift
    ui/details/TorrentDetails.swift
    ui/DockTileView.swift
    ui/FilesTab.swift
    ui/FilterToolbarField.swift
    ui/Formatting.swift
    ui/GeneralPane.swift
    ui/InspectorLists.swift
    ui/MainView.swift
    ui/MenuBarItem.swift
    ui/Models.swift
    ui/Notifier.swift
    ui/rss/RSSArticleViews.swift
    ui/rss/RSSFeedList.swift
    ui/rss/RSSModel.swift
    ui/rss/RSSReaderView.swift
    ui/rss/RSSRuleForm.swift
    ui/rss/RSSRulesView.swift
    ui/rss/RSSSettingsPane.swift
    ui/search/PluginManagerView.swift
    ui/search/PythonMissingView.swift
    ui/search/SearchModel.swift
    ui/search/SearchResultsView.swift
    ui/search/SearchView.swift
    ui/SessionModel.swift
    ui/SettingsView.swift
    ui/sidebar/SidebarCustomizationSheet.swift
    ui/sidebar/SidebarSettings.swift
    ui/sidebar/SidebarView.swift
    ui/sidebar/TagsSidebarSection.swift
    ui/settings/AdvancedLibtorrentSections.swift
    ui/settings/AdvancedPane.swift
    ui/settings/BitTorrentPane.swift
    ui/settings/ConnectionFilterSections.swift
    ui/settings/ConnectionPane.swift
    ui/settings/DownloadsNotificationSections.swift
    ui/settings/DownloadsPane.swift
    ui/settings/LogFileSection.swift
    ui/settings/SpeedPane.swift
    ui/settings/WebUIPane.swift
    ui/settings/WebUISections.swift
    ui/Sheets.swift
    ui/style/Metrics.swift
    ui/TableColumnFitting.swift
    ui/TorrentActions.swift
    ui/TorrentLimitsSheet.swift
    ui/TorrentOptionsMenu.swift
    ui/TorrentSheets.swift
    ui/TorrentTableView.swift
    ui/TransfersPage.swift
    ui/TransfersToolbar.swift
)
set_target_properties(qbt_macos_ui PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
target_compile_options(qbt_macos_ui PRIVATE
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-I ${CMAKE_CURRENT_SOURCE_DIR}/bridge>"
    "$<$<COMPILE_LANGUAGE:Swift>:-parse-as-library>"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-swift-version 6>"
)
# swiftc lists the strings it finds, for qbt_macos_update_strings below.
set(QBT_MACOS_STRINGSDATA_DIR "${CMAKE_CURRENT_BINARY_DIR}/stringsdata")
target_compile_options(qbt_macos_ui PRIVATE
    "$<$<COMPILE_LANGUAGE:Swift>:-emit-localized-strings>"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-emit-localized-strings-path ${QBT_MACOS_STRINGSDATA_DIR}>"
)

# CMake does not hand the deployment target to swiftc by itself.
if (CMAKE_OSX_DEPLOYMENT_TARGET)
    target_compile_options(qbt_macos_ui PRIVATE
        "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-target ${CMAKE_SYSTEM_PROCESSOR}-apple-macosx${CMAKE_OSX_DEPLOYMENT_TARGET}>"
    )
endif()

# The Swift objects name the libraries they need (autolink), so linking with
# clang++ works once it can find the Swift runtime and compatibility libraries.
execute_process(COMMAND xcrun --show-sdk-path OUTPUT_VARIABLE QBT_MACOS_SDK OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND xcrun --find swiftc OUTPUT_VARIABLE QBT_SWIFTC OUTPUT_STRIP_TRAILING_WHITESPACE)
get_filename_component(QBT_SWIFT_TOOLCHAIN_DIR "${QBT_SWIFTC}" DIRECTORY)
target_link_options(qbt_macos_ui INTERFACE
    "-L${QBT_MACOS_SDK}/usr/lib/swift"
    "-L${QBT_SWIFT_TOOLCHAIN_DIR}/../lib/swift/macosx"
    "-Wl,-rpath,/usr/lib/swift"
)

# Translations: a string catalog, compiled into .lproj folders that qbt_app
# copies into its bundle.
set(QBT_MACOS_STRING_CATALOG "${CMAKE_CURRENT_SOURCE_DIR}/resources/Localizable.xcstrings")
set(QBT_MACOS_STRINGS_DIR "${CMAKE_CURRENT_BINARY_DIR}/strings")
add_custom_command(
    OUTPUT "${QBT_MACOS_STRINGS_DIR}/zh-Hans.lproj/Localizable.strings"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${QBT_MACOS_STRINGS_DIR}"
    COMMAND xcrun xcstringstool compile "${QBT_MACOS_STRING_CATALOG}" --output-directory "${QBT_MACOS_STRINGS_DIR}"
    DEPENDS "${QBT_MACOS_STRING_CATALOG}"
    VERBATIM
)
add_custom_target(qbt_macos_strings DEPENDS "${QBT_MACOS_STRINGS_DIR}/zh-Hans.lproj/Localizable.strings")

# Adds strings that are new in the Swift sources to the catalog. swiftc
# writes one .stringsdata file for each source file.
get_target_property(QBT_MACOS_UI_SOURCES qbt_macos_ui SOURCES)
set(QBT_MACOS_STRINGSDATA_FILES)
foreach(source IN LISTS QBT_MACOS_UI_SOURCES)
    get_filename_component(name "${source}" NAME_WE)
    list(APPEND QBT_MACOS_STRINGSDATA_FILES "${QBT_MACOS_STRINGSDATA_DIR}/${name}.stringsdata")
endforeach()
add_custom_target(qbt_macos_update_strings
    COMMAND xcrun xcstringstool sync "${QBT_MACOS_STRING_CATALOG}" --stringsdata ${QBT_MACOS_STRINGSDATA_FILES}
    DEPENDS qbt_macos_ui
    VERBATIM
)
