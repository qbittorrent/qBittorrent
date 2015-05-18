INCLUDEPATH += $$PWD

HEADERS += $$PWD/geoipmanager.h

SOURCES += $$PWD/geoipmanager.cpp

# Add GeoIP resource file if the GeoIP database
# should be embedded in qBittorrent executable
contains(DEFINES, WITH_GEOIP_EMBEDDED) {
  exists("GeoIP.dat") {
    message("GeoIP.dat was found in src/gui/geoip/.")
    RESOURCES += $$PWD/geoip.qrc
  } else {
    DEFINES -= WITH_GEOIP_EMBEDDED
    error("GeoIP.dat was not found in src/gui/geoip/ folder, please follow instructions in src/gui/geoip/README.")
  }
} else {
  message("GeoIP database will not be embedded in qBittorrent executable.")
}
