# Set common variables and create some interface-only library targets
# that some or all other targets will link to, either directly or transitively,
# to consume common compile options/definitions

macro(qbt_common_config)

    # treat value specified by the CXX_STANDARD target property as a requirement by default
    set(CMAKE_CXX_STANDARD_REQUIRED ON)

    # these definitions are only needed for calls to
    # lt::generate_fingerprint and for the qbittorrent.rc file on Windows
    add_library(qbt_version_definitions INTERFACE)

    target_compile_definitions(qbt_version_definitions
        INTERFACE
            QBT_VERSION_MAJOR=${qBittorrent_VERSION_MAJOR}
            QBT_VERSION_MINOR=${qBittorrent_VERSION_MINOR}
            QBT_VERSION_BUGFIX=${qBittorrent_VERSION_PATCH}
            QBT_VERSION_BUILD=${qBittorrent_VERSION_TWEAK}
    )

    add_library(qbt_common_cfg INTERFACE)

    # Full C++ 14 support is required
    target_compile_features(qbt_common_cfg
        INTERFACE
            cxx_std_14
            cxx_contextual_conversions
            cxx_binary_literals
            cxx_decltype_auto
            cxx_lambda_init_captures
            cxx_generic_lambdas
            cxx_variable_templates
            cxx_relaxed_constexpr
            cxx_attribute_deprecated
            cxx_digit_separators
    )

    set(QBT_FULL_VERSION "${qBittorrent_VERSION}${QBT_VER_STATUS}")

    target_compile_definitions(qbt_common_cfg
        INTERFACE
            QBT_VERSION="v${QBT_FULL_VERSION}"
            QBT_VERSION_2="${QBT_FULL_VERSION}"
            QT_DEPRECATED_WARNINGS
            QT_NO_CAST_TO_ASCII
            QT_NO_CAST_FROM_BYTEARRAY
            QT_USE_QSTRINGBUILDER
            QT_STRICT_ITERATORS
            $<$<NOT:$<CONFIG:Debug>>:QT_NO_DEBUG_OUTPUT>
            $<$<AND:$<BOOL:${UNIX}>,$<NOT:$<BOOL:${APPLE}>>,$<NOT:$<BOOL:${CYGWIN}>>>:_DEFAULT_SOURCE>
            $<$<PLATFORM_ID:Windows>:
                NTDDI_VERSION=0x06010000
                _WIN32_WINNT=0x0601
                _WIN32_IE=0x0601
                UNICODE
                _UNICODE
                WIN32_LEAN_AND_MEAN
                _CRT_SECURE_NO_DEPRECATE
                _SCL_SECURE_NO_DEPRECATE
                NOMINMAX>
    )

    # Clang 11 still doesn't support -Wstrict-null-sentinel
    include(CheckCXXCompilerFlag)
    check_cxx_compiler_flag(-Wstrict-null-sentinel SNS_SUPPORT)
    target_compile_options(qbt_common_cfg
        INTERFACE
            $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:
                -Wall
                -Wextra
                -Wcast-qual
                -Wcast-align
                -Winvalid-pch
                -Woverloaded-virtual
                -Wold-style-cast
                -Wnon-virtual-dtor
                -pedantic
                -pedantic-errors>
            $<$<AND:$<CXX_COMPILER_ID:GNU,Clang,AppleClang>,$<BOOL:${SNS_SUPPORT}>>:-Wstrict-null-sentinel>
    )

    target_link_options(qbt_common_cfg
        INTERFACE
            $<$<AND:$<BOOL:${MINGW}>,$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>>:LINKER:--dynamicbase>
    )

    # set the default for the MSVC_RUNTIME_LIBRARY target property; ignored on non-MSVC compilers
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>$<$<BOOL:${MSVC_RUNTIME_DYNAMIC}>:DLL>")

    # There is no generator expression for CMAKE_GENERATOR yet
    if (CMAKE_GENERATOR MATCHES "Visual Studio")
        target_compile_options(qbt_common_cfg INTERFACE /MP)
    endif()

endmacro(qbt_common_config)
