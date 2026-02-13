#pragma once

// APM System Configuration
// Generated automatically by CMake - DO NOT EDIT

#define APM_VERSION_MAJOR 2
#define APM_VERSION_MINOR 0
#define APM_VERSION_PATCH 0
#define APM_VERSION "2.0.0"

// Feature flags
/* #undef HAVE_LOCAL_TRANSLATION */
/* #undef HAVE_TFLITE */
/* #undef HAVE_ENCRYPTION */

// Platform detection
#if defined(_WIN32) || defined(_WIN64)
    #define APM_PLATFORM_WINDOWS
#elif defined(__APPLE__)
    #define APM_PLATFORM_MACOS
#elif defined(__linux__)
    #define APM_PLATFORM_LINUX
#else
    #define APM_PLATFORM_UNKNOWN
#endif

// Compiler detection
#if defined(__clang__)
    #define APM_COMPILER_CLANG
#elif defined(__GNUC__) || defined(__GNUG__)
    #define APM_COMPILER_GCC
#elif defined(_MSC_VER)
    #define APM_COMPILER_MSVC
#endif

namespace apm {
namespace config {

constexpr const char* version = APM_VERSION;
constexpr int version_major = APM_VERSION_MAJOR;
constexpr int version_minor = APM_VERSION_MINOR;
constexpr int version_patch = APM_VERSION_PATCH;

constexpr bool has_local_translation =
#ifdef HAVE_LOCAL_TRANSLATION
    true;
#else
    false;
#endif

constexpr bool has_tflite =
#ifdef HAVE_TFLITE
    true;
#else
    false;
#endif

constexpr bool has_encryption =
#ifdef HAVE_ENCRYPTION
    true;
#else
    false;
#endif

} // namespace config
} // namespace apm
