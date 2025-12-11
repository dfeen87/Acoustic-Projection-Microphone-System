cmake_minimum_required(VERSION 3.18)
project(AcousticProjectionMicrophone VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build options
option(BUILD_TESTS "Build unit tests" ON)
option(BUILD_BENCHMARKS "Build benchmarks" OFF)
option(ENABLE_SANITIZERS "Enable AddressSanitizer and UBSan" OFF)
option(ENABLE_COVERAGE "Enable code coverage" OFF)

# Compiler warnings
if(MSVC)
    add_compile_options(/W4 /WX)
else()
    if(NOT ENABLE_SANITIZERS)
        add_compile_options(-Wall -Wextra -Wpedantic -Werror)
    else()
        add_compile_options(-Wall -Wextra -Wpedantic)
    endif()
endif()

# Sanitizers
if(ENABLE_SANITIZERS)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

# Coverage
if(ENABLE_COVERAGE)
    add_compile_options(--coverage)
    add_link_options(--coverage)
endif()

# Required packages
find_package(Threads REQUIRED)

# FFTW3
find_library(FFTW3F_LIBRARY NAMES fftw3f)
find_path(FFTW3_INCLUDE_DIR NAMES fftw3.h)
if(NOT FFTW3F_LIBRARY OR NOT FFTW3_INCLUDE_DIR)
    message(FATAL_ERROR "FFTW3 float library not found. Install with: sudo apt-get install libfftw3-dev")
endif()

# TensorFlow Lite (optional)
find_library(TFLITE_LIBRARY NAMES tensorflowlite tensorflow-lite)
find_path(TFLITE_INCLUDE_DIR NAMES tensorflow/lite/interpreter.h)

if(TFLITE_LIBRARY AND TFLITE_INCLUDE_DIR)
    set(HAVE_TFLITE ON)
else()
    message(WARNING "TensorFlow Lite not found. Using mock translation engine.")
    set(HAVE_TFLITE OFF)
endif()

# Generate config header
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/include/apm/config.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/include/apm/config.h"
)

# Main library
add_library(apm STATIC
    src/audio_frame.cpp
    src/beamforming_engine.cpp
    src/noise_suppression_engine.cpp
    src/echo_cancellation_engine.cpp
    src/voice_activity_detector.cpp
    src/directional_projector.cpp
    src/apm_system.cpp
    src/fft_processor.cpp
)

if(HAVE_TFLITE)
    target_sources(apm PRIVATE src/tflite_translation_engine.cpp)
    target_compile_definitions(apm PRIVATE HAVE_TFLITE=1)
else()
    target_sources(apm PRIVATE src/mock_translation_engine.cpp)
endif()

target_include_directories(apm
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_BINARY_DIR}/include
    PRIVATE
        ${FFTW3_INCLUDE_DIR}
        $<$<BOOL:${HAVE_TFLITE}>:${TFLITE_INCLUDE_DIR}>
)

target_link_libraries(apm
    PUBLIC Threads::Threads
    PRIVATE ${FFTW3F_LIBRARY}
)

if(HAVE_TFLITE)
    target_link_libraries(apm PRIVATE ${TFLITE_LIBRARY})
endif()

# Example executable
add_executable(apm_example examples/main.cpp)
target_link_libraries(apm_example PRIVATE apm)

# Tests
if(BUILD_TESTS)
    enable_testing()
    include(FetchContent)

    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    add_executable(apm_tests
        tests/test_audio_frame.cpp
        tests/test_beamforming.cpp
        tests/test_noise_suppression.cpp
        tests/test_echo_cancellation.cpp
        tests/test_vad.cpp
        tests/test_fft.cpp
        tests/test_integration.cpp
    )

    target_link_libraries(apm_tests PRIVATE apm GTest::gtest_main GTest::gmock)
    include(GoogleTest)
    gtest_discover_tests(apm_tests)
endif()

# Install
install(TARGETS apm EXPORT apm-targets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/apm DESTINATION include)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/include/apm/config.h" DESTINATION include/apm)

install(EXPORT apm-targets
    FILE apm-targets.cmake
    NAMESPACE apm::
    DESTINATION lib/cmake/apm
)

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/apm-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/apm-config-version.cmake"
    DESTINATION lib/cmake/apm
)
