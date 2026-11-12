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
