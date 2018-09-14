# Borrowed from Avogadro project (https://github.com/OpenChemistry/avogadroapp)

#.rst:
# DeployQt5
# ---------
#
# Functions to help assemble a standalone Qt5 executable.
#
# A collection of CMake utility functions useful for deploying Qt5
# executables.
#
# The following functions are provided by this module:
#
# ::
#
#    write_qt5_conf
#    resolve_qt5_paths
#    fixup_qt5_executable
#    install_qt5_plugin_path
#    install_qt5_plugin
#    install_qt5_executable
#
# Requires CMake 2.8.9 or greater because Qt 5 does.
# Also depends on BundleUtilities.cmake.
#
# ::
#
#   WRITE_QT5_CONF(<qt_conf_dir> <qt_conf_contents>)
#
# Writes a qt.conf file with the <qt_conf_contents> into <qt_conf_dir>.
#
# ::
#
#   RESOLVE_QT5_PATHS(<paths_var> [<executable_path>])
#
# Loop through <paths_var> list and if any don't exist resolve them
# relative to the <executable_path> (if supplied) or the
# CMAKE_INSTALL_PREFIX.
#
# ::
#
#   FIXUP_QT5_EXECUTABLE(<executable> [<qtplugins> <libs> <dirs> <plugins_dir> <request_qt_conf>])
#
# Copies Qt plugins, writes a Qt configuration file (if needed) and
# fixes up a Qt5 executable using BundleUtilities so it is standalone
# and can be drag-and-drop copied to another machine as long as all of
# the system libraries are compatible.
#
# <executable> should point to the executable to be fixed-up.
#
# <qtplugins> should contain a list of the names or paths of any Qt
# plugins to be installed.
#
# <libs> will be passed to BundleUtilities and should be a list of any
# already installed plugins, libraries or executables to also be
# fixed-up.
#
# <dirs> will be passed to BundleUtilities and should contain and
# directories to be searched to find library dependencies.
#
# <plugins_dir> allows an custom plugins directory to be used.
#
# <request_qt_conf> will force a qt.conf file to be written even if not
# needed.
#
# ::
#
#   INSTALL_QT5_PLUGIN_PATH(plugin executable copy installed_plugin_path_var <plugins_dir> <component> <configurations>)
#
# Install (or copy) a resolved <plugin> to the default plugins directory
# (or <plugins_dir>) relative to <executable> and store the result in
# <installed_plugin_path_var>.
#
# If <copy> is set to TRUE then the plugins will be copied rather than
# installed.  This is to allow this module to be used at CMake time
# rather than install time.
#
# If <component> is set then anything installed will use this COMPONENT.
#
# ::
#
#   INSTALL_QT5_PLUGIN(plugin executable copy installed_plugin_path_var <plugins_dir> <component>)
#
# Install (or copy) an unresolved <plugin> to the default plugins
# directory (or <plugins_dir>) relative to <executable> and store the
# result in <installed_plugin_path_var>.  See documentation of
# INSTALL_QT5_PLUGIN_PATH.
#
# ::
#
#   INSTALL_QT5_EXECUTABLE(<executable> [<qtplugins> <libs> <dirs> <plugins_dir> <request_qt_conf> <component>])
#
# Installs Qt plugins, writes a Qt configuration file (if needed) and
# fixes up a Qt5 executable using BundleUtilities so it is standalone
# and can be drag-and-drop copied to another machine as long as all of
# the system libraries are compatible.  The executable will be fixed-up
# at install time.  <component> is the COMPONENT used for bundle fixup
# and plugin installation.  See documentation of FIXUP_QT5_BUNDLE.

#=============================================================================
# Copyright 2011 Mike McQuaid <mike@mikemcquaid.com>
#
# Distributed under the OSI-approved BSD License (the "License");
# see accompanying file Copyright.txt for details.
#
# This software is distributed WITHOUT ANY WARRANTY; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the License for more information.
#=============================================================================
# (To distribute this file outside of CMake, substitute the full
#  License text for the above reference.)

# The functions defined in this file depend on the fixup_bundle function
# (and others) found in BundleUtilities.cmake

include(BundleUtilities)
set(DeployQt5_cmake_dir "${CMAKE_CURRENT_LIST_DIR}")
set(DeployQt5_apple_plugins_dir "PlugIns")

function(write_qt5_conf qt_conf_dir qt_conf_contents)
  set(qt_conf_path "${qt_conf_dir}/qt.conf")
  message(STATUS "Writing ${qt_conf_path}")
  file(WRITE "${qt_conf_path}" "${qt_conf_contents}")
endfunction()

function(resolve_qt5_paths paths_var)
  set(executable_path ${ARGV1})

  set(paths_resolved)
  foreach(path ${${paths_var}})
    if(EXISTS "${path}")
      list(APPEND paths_resolved "${path}")
    else()
      if(${executable_path})
        list(APPEND paths_resolved "${executable_path}/${path}")
      else()
        list(APPEND paths_resolved "\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${path}")
      endif()
    endif()
  endforeach()
  set(${paths_var} ${paths_resolved} PARENT_SCOPE)
