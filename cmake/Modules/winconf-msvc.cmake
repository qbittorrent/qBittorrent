if (STACKTRACE)
    if (NOT "${WINXXBITS}" STREQUAL "Win64")
        # i686 arch requires frame pointer preservation
        add_compile_options(-Oy-)
    endif (NOT "${WINXXBITS}" STREQUAL "Win64")
    add_compile_options(-Zi)
    link_libraries(dbghelp.lib /DEBUG)
endif (STACKTRACE)

include(MacroConfigureMSVCRuntime)
set(MSVC_RUNTIME "dynamic")
configure_msvc_runtime()
