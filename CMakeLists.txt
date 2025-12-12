cmake_minimum_required(VERSION 3.18)
project(AcousticProjectionMicrophone VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build options
option(BUILD_TESTING "Build tests" ON)
option(BUILD_EXAMPLES "Build examples" ON)
option(BUILD_BENCHMARKS "Build benchmarks" OFF)
option(ENABLE_TFLITE "Enable TensorFlow Lite translation engine" OFF)
option(ENABLE_COVERAGE "Enable coverage reporting" OFF)
option(ENABLE_LOCAL_TRANSLATION "Enable local Whisper+NLLB translation" ON)

# Coverage support
if(ENABLE_COVERAGE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        message(STATUS "Coverage reporting enabled")
        add_compile_options(--coverage -fprofile-arcs -ftest-coverage)
        add_link_options(--coverage)
    else()
        message(WARNING "Coverage only supported with GCC/Clang")
    endif()
endif()

# Compiler warnings
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Threads
find_package(Threads REQUIRED)

# FFTW3 (optional for now — only needed if you add fft_processor.cpp later)
find_library(FFTW3F_LIBRARY NAMES fftw3f)
find_path(FFTW3_INCLUDE_DIR NAMES fftw3.h)

# TensorFlow Lite (optional)
find_library(TFLITE_LIBRARY NAMES tensorflowlite tensorflow-lite)
find_path(TFLITE_INCLUDE_DIR NAMES tensorflow/lite/interpreter.h)

if(TFLITE_LIBRARY AND TFLITE_INCLUDE_DIR)
    set(HAVE_TFLITE ON)
else()
    set(HAVE_TFLITE OFF)
endif()

# Generate config header
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/include/apm/config.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/include/apm/config.h"
)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}/include
)

# ============================================================================
# APM CORE LIBRARY
# ============================================================================

set(APM_CORE_SOURCES
    src/apm_core.cpp
)

# Add local translation engine if enabled
if(ENABLE_LOCAL_TRANSLATION)
    list(APPEND APM_CORE_SOURCES src/local_translation_engine.cpp)
    message(STATUS "Local translation enabled (Whisper + NLLB)")
endif()

add_library(apm_core STATIC ${APM_CORE_SOURCES})

target_include_directories(apm_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}/include
)

target_link_libraries(apm_core PUBLIC Threads::Threads)

# Optional: link FFTW if present
if(FFTW3F_LIBRARY AND FFTW3_INCLUDE_DIR)
    target_include_directories(apm_core PUBLIC ${FFTW3_INCLUDE_DIR})
    target_link_libraries(apm_core PUBLIC ${FFTW3F_LIBRARY})
    message(STATUS "FFTW3 found and linked")
endif()

# Optional: link TFLite if present
if(HAVE_TFLITE)
    target_include_directories(apm_core PUBLIC ${TFLITE_INCLUDE_DIR})
    target_link_libraries(apm_core PUBLIC ${TFLITE_LIBRARY})
    target_compile_definitions(apm_core PUBLIC HAVE_TFLITE=1)
    message(STATUS "TensorFlow Lite found and linked")
endif()

# Add local translation flag if enabled
if(ENABLE_LOCAL_TRANSLATION)
    target_compile_definitions(apm_core PUBLIC HAVE_LOCAL_TRANSLATION=1)
endif()

# ============================================================================
# MAIN EXECUTABLE
# ============================================================================

add_executable(apm main.cpp)
target_link_libraries(apm PRIVATE apm_core)

# ============================================================================
# EXAMPLES
# ============================================================================

if(BUILD_EXAMPLES)
    # Translation example - demonstrates full pipeline with real translation
    if(ENABLE_LOCAL_TRANSLATION)
        add_executable(translation_example examples/translation_example.cpp)
        target_link_libraries(translation_example PRIVATE apm_core Threads::Threads)
        target_include_directories(translation_example PRIVATE 
            ${CMAKE_CURRENT_SOURCE_DIR}/include
        )
        message(STATUS "Building translation_example")
    endif()
    
    # Basic APM example (if you have one)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/examples/basic_example.cpp)
        add_executable(basic_example examples/basic_example.cpp)
        target_link_libraries(basic_example PRIVATE apm_core)
        message(STATUS "Building basic_example")
    endif()
    
    message(STATUS "Examples enabled - will be built")
endif()

# ============================================================================
# TESTING
# ============================================================================

if(BUILD_TESTING)
    enable_testing()
    
    # Find GTest
    find_package(GTest)
    
    if(NOT GTest_FOUND)
        # Fetch GTest if not found
        include(FetchContent)
        FetchContent_Declare(
            googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG        v1.14.0
        )
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(googletest)
    endif()
    
    # Core functionality tests
    add_executable(apm_test
        tests/test_apm_core.cpp
    )
    
    target_link_libraries(apm_test
        PRIVATE
        apm_core
        GTest::gtest
        GTest::gtest_main
    )
    
    add_test(NAME apm_core_test COMMAND apm_test)
    
    # Translation integration tests (if enabled)
    if(ENABLE_LOCAL_TRANSLATION)
        add_executable(test_translation
            tests/test_translation_integration.cpp
        )
        
        target_link_libraries(test_translation
            PRIVATE
            apm_core
            Threads::Threads
        )
        
        target_include_directories(test_translation PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
        )
        
        add_test(NAME translation_integration_test COMMAND test_translation)
        
        message(STATUS "Building translation integration tests")
    endif()
    
    message(STATUS "Testing enabled - tests will be built")
endif()

# ============================================================================
# BENCHMARKS
# ============================================================================

