# Set platform variables and create some interface-only library targets
# that some or all other targets will link to, either directly or transitively,
# to consume common compile options/definitions

# treat value specified by the CXX_STANDARD target property as a requirement by default
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTORCC_OPTIONS --compress-algo best --threshold 5)

add_library(qbt_common_cfg INTERFACE)

# C++ 20 support is required
# See also https://cmake.org/cmake/help/latest/prop_gbl/CMAKE_CXX_KNOWN_FEATURES.html
# for a breakdown of the features that CMake recognizes for each C++ standard
target_compile_features(qbt_common_cfg INTERFACE
    cxx_std_20
)

target_compile_definitions(qbt_common_cfg INTERFACE
    QT_DISABLE_DEPRECATED_UP_TO=0x060500
    QT_NO_CAST_FROM_ASCII
    QT_NO_CAST_TO_ASCII
    QT_NO_CAST_FROM_BYTEARRAY
    QT_NO_NARROWING_CONVERSIONS_IN_CONNECT
    QT_USE_QSTRINGBUILDER
    QT_STRICT_ITERATORS
    $<$<NOT:$<CONFIG:Debug>>:QT_NO_DEBUG_OUTPUT>
)

if (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    target_compile_definitions(qbt_common_cfg INTERFACE
        _DARWIN_FEATURE_64_BIT_INODE
    )
endif()

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_compile_definitions(qbt_common_cfg INTERFACE
        NTDDI_VERSION=0x0A000006
        _WIN32_WINNT=0x0A00
        _WIN32_IE=0x0A00
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        UNICODE
        _UNICODE
    )
endif()

if ((CMAKE_CXX_COMPILER_ID STREQUAL "GNU") OR (CMAKE_CXX_COMPILER_ID STREQUAL "Clang") OR (CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang"))
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

if ((CMAKE_CXX_COMPILER_ID STREQUAL "Clang") OR (CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang"))
    target_compile_options(qbt_common_cfg INTERFACE
        -Wno-range-loop-analysis
    )
endif()

if (MINGW)
    target_link_options(qbt_common_cfg INTERFACE $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:LINKER:--dynamicbase>)
endif()

if (MSVC)
    target_compile_options(qbt_common_cfg INTERFACE
        /guard:cf
        /permissive-
        /utf-8
        # https://devblogs.microsoft.com/cppblog/msvc-now-correctly-reports-__cplusplus/
        /Zc:__cplusplus
    )
    target_link_options(qbt_common_cfg INTERFACE
        /guard:cf
        $<$<NOT:$<CONFIG:Debug>>:/OPT:REF /OPT:ICF>
        # suppress linking warning due to /INCREMENTAL and /OPT:ICF being both ON
        $<$<CONFIG:RelWithDebInfo>:/INCREMENTAL:NO>
    )

    if (MSVC_RUNTIME_DYNAMIC)
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    else()
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()
endif()

if (DBUS)
    target_compile_definitions(qbt_common_cfg INTERFACE QBT_USES_DBUS)
endif()

if (LibtorrentRasterbar_VERSION VERSION_GREATER_EQUAL ${minLibtorrentVersion})
    target_compile_definitions(qbt_common_cfg INTERFACE QBT_USES_LIBTORRENT2)
endif()
