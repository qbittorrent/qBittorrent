# Helper functions for coupling add_feature_info() or CMAKE_DEPENDENT_OPTION() and option()

function(feature_option _name _description _default)
    string(CONCAT _desc "${_description} (default: ${_default})")
    option("${_name}" "${_desc}" "${_default}")
    add_feature_info("${_name}" "${_name}" "${_desc}")
endfunction()

include(CMakeDependentOption)
function(feature_option_dependent _name _description _default_opt _dependency _default_dep_not_sat)
    string(CONCAT _desc "${_description} (default: ${_default_opt}; depends on condition: ${_dependency})")
    CMAKE_DEPENDENT_OPTION("${_name}" "${_desc}" "${_default_opt}" "${_dependency}" "${_default_dep_not_sat}")
    add_feature_info("${_name}" "${_name}" "${_desc}")
endfunction()