if(BUILD_BENCHMARKS)
    # Add benchmark executable when ready
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/benchmarks/apm_bench.cpp)
        add_executable(apm_bench benchmarks/apm_bench.cpp)
        target_link_libraries(apm_bench PRIVATE apm_core)
        message(STATUS "Building benchmarks")
    else()
        message(STATUS "Benchmarks enabled but no benchmark code found")
    endif()
endif()

# ============================================================================
# INSTALLATION
# ============================================================================

# Install binaries
install(TARGETS apm apm_core
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

# Install headers
install(DIRECTORY include/apm
    DESTINATION include
    FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h"
)

# Install generated config header
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/include/apm/config.h
    DESTINATION include/apm
)

# Install Python translation bridge and setup scripts
if(ENABLE_LOCAL_TRANSLATION)
    install(FILES 
        scripts/translation_bridge.py
        DESTINATION share/apm/scripts
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE 
                    GROUP_READ GROUP_EXECUTE 
                    WORLD_READ WORLD_EXECUTE
    )
    
    install(FILES 
        scripts/setup_translation.sh
        DESTINATION share/apm/scripts
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE 
                    GROUP_READ GROUP_EXECUTE 
                    WORLD_READ WORLD_EXECUTE
    )
    
    # Install requirements file if it exists
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/requirements-translation.txt)
        install(FILES requirements-translation.txt
            DESTINATION share/apm/
        )
    endif()
    
    message(STATUS "Translation scripts will be installed")
endif()

# Install documentation
install(FILES 
    README.md
    LICENSE
    DESTINATION share/doc/apm
)

# Install additional docs if they exist
foreach(doc BUILD.md ROADMAP.md TRANSLATOR_QUICKSTART.md TRANSLATION_BRIDGE.md)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${doc})
        install(FILES ${doc} DESTINATION share/doc/apm)
    endif()
endforeach()

# Install examples (source code for reference)
if(BUILD_EXAMPLES)
    install(DIRECTORY examples/
        DESTINATION share/apm/examples
        FILES_MATCHING PATTERN "*.cpp" PATTERN "*.hpp"
    )
endif()

# ============================================================================
# PACKAGE CONFIGURATION
# ============================================================================

# Generate package config files for find_package()
include(CMakePackageConfigHelpers)

# Create config file
configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/APMConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/APMConfig.cmake
    INSTALL_DESTINATION lib/cmake/APM
)

# Create version file
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/APMConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

# Install package config files
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/APMConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/APMConfigVersion.cmake
    DESTINATION lib/cmake/APM
)

# Export targets
install(EXPORT APMTargets
    FILE APMTargets.cmake
    NAMESPACE APM::
    DESTINATION lib/cmake/APM
)

# ============================================================================
# CONFIGURATION SUMMARY
# ============================================================================

message(STATUS "")
message(STATUS "╔════════════════════════════════════════════════════════╗")
message(STATUS "║        APM System Configuration Summary               ║")
message(STATUS "╚════════════════════════════════════════════════════════╝")
message(STATUS "")
message(STATUS "Version: ${PROJECT_VERSION}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "")
message(STATUS "──────────────────────────────────────────────────────────")
message(STATUS "Build Options:")
message(STATUS "──────────────────────────────────────────────────────────")
message(STATUS "  Examples:            ${BUILD_EXAMPLES}")
message(STATUS "  Testing:             ${BUILD_TESTING}")
message(STATUS "  Benchmarks:          ${BUILD_BENCHMARKS}")
message(STATUS "  Coverage:            ${ENABLE_COVERAGE}")
message(STATUS "")
message(STATUS "──────────────────────────────────────────────────────────")
message(STATUS "Features:")
message(STATUS "──────────────────────────────────────────────────────────")
message(STATUS "  Local Translation:   ${ENABLE_LOCAL_TRANSLATION}")
if(ENABLE_LOCAL_TRANSLATION)
    message(STATUS "    ↳ Whisper + NLLB (200+ languages)")
endif()
message(STATUS "  TensorFlow Lite:     ${HAVE_TFLITE}")
if(FFTW3F_LIBRARY)
    message(STATUS "  FFTW3:               Yes")
else()
    message(STATUS "  FFTW3:               No (optional)")
endif()
message(STATUS "")
message(STATUS "──────────────────────────────────────────────────────────")
message(STATUS "Targets to Build:")
message(STATUS "──────────────────────────────────────────────────────────")
message(STATUS "  apm                  (main executable)")
message(STATUS "  apm_core             (static library)")
if(BUILD_EXAMPLES AND ENABLE_LOCAL_TRANSLATION)
    message(STATUS "  translation_example  (full pipeline demo)")
endif()
if(BUILD_TESTING)
    message(STATUS "  apm_test             (core tests)")
    if(ENABLE_LOCAL_TRANSLATION)
        message(STATUS "  test_translation     (integration tests)")
    endif()
endif()
message(STATUS "")
if(ENABLE_LOCAL_TRANSLATION)
    message(STATUS "──────────────────────────────────────────────────────────")
    message(STATUS "Next Steps:")
    message(STATUS "──────────────────────────────────────────────────────────")
    message(STATUS "  1. Build: cmake --build . -j$(nproc)")
    message(STATUS "  2. Setup translation: ./scripts/setup_translation.sh")
    message(STATUS "  3. Test: ./test_translation")
    message(STATUS "  4. Run: ./translation_example")
    message(STATUS "")
endif()
message(STATUS "════════════════════════════════════════════════════════════")
message(STATUS "")