endfunction()

function(fixup_qt5_executable executable)
  set(qtplugins ${ARGV1})
  set(libs ${ARGV2})
  set(dirs ${ARGV3})
  set(plugins_dir ${ARGV4})
  set(request_qt_conf ${ARGV5})

  message(STATUS "fixup_qt5_executable")
  message(STATUS "  executable='${executable}'")
  message(STATUS "  qtplugins='${qtplugins}'")
  message(STATUS "  libs='${libs}'")
  message(STATUS "  dirs='${dirs}'")
  message(STATUS "  plugins_dir='${plugins_dir}'")
  message(STATUS "  request_qt_conf='${request_qt_conf}'")

  if(QT_LIBRARY_DIR)
    list(APPEND dirs "${QT_LIBRARY_DIR}")
  endif()
  if(QT_BINARY_DIR)
    list(APPEND dirs "${QT_BINARY_DIR}")
  endif()

  if(APPLE)
    set(qt_conf_dir "${executable}/Contents/Resources")
    set(executable_path "${executable}")
    set(write_qt_conf TRUE)
    if(NOT plugins_dir)
      set(plugins_dir "${DeployQt5_apple_plugins_dir}")
    endif()
  else()
    get_filename_component(executable_path "${executable}" PATH)
    if(NOT executable_path)
      set(executable_path ".")
    endif()
    set(qt_conf_dir "${executable_path}")
    set(write_qt_conf ${request_qt_conf})
  endif()

  foreach(plugin ${qtplugins})
    set(installed_plugin_path "")
    install_qt5_plugin("${plugin}" "${executable}" 1 installed_plugin_path)
    list(APPEND libs ${installed_plugin_path})
  endforeach()

  foreach(lib ${libs})
    if(NOT EXISTS "${lib}")
      message(FATAL_ERROR "Library does not exist: ${lib}")
    endif()
  endforeach()

  resolve_qt5_paths(libs "${executable_path}")

  if(write_qt_conf)
    set(qt_conf_contents "[Paths]\nPlugins = ${plugins_dir}")
    write_qt5_conf("${qt_conf_dir}" "${qt_conf_contents}")
  endif()

  fixup_bundle("${executable}" "${libs}" "${dirs}")
endfunction()

function(install_qt5_plugin_path plugin executable copy installed_plugin_path_var)
  set(plugins_dir ${ARGV4})
  set(component ${ARGV5})
  set(configurations ${ARGV6})
  if(EXISTS "${plugin}")
    if(APPLE)
      if(NOT plugins_dir)
        set(plugins_dir "${DeployQt5_apple_plugins_dir}")
      endif()
      set(plugins_path "${executable}/Contents/${plugins_dir}")
    else()
      get_filename_component(plugins_path "${executable}" PATH)
      if(NOT plugins_path)
        set(plugins_path ".")
      endif()
      if(plugins_dir)
        set(plugins_path "${plugins_path}/${plugins_dir}")
      endif()
    endif()

    set(plugin_group "")

    get_filename_component(plugin_path "${plugin}" PATH)
    get_filename_component(plugin_parent_path "${plugin_path}" PATH)
    get_filename_component(plugin_parent_dir_name "${plugin_parent_path}" NAME)
    get_filename_component(plugin_name "${plugin}" NAME)
    string(TOLOWER "${plugin_parent_dir_name}" plugin_parent_dir_name)

    if("${plugin_parent_dir_name}" STREQUAL "plugins")
      get_filename_component(plugin_group "${plugin_path}" NAME)
      set(${plugin_group_var} "${plugin_group}")
    endif()
    set(plugins_path "${plugins_path}/${plugin_group}")

    if(${copy})
      file(MAKE_DIRECTORY "${plugins_path}")
      file(COPY "${plugin}" DESTINATION "${plugins_path}")
    else()
      if(configurations AND (CMAKE_CONFIGURATION_TYPES OR CMAKE_BUILD_TYPE))
        set(configurations CONFIGURATIONS ${configurations})
      else()
        unset(configurations)
      endif()
      install(FILES "${plugin}" DESTINATION "${plugins_path}" ${configurations} ${component})
    endif()
    set(${installed_plugin_path_var} "${plugins_path}/${plugin_name}" PARENT_SCOPE)
  endif()
endfunction()

