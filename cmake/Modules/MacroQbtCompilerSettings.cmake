# Sets cache variable QBT_ADDITONAL_FLAGS and QBT_ADDITONAL_CXX_FLAGS to list of additional
# compiler flags for C and C++ (QBT_ADDITONAL_FLAGS) and for C++ only (QBT_ADDITONAL_CXX_FLAGS)
# and appends them to CMAKE_XXX_FLAGS variables.

# It could use add_compile_options(), but then it is needed to use generator expressions,
# and most interesting of them are not compatible with Visual Studio :(

macro(qbt_set_compiler_options)
# if (NOT QBT_ADDITONAL_FLAGS)
    if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
        #-Wshadow -Wconversion ?
        set(_GCC_COMMON_C_AND_CXX_FLAGS "-Wall -Wextra"
            "-Wfloat-equal -Wcast-qual -Wcast-align"
            "-Wsign-conversion -Winvalid-pch -Wno-long-long"
            #"-fstack-protector-all"
            #"-Werror -Wno-error=deprecated-declarations"
        )
        set(_GCC_COMMON_CXX_FLAGS "-fexceptions -frtti"
            "-Woverloaded-virtual -Wold-style-cast"
            "-Wnon-virtual-dtor -Wfloat-equal -Wcast-qual -Wcast-align"
            #"-Weffc++"
            #"-Werror -Wno-error=cpp"
            # we should modify code to make these ones obsolete
            #"-Wno-error=sign-conversion -Wno-error=float-equal"
        )

        if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 4.9)
            # GCC 4.8 has problems with std::array and its initialization
            list(APPEND _GCC_COMMON_CXX_FLAGS "-Wno-error=missing-field-initializers")
        endif()

        include(CheckCXXCompilerFlag)
        # check for -pedantic
        check_cxx_compiler_flag(-pedantic _PEDANTIC_IS_SUPPORTED)
        if (_PEDANTIC_IS_SUPPORTED)
            list(APPEND _GCC_COMMON_CXX_FLAGS "-pedantic -pedantic-errors")
        else (_PEDANTIC_IS_SUPPORTED)
            list(APPEND _GCC_COMMON_CXX_FLAGS "-Wpedantic")
        endif (_PEDANTIC_IS_SUPPORTED)

        if (CMAKE_SYSTEM_NAME MATCHES Linux)
            # if Glibc version is 2.20 or higher, set -D_DEFAULT_SOURCE
            include(MacroGlibcDetect)
            message(STATUS "Detecting Glibc version...")
            glibc_detect(GLIBC_VERSION)
            if(${GLIBC_VERSION})
                if(GLIBC_VERSION LESS "220")
                    message(STATUS "Glibc version is ${GLIBC_VERSION}")
                else(GLIBC_VERSION LESS "220")
                    message(STATUS "Glibc version is ${GLIBC_VERSION}, adding -D_DEFAULT_SOURCE")
                    add_definitions(-D_DEFAULT_SOURCE)
                endif(GLIBC_VERSION LESS "220")
            endif(${GLIBC_VERSION})
        endif (CMAKE_SYSTEM_NAME MATCHES Linux)

        if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
            # Clang 5.0 still doesn't support -Wstrict-null-sentinel
            check_cxx_compiler_flag(-Wstrict-null-sentinel _STRICT_NULL_SENTINEL_IS_SUPPORTED)
            if (_STRICT_NULL_SENTINEL_IS_SUPPORTED)
                list(APPEND _GCC_COMMON_CXX_FLAGS "-Wstrict-null-sentinel")
            endif (_STRICT_NULL_SENTINEL_IS_SUPPORTED)

            # Code should be improved to render this not needed
            list(APPEND _GCC_COMMON_CXX_FLAGS "-Wno-error=unused-function -Wno-error=inconsistent-missing-override")
        else ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
            # GCC supports it
            list(APPEND _GCC_COMMON_CXX_FLAGS "-Wstrict-null-sentinel")
        endif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")

        string(REPLACE ";" " " _GCC_COMMON_C_AND_CXX_FLAGS_STRING "${_GCC_COMMON_C_AND_CXX_FLAGS}")
        string(REPLACE ";" " " _GCC_COMMON_CXX_FLAGS_STRING "${_GCC_COMMON_CXX_FLAGS}")

        string(APPEND CMAKE_C_FLAGS " ${_GCC_COMMON_C_AND_CXX_FLAGS_STRING}")
        string(APPEND CMAKE_CXX_FLAGS " ${_GCC_COMMON_C_AND_CXX_FLAGS_STRING} ${_GCC_COMMON_CXX_FLAGS_STRING}")

        set(QBT_ADDITONAL_FLAGS "${_GCC_COMMON_C_AND_CXX_FLAGS_STRING}" CACHE STRING
            "Additional qBittorent compile flags" FORCE)
        set(QBT_ADDITONAL_CXX_FLAGS "${_GCC_COMMON_CXX_FLAGS_STRING}" CACHE STRING
            "Additional qBittorent C++ compile flags" FORCE)

        # check whether we can enable -Og optimization for debug build
        # also let's enable -march=native for debug builds
        check_cxx_compiler_flag(-Og _DEBUG_OPTIMIZATION_LEVEL_IS_SUPPORTED)

        if (_DEBUG_OPTIMIZATION_LEVEL_IS_SUPPORTED)
            string(APPEND CMAKE_C_FLAGS_DEBUG " -Og -g3 -march=native -pipe" )
            string(APPEND CMAKE_CXX_FLAGS_DEBUG " -Og -g3 -march=native -pipe" )
        else(_DEBUG_OPTIMIZATION_LEVEL_IS_SUPPORTED)
            string(APPEND CMAKE_C_FLAGS_DEBUG " -O0 -g3 -march=native -pipe" )
            string(APPEND CMAKE_CXX_FLAGS_DEBUG " -O0 -g3 -march=native -pipe" )
        endif (_DEBUG_OPTIMIZATION_LEVEL_IS_SUPPORTED)
    endif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")

    if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
        set(QBT_ADDITONAL_FLAGS "-wd4290 -wd4275 -wd4251 /W4" CACHE STRING "Additional qBittorent compile flags")
        string(APPEND CMAKE_C_FLAGS " ${QBT_ADDITONAL_FLAGS}")
        string(APPEND CMAKE_CXX_FLAGS " ${QBT_ADDITONAL_FLAGS}")
    endif ()

# endif (NOT QBT_ADDITONAL_FLAGS)
endmacro(qbt_set_compiler_options)

