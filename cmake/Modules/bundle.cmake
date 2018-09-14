set(BU_CHMOD_BUNDLE_ITEMS ON)
include(DeployQt5)

set(plugins "")

get_property(svgIconPluginLocation TARGET Qt5::QSvgIconPlugin
        PROPERTY LOCATION_RELEASE)
list(APPEND plugins "${svgIconPluginLocation}")
get_property(svgPluginLocation TARGET Qt5::QSvgPlugin
        PROPERTY LOCATION_RELEASE)
list(APPEND plugins "${svgPluginLocation}")

set(sfx "")
if(APPLE)
    set(sfx ".app")
elseif(WIN32)
    set(sfx "${CMAKE_EXECUTABLE_SUFFIX}")
endif()

get_target_property(exe qBittorrent OUTPUT_NAME)
install_qt5_executable("${exe}${sfx}" "${plugins}" "" "" "")
