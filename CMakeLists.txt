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

# Main executable (only file you currently have)
add_executable(apm main.cpp)

# Link libraries (only threads for now)
target_link_libraries(apm PRIVATE Threads::Threads)

# Optional: link FFTW if present
if(FFTW3F_LIBRARY AND FFTW3_INCLUDE_DIR)
    target_include_directories(apm PRIVATE ${FFTW3_INCLUDE_DIR})
    target_link_libraries(apm PRIVATE ${FFTW3F_LIBRARY})
endif()

# Optional: link TFLite if present
if(HAVE_TFLITE)
    target_include_directories(apm PRIVATE ${TFLITE_INCLUDE_DIR})
    target_link_libraries(apm PRIVATE ${TFLITE_LIBRARY})
    target_compile_definitions(apm PRIVATE HAVE_TFLITE=1)
endif()

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
        # For Windows: Prevent overriding the parent project's compiler/linker settings
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(googletest)
    endif()
    
    # Create a simple test to verify coverage works
    add_executable(apm_test
        tests/test_main.cpp
    )
    
    target_link_libraries(apm_test
        PRIVATE
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
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
install(TARGETS apm
    RUNTIME DESTINATION bin
)
