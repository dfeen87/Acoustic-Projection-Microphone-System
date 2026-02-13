#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "APM::apm_backend" for configuration "Release"
set_property(TARGET APM::apm_backend APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(APM::apm_backend PROPERTIES
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/bin/apm_backend"
  )

list(APPEND _cmake_import_check_targets APM::apm_backend )
list(APPEND _cmake_import_check_files_for_APM::apm_backend "${_IMPORT_PREFIX}/bin/apm_backend" )

# Import target "APM::apm_core" for configuration "Release"
set_property(TARGET APM::apm_core APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(APM::apm_core PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libapm_core.a"
  )

list(APPEND _cmake_import_check_targets APM::apm_core )
list(APPEND _cmake_import_check_files_for_APM::apm_core "${_IMPORT_PREFIX}/lib/libapm_core.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
