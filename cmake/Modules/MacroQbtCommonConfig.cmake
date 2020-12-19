# Set common variables and create some interface-only library targets
# that some or all other targets will link to, either directly or transitively,
# to consume common compile options/definitions

macro(qbt_common_config)

    # treat value specified by the CXX_STANDARD target property as a requirement by default
    set(CMAKE_CXX_STANDARD_REQUIRED ON)

    # these definitions are only needed for calls to
    # lt::generate_fingerprint and for the qbittorrent.rc file on Windows
    add_library(qbt_version_definitions INTERFACE)

    target_compile_definitions(qbt_version_definitions INTERFACE
        QBT_VERSION_MAJOR=${qBittorrent_VERSION_MAJOR}
        QBT_VERSION_MINOR=${qBittorrent_VERSION_MINOR}
        QBT_VERSION_BUGFIX=${qBittorrent_VERSION_PATCH}
        QBT_VERSION_BUILD=${qBittorrent_VERSION_TWEAK}
    )

    add_library(qbt_common_cfg INTERFACE)

    # Full C++ 17 support is required
    # See also https://cmake.org/cmake/help/latest/prop_gbl/CMAKE_CXX_KNOWN_FEATURES.html
    # for a breakdown of the features that CMake recognizes for each C++ standard
    target_compile_features(qbt_common_cfg INTERFACE
        cxx_std_17
    )

    set(QBT_PROJECT_VERSION "${qBittorrent_VERSION_MAJOR}.${qBittorrent_VERSION_MINOR}.${qBittorrent_VERSION_PATCH}")
    if (NOT ${qBittorrent_VERSION_TWEAK} EQUAL 0)
        set(QBT_PROJECT_VERSION "${QBT_PROJECT_VERSION}.${qBittorrent_VERSION_TWEAK}")
    endif()

    set(QBT_FULL_VERSION "${QBT_PROJECT_VERSION}${QBT_VER_STATUS}")

    target_compile_definitions(qbt_common_cfg INTERFACE
        QBT_VERSION="v${QBT_FULL_VERSION}"
        QBT_VERSION_2="${QBT_FULL_VERSION}"
        QT_DEPRECATED_WARNINGS
        QT_NO_CAST_TO_ASCII
        QT_NO_CAST_FROM_BYTEARRAY
        QT_USE_QSTRINGBUILDER
        QT_STRICT_ITERATORS
        $<$<NOT:$<CONFIG:Debug>>:QT_NO_DEBUG_OUTPUT>
    )

    if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
        target_compile_definitions(qbt_common_cfg INTERFACE
            NTDDI_VERSION=0x06010000
            _WIN32_WINNT=0x0601
            _WIN32_IE=0x0601
            WIN32_LEAN_AND_MEAN
            NOMINMAX
            UNICODE
            _UNICODE
        )
    endif()

    if ((CXX_COMPILER_ID STREQUAL "GNU") OR (CXX_COMPILER_ID STREQUAL "Clang") OR (CXX_COMPILER_ID STREQUAL "AppleClang"))
        target_compile_options(qbt_common_cfg INTERFACE
            -Wall
            -Wextra
            -Wcast-qual
            -Wcast-align
            -Winvalid-pch
            -Woverloaded-virtual
            -Wold-style-cast
            -Wnon-virtual-dtor
            -pedantic
            -pedantic-errors
        )

        # Clang 11 still doesn't support -Wstrict-null-sentinel
        include(CheckCXXCompilerFlag)
        check_cxx_compiler_flag(-Wstrict-null-sentinel SNS_SUPPORT)
        if (SNS_SUPPORT)
            target_compile_options(qbt_common_cfg INTERFACE -Wstrict-null-sentinel)
        endif()
    endif()

    if (MINGW)
        target_link_options(qbt_common_cfg INTERFACE $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:LINKER:--dynamicbase>)
    endif()

    if (MSVC_RUNTIME_DYNAMIC)
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    else()
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()

    if (MSVC)
        target_compile_options(qbt_common_cfg INTERFACE /guard:cf)
        target_link_options(qbt_common_cfg INTERFACE /guard:cf
            $<$<NOT:$<CONFIG:Debug>>:/OPT:REF /OPT:ICF>
            # suppress linking warning due to /INCREMENTAL and /OPT:ICF being both ON
            $<$<CONFIG:RelWithDebInfo>:/INCREMENTAL:NO>
        )
    endif()

endmacro(qbt_common_config)
