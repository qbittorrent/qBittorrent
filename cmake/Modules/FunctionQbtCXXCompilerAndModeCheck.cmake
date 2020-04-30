# Function for ensuring the build does not use a lower than the required minimum C++ mode, _min_std.
# It fails the build if:
# - the compiler does not fully support _min_std
# - the user specified a mode lower than _min_std via the CMAKE_CXX_STANDARD variable
# If both checks are successful, it sets the following variables at the PARENT_SCOPE:
# - CMAKE_CXX_STANDARD with the value _min_std, if it was not already set to a value >= _min_std
# - CMAKE_CXX_STANDARD_REQUIRED with the value ON

function(qbt_minimum_cxx_mode_check _min_std)
    # ensure the compiler fully supports the minimum required C++ mode.
    if(NOT CMAKE_CXX${_min_std}_STANDARD__HAS_FULL_SUPPORT)
        message(FATAL_ERROR "${PROJECT_NAME} requires a compiler with full C++${_min_std} support")
    endif()

    # now we know that the compiler fully supports the minimum required C++ mode,
    # but we must still prevent the user or compiler from forcing/defaulting to an insufficient C++ mode
    if(NOT CMAKE_CXX_STANDARD)
        set(CMAKE_CXX_STANDARD ${_min_std} PARENT_SCOPE)
    elseif((CMAKE_CXX_STANDARD VERSION_LESS ${_min_std}) OR (CMAKE_CXX_STANDARD VERSION_EQUAL 98))
        message(FATAL_ERROR "${PROJECT_NAME} has to be built with at least C++${_min_std} mode.")
    endif()

    set(CMAKE_CXX_STANDARD_REQUIRED ON PARENT_SCOPE)
endfunction()