function(install_qt5_plugin plugin executable copy installed_plugin_path_var)
  set(plugins_dir ${ARGV4})
  set(component ${ARGV5})
  if(EXISTS "${plugin}")
    install_qt5_plugin_path("${plugin}" "${executable}" "${copy}" "${installed_plugin_path_var}" "${plugins_dir}" "${component}")
  else()
    string(TOUPPER "QT_${plugin}_PLUGIN" plugin_var)
    set(plugin_release_var "${plugin_var}_RELEASE")
    set(plugin_debug_var "${plugin_var}_DEBUG")
    set(plugin_release "${${plugin_release_var}}")
    set(plugin_debug "${${plugin_debug_var}}")
    if(DEFINED "${plugin_release_var}" AND DEFINED "${plugin_debug_var}" AND NOT EXISTS "${plugin_release}" AND NOT EXISTS "${plugin_debug}")
      message(WARNING "Qt plugin \"${plugin}\" not recognized or found.")
    endif()
    if(NOT EXISTS "${${plugin_debug_var}}")
      set(plugin_debug "${plugin_release}")
    endif()

    if(CMAKE_CONFIGURATION_TYPES OR CMAKE_BUILD_TYPE)
      install_qt5_plugin_path("${plugin_release}" "${executable}" "${copy}" "${installed_plugin_path_var}_release" "${plugins_dir}" "${component}" "Release|RelWithDebInfo|MinSizeRel")
      install_qt5_plugin_path("${plugin_debug}" "${executable}" "${copy}" "${installed_plugin_path_var}_debug" "${plugins_dir}" "${component}" "Debug")

      if(CMAKE_BUILD_TYPE MATCHES "^Debug$")
        set(${installed_plugin_path_var} ${${installed_plugin_path_var}_debug})
      else()
        set(${installed_plugin_path_var} ${${installed_plugin_path_var}_release})
      endif()
    else()
      install_qt5_plugin_path("${plugin_release}" "${executable}" "${copy}" "${installed_plugin_path_var}" "${plugins_dir}" "${component}")
    endif()
  endif()
  set(${installed_plugin_path_var} ${${installed_plugin_path_var}} PARENT_SCOPE)
endfunction()

function(install_qt5_executable executable)
  set(qtplugins ${ARGV1})
  set(libs ${ARGV2})
  set(dirs ${ARGV3})
  set(plugins_dir ${ARGV4})
  set(request_qt_conf ${ARGV5})
  set(component ${ARGV6})
  if(QT_LIBRARY_DIR)
    list(APPEND dirs "${QT_LIBRARY_DIR}")
  endif()
  if(QT_BINARY_DIR)
    list(APPEND dirs "${QT_BINARY_DIR}")
  endif()
  if(TARGET Qt5::Core)
    get_property(_locCore TARGET Qt5::Core PROPERTY LOCATION_RELEASE)
    get_filename_component(_loc ${_locCore} DIRECTORY)
    message(STATUS "Adding Qt 5 directory: ${_loc}")
    list(APPEND dirs "${_loc}")
  else()
    message(FATAL_ERROR "No Qt5::Core target found, ensure it is available")
  endif()
  if(component)
    set(component COMPONENT ${component})
  else()
    unset(component)
  endif()

  get_filename_component(executable_absolute "${executable}" ABSOLUTE)
  if(EXISTS "${QT_QTCORE_LIBRARY_RELEASE}")
    gp_file_type("${executable_absolute}" "${QT_QTCORE_LIBRARY_RELEASE}" qtcore_type)
  elseif(EXISTS "${QT_QTCORE_LIBRARY_DEBUG}")
    gp_file_type("${executable_absolute}" "${QT_QTCORE_LIBRARY_DEBUG}" qtcore_type)
  endif()
  if(qtcore_type STREQUAL "system")
    set(qt_plugins_dir "")
  endif()

  if(QT_IS_STATIC)
    message(WARNING "Qt built statically: not installing plugins.")
  else()
    if(APPLE)
      get_property(loc TARGET Qt5::QCocoaIntegrationPlugin
        PROPERTY LOCATION_RELEASE)
      install_qt5_plugin("${loc}" "${executable}" 0 installed_plugin_paths
        "PlugIns" "${component}")
      list(APPEND libs ${installed_plugin_paths})
    elseif(WIN32)
      get_property(loc TARGET Qt5::QWindowsIntegrationPlugin
        PROPERTY LOCATION_RELEASE)
      install_qt5_plugin("${loc}" "${executable}" 0 installed_plugin_paths
        "" "${component}")
      list(APPEND libs ${installed_plugin_paths})
    endif()
    foreach(plugin ${qtplugins})
      set(installed_plugin_paths "")
      install_qt5_plugin("${plugin}" "${executable}" 0 installed_plugin_paths "${plugins_dir}" "${component}")
      list(APPEND libs ${installed_plugin_paths})
    endforeach()
  endif()

  resolve_qt5_paths(libs "")

  install(CODE
  "include(\"${DeployQt5_cmake_dir}/DeployQt5.cmake\")
  set(BU_CHMOD_BUNDLE_ITEMS TRUE)
  fixup_qt5_executable(\"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${executable}\" \"\" \"${libs}\" \"${dirs}\" \"${plugins_dir}\" \"${request_qt_conf}\")"
    ${component}
    )
endfunction()
