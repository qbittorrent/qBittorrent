# Find the paths to the dependent DLLs of qBittorrent that are not accessible in another way;
# Currently, they are the DLLs corresponding to the OpenSSL and ZLIB packages.
# Variables this macro defines: OPENSSL_LIBCRYPTO_DLL, OPENSSL_LIBSSL_DLL,
# and one or both of ZLIB_DLL_DBG, ZLIB_DLL_REL
# TODO: works for MSVC, what about MinGW?
# TODO: This can probably be entirely removed (or at least greatly simplified and made more robust)
# by using TARGET_RUNTIME_DLLS instead, in CMake >= 3.21, which even resolves and handles
# transitive dependencies automatically

macro(qbt_find_dependent_dlls)
    # Must manually find these DLLs, because the target names for these libs point at the .lib files, not the .dll files
    # Handling zlib is trickier than OpenSSL because FindZLIB sucks
    cmake_path(SET OPENSSL_DLLS_PATH "${OPENSSL_CRYPTO_LIBRARY}")
    cmake_path(GET OPENSSL_DLLS_PATH PARENT_PATH OPENSSL_DLLS_PATH)
    cmake_path(SET OPENSSL_DLLS_PATH NORMALIZE "${OPENSSL_DLLS_PATH}/../bin")
    find_file(OPENSSL_LIBSSL_DLL "libssl-1_1-x64.dll" PATHS "${OPENSSL_DLLS_PATH}" DOC "OpenSSL::SSL DLL path" REQUIRED
        NO_DEFAULT_PATH NO_PACKAGE_ROOT_PATH NO_CMAKE_PATH NO_CMAKE_ENVIRONMENT_PATH NO_SYSTEM_ENVIRONMENT_PATH NO_CMAKE_SYSTEM_PATH NO_CMAKE_FIND_ROOT_PATH
    )
    find_file(OPENSSL_LIBCRYPTO_DLL "libcrypto-1_1-x64.dll" PATHS "${OPENSSL_DLLS_PATH}" DOC "OpenSSL::Crypto DLL path" REQUIRED
        NO_DEFAULT_PATH NO_PACKAGE_ROOT_PATH NO_CMAKE_PATH NO_CMAKE_ENVIRONMENT_PATH NO_SYSTEM_ENVIRONMENT_PATH NO_CMAKE_SYSTEM_PATH NO_CMAKE_FIND_ROOT_PATH
    )
    list(APPEND ZLIB_NAMES_LIST "z.dll" "zlib.dll" "zdll.dll" "zlib1.dll" "zd.dll" "zlibd.dll" "zdlld.dll" "zlibd1.dll" "zlib1d.dll")
    get_target_property(ZLIB_RELEASE_LOCATION ZLIB::ZLIB IMPORTED_LOCATION_RELEASE)
    get_target_property(ZLIB_DEBUG_LOCATION ZLIB::ZLIB IMPORTED_LOCATION_DEBUG)
    get_target_property(ZLIB_ANY_LOCATION ZLIB::ZLIB IMPORTED_LOCATION)
    if (ZLIB_RELEASE_LOCATION)
        cmake_path(SET ZLIB_RELEASE_LOCATION "${ZLIB_RELEASE_LOCATION}")
        cmake_path(GET ZLIB_RELEASE_LOCATION PARENT_PATH ZLIB_RELEASE_LOCATION)
        cmake_path(SET ZLIB_RELEASE_LOCATION NORMALIZE "${ZLIB_RELEASE_LOCATION}/../bin")
        find_file(ZLIB_DLL_REL ${ZLIB_NAMES_LIST} PATHS "${ZLIB_RELEASE_LOCATION}" DOC "ZLIB::ZLIB DLL path (Release)" REQUIRED
            NO_DEFAULT_PATH NO_PACKAGE_ROOT_PATH NO_CMAKE_PATH NO_CMAKE_ENVIRONMENT_PATH NO_SYSTEM_ENVIRONMENT_PATH NO_CMAKE_SYSTEM_PATH NO_CMAKE_FIND_ROOT_PATH
        )
    endif()
    if (ZLIB_DEBUG_LOCATION)
        cmake_path(SET ZLIB_DEBUG_LOCATION "${ZLIB_DEBUG_LOCATION}")
        cmake_path(GET ZLIB_DEBUG_LOCATION PARENT_PATH ZLIB_DEBUG_LOCATION)
        cmake_path(SET ZLIB_DEBUG_LOCATION NORMALIZE "${ZLIB_DEBUG_LOCATION}/../bin")
        find_file(ZLIB_DLL_DBG ${ZLIB_NAMES_LIST} PATHS "${ZLIB_DEBUG_LOCATION}" DOC "ZLIB::ZLIB DLL path (Debug)" REQUIRED
            NO_DEFAULT_PATH NO_PACKAGE_ROOT_PATH NO_CMAKE_PATH NO_CMAKE_ENVIRONMENT_PATH NO_SYSTEM_ENVIRONMENT_PATH NO_CMAKE_SYSTEM_PATH NO_CMAKE_FIND_ROOT_PATH
        )
    endif()
    if (ZLIB_ANY_LOCATION)
        cmake_path(SET ZLIB_ANY_LOCATION "${ZLIB_ANY_LOCATION}")
        cmake_path(GET ZLIB_ANY_LOCATION PARENT_PATH ZLIB_ANY_LOCATION)
        cmake_path(SET ZLIB_ANY_LOCATION NORMALIZE "${ZLIB_ANY_LOCATION}/../bin")
        find_file(ZLIB_DLL_ANY ${ZLIB_NAMES_LIST} PATHS "${ZLIB_ANY_LOCATION}" DOC "ZLIB::ZLIB DLL path (any)" REQUIRED
            NO_DEFAULT_PATH NO_PACKAGE_ROOT_PATH NO_CMAKE_PATH NO_CMAKE_ENVIRONMENT_PATH NO_SYSTEM_ENVIRONMENT_PATH NO_CMAKE_SYSTEM_PATH NO_CMAKE_FIND_ROOT_PATH
        )
        set(ZLIB_DLL_REL "${ZLIB_DLL_ANY}")
        set(ZLIB_DLL_DBG "${ZLIB_DLL_ANY}")
    endif()
endmacro(qbt_find_dependent_dlls)
