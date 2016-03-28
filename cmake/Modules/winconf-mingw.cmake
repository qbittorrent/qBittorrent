if (STACKTRACE_WIN)
    if ("${WINXXBITS}" NOT STREQUAL "Win64")
        add_compile_options(-fno-omit-frame-pointer)
    endif ("${WINXXBITS}" NOT STREQUAL "Win64")
    link_libraries(libdbghelp  -Wl,--export-all-symbols)
endif (STACKTRACE_WIN)

if (("${CMAKE_BUILD_TYPE}" STREQUAL "Debug") OR ("${CMAKE_BUILD_TYPE}" STREQUAL "RelWithDebInfo"))
    link_libraries(-Wl,--dynamicbase)
endif()

# LIBS += libadvapi32 libshell32 libuser32
# LIBS += libcrypto.dll libssl.dll libwsock32 libws2_32 libz libiconv.dll
# LIBS += libpowrprof
