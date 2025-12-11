cmake_minimum_required(VERSION 3.18)
project(AcousticProjectionMicrophone VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build options
option(ENABLE_TFLITE "Enable TensorFlow Lite translation engine" OFF)

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
