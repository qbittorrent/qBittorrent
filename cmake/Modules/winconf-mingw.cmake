if (("${CMAKE_BUILD_TYPE}" STREQUAL "Debug") OR ("${CMAKE_BUILD_TYPE}" STREQUAL "RelWithDebInfo"))
    link_libraries(-Wl,--dynamicbase)
endif ()

list(APPEND LibtorrentRasterbar_CUSTOM_DEFINITIONS
    -D_FILE_OFFSET_BITS=64
    -D__USE_W32_SOCKETS)

link_libraries(advapi32 iphlpapi ole32 powrprof shell32 user32 wsock32 ws2_32)
