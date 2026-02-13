
####### Expanded from @PACKAGE_INIT@ by configure_package_config_file() #######
####### Any changes to this file will be overwritten by the next CMake run ####
####### The input file was APMConfig.cmake.in                            ########

get_filename_component(PACKAGE_PREFIX_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../" ABSOLUTE)

macro(set_and_check _var _file)
  set(${_var} "${_file}")
  if(NOT EXISTS "${_file}")
    message(FATAL_ERROR "File or directory ${_file} referenced by variable ${_var} does not exist !")
  endif()
endmacro()

macro(check_required_components _NAME)
  foreach(comp ${${_NAME}_FIND_COMPONENTS})
    if(NOT ${_NAME}_${comp}_FOUND)
      if(${_NAME}_FIND_REQUIRED_${comp})
        set(${_NAME}_FOUND FALSE)
      endif()
    endif()
  endforeach()
endmacro()

####################################################################################

# APM System Package Configuration
# Version 2.0.0

include(CMakeFindDependencyMacro)

# Required dependencies
find_dependency(Threads REQUIRED)

# Optional dependencies
find_package(FFTW3 QUIET)

# Check if local translation is enabled
set(APM_HAS_LOCAL_TRANSLATION ON)
set(APM_HAS_TFLITE OFF)

# Include targets
if(NOT TARGET APM::apm_core)
    include("${CMAKE_CURRENT_LIST_DIR}/APMTargets.cmake")
endif()

# Provide variables
set(APM_LIBRARIES APM::apm_core)
set(APM_INCLUDE_DIRS "/include")
set(APM_VERSION "2.0.0")

# Feature flags
set(APM_LOCAL_TRANSLATION_ENABLED ON)
set(APM_TFLITE_ENABLED OFF)

# Check that all required components are present
check_required_components(APM)
