if (("${CMAKE_BUILD_TYPE}" STREQUAL "Debug") OR ("${CMAKE_BUILD_TYPE}" STREQUAL "RelWithDebInfo"))
    link_libraries(-Wl,--dynamicbase)
endif ()

list(APPEND LibtorrentRasterbar_CUSTOM_DEFINITIONS
    -D__USE_W32_SOCKETS
    -D_FILE_OFFSET_BITS=64)

link_libraries(advapi32 shell32 user32 wsock32 ws2_32 powrprof)
