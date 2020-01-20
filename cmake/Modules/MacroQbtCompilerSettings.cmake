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
            "-Wcast-qual -Wcast-align"
            "-Winvalid-pch -Wno-long-long"
            #"-fstack-protector-all"
            #"-Werror -Wno-error=deprecated-declarations"
        )
        set(_GCC_COMMON_CXX_FLAGS "-fexceptions -frtti"
            "-Woverloaded-virtual -Wold-style-cast"
            "-Wnon-virtual-dtor"
            #"-Weffc++"
            #"-Werror -Wno-error=cpp"
            # we should modify code to make these ones obsolete
            #"-Wno-error=sign-conversion -Wno-error=float-equal"
        )

        # GCC 4.8 has problems with std::array and its initialization
        if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 4.9)
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
            add_definitions(-D_DEFAULT_SOURCE)
        endif()

        # Clang 5.0 still doesn't support -Wstrict-null-sentinel
        if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
            check_cxx_compiler_flag(-Wstrict-null-sentinel _STRICT_NULL_SENTINEL_IS_SUPPORTED)
            if (_STRICT_NULL_SENTINEL_IS_SUPPORTED)
                list(APPEND _GCC_COMMON_CXX_FLAGS "-Wstrict-null-sentinel")
            endif (_STRICT_NULL_SENTINEL_IS_SUPPORTED)

            # Code should be improved to render this not needed
            list(APPEND _GCC_COMMON_CXX_FLAGS "-Wno-error=unused-function")
        else ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
            # GCC supports it
            list(APPEND _GCC_COMMON_CXX_FLAGS "-Wstrict-null-sentinel")
        endif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")

        string(REPLACE ";" " " _GCC_COMMON_C_AND_CXX_FLAGS_STRING "${_GCC_COMMON_C_AND_CXX_FLAGS}")
        string(REPLACE ";" " " _GCC_COMMON_CXX_FLAGS_STRING "${_GCC_COMMON_CXX_FLAGS}")

        string(APPEND CMAKE_C_FLAGS " ${_GCC_COMMON_C_AND_CXX_FLAGS_STRING}")
        string(APPEND CMAKE_CXX_FLAGS " ${_GCC_COMMON_C_AND_CXX_FLAGS_STRING} ${_GCC_COMMON_CXX_FLAGS_STRING}")
    endif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")

    if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
        set(QBT_ADDITONAL_FLAGS "/wd4251 /wd4275 /wd4290  /W4" CACHE STRING "Additional qBittorent compile flags")
    endif ()

    string(APPEND CMAKE_C_FLAGS " ${QBT_ADDITONAL_FLAGS}")
    string(APPEND CMAKE_CXX_FLAGS " ${QBT_ADDITONAL_FLAGS}")

# endif (NOT QBT_ADDITONAL_FLAGS)
endmacro(qbt_set_compiler_options)
