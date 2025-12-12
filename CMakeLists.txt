cmake_minimum_required(VERSION 3.18)
project(AcousticProjectionMicrophone VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build options
option(BUILD_TESTING "Build tests" OFF)
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

# Create a library with your APM core functionality
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
endif()

# Optional: link TFLite if present
if(HAVE_TFLITE)
    target_include_directories(apm_core PUBLIC ${TFLITE_INCLUDE_DIR})
    target_link_libraries(apm_core PUBLIC ${TFLITE_LIBRARY})
    target_compile_definitions(apm_core PUBLIC HAVE_TFLITE=1)
endif()

# Add local translation flag if enabled
if(ENABLE_LOCAL_TRANSLATION)
    target_compile_definitions(apm_core PUBLIC HAVE_LOCAL_TRANSLATION=1)
endif()

# Main executable
add_executable(apm main.cpp)
target_link_libraries(apm PRIVATE apm_core)

# Testing
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
    
    # Test executable
    add_executable(apm_test
        tests/test_apm_core.cpp
    )
    
    target_link_libraries(apm_test
        PRIVATE
        apm_core
        GTest::gtest
        GTest::gtest_main
    )
    
    # Add test to CTest
    add_test(NAME apm_test COMMAND apm_test)
    
    message(STATUS "Testing enabled - tests will be built")
endif()

# Benchmarks
if(BUILD_BENCHMARKS)
    message(STATUS "Benchmarks enabled - will be built when benchmark code is added")
endif()

# Installation
install(TARGETS apm apm_core
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

# Install Python translation bridge
if(ENABLE_LOCAL_TRANSLATION)
    install(FILES scripts/translation_bridge.py
        DESTINATION share/apm/scripts
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
    )
    install(FILES scripts/setup_translation.sh
        DESTINATION share/apm/scripts
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
    )
    install(FILES requirements-translation.txt
        DESTINATION share/apm/
    )
endif()

# Print configuration summary
message(STATUS "")
message(STATUS "=== APM System Configuration ===")
message(STATUS "Version: ${PROJECT_VERSION}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "")
message(STATUS "Features:")
message(STATUS "  Testing: ${BUILD_TESTING}")
message(STATUS "  Benchmarks: ${BUILD_BENCHMARKS}")
message(STATUS "  Coverage: ${ENABLE_COVERAGE}")
message(STATUS "  Local Translation: ${ENABLE_LOCAL_TRANSLATION}")
message(STATUS "  TensorFlow Lite: ${HAVE_TFLITE}")
if(FFTW3F_LIBRARY)
    message(STATUS "  FFTW3: Yes")
else()
    message(STATUS "  FFTW3: No")
endif()
message(STATUS "================================")
message(STATUS "")
