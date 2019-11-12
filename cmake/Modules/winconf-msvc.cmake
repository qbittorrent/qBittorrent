include(MacroConfigureMSVCRuntime)
set(MSVC_RUNTIME "dynamic")
configure_msvc_runtime()

# libraries from winconf.pri
link_libraries(advapi32 crypt32 Iphlpapi ole32 shell32 User32)

