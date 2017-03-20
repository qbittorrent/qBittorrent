set(BU_CHMOD_BUNDLE_ITEMS ON)
include(BundleUtilities)
fixup_bundle("$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/qbittorrent.app" "" "")
