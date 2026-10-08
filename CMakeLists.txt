cmake_minimum_required(VERSION 3.18)
project(AcousticProjectionMicrophone VERSION 11.0.0 LANGUAGES CXX)

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
option(ENABLE_ENCRYPTION "Enable encryption support" ON)

set(APM_ENABLE_TESTS ${BUILD_TESTING})

# Wall-clock DSP budgets are meaningful only in an explicitly requested,
# uninstrumented Release build. Ordinary correctness and sanitizer runs never
# register performance checks.
if(BUILD_BENCHMARKS)
    if(NOT BUILD_TESTING OR NOT CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_CONFIGURATION_TYPES)
        message(FATAL_ERROR "BUILD_BENCHMARKS requires BUILD_TESTING=ON and a single-configuration Release build")
    endif()
    if(ENABLE_COVERAGE OR "${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_RELEASE}" MATCHES "-fsanitize[= ]|/fsanitize")
        message(FATAL_ERROR "BUILD_BENCHMARKS requires an uninstrumented build")
    endif()
endif()

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

# Threads
find_package(Threads REQUIRED)

# FFTW3 (optional for now — only needed if you add fft_processor.cpp later)
find_library(FFTW3F_LIBRARY NAMES fftw3f)
find_path(FFTW3_INCLUDE_DIR NAMES fftw3.h)

if(FFTW3F_LIBRARY AND FFTW3_INCLUDE_DIR)
    set(HAVE_FFTW3 ON)
else()
    set(HAVE_FFTW3 OFF)
endif()

# TensorFlow Lite (optional)
find_library(TFLITE_LIBRARY NAMES tensorflowlite tensorflow-lite)
find_path(TFLITE_INCLUDE_DIR NAMES tensorflow/lite/interpreter.h)

if(ENABLE_TFLITE AND TFLITE_LIBRARY AND TFLITE_INCLUDE_DIR)
    set(HAVE_TFLITE ON)
else()
    set(HAVE_TFLITE OFF)
endif()

# Libsodium (for encryption)
if(ENABLE_ENCRYPTION)
    find_library(SODIUM_LIBRARY NAMES sodium)
    find_path(SODIUM_INCLUDE_DIR NAMES sodium.h)
    
    if(SODIUM_LIBRARY AND SODIUM_INCLUDE_DIR)
        set(HAVE_ENCRYPTION ON)
        message(STATUS "Libsodium found - encryption enabled")
    else()
        set(HAVE_ENCRYPTION OFF)
        message(WARNING "Libsodium not found - encryption disabled")
        message(WARNING "Install: apt-get install libsodium-dev (Ubuntu) or brew install libsodium (macOS)")
    endif()
else()
    set(HAVE_ENCRYPTION OFF)
endif()

# PortAudio
find_package(PkgConfig)
set(PORTAUDIO_FOUND OFF)
if(PkgConfig_FOUND)
    pkg_check_modules(PORTAUDIO portaudio-2.0)
endif()
if(PORTAUDIO_FOUND)
    # pkg-config's resolved paths also work for non-system dependency prefixes.
    set(PORTAUDIO_LIBRARIES ${PORTAUDIO_LINK_LIBRARIES})
endif()

if(NOT PORTAUDIO_FOUND)
    find_library(PORTAUDIO_LIBRARY NAMES portaudio)
    find_path(PORTAUDIO_INCLUDE_DIR NAMES portaudio.h)
    if(PORTAUDIO_LIBRARY AND PORTAUDIO_INCLUDE_DIR)
        set(PORTAUDIO_FOUND ON)
        set(PORTAUDIO_LIBRARIES ${PORTAUDIO_LIBRARY})
        set(PORTAUDIO_INCLUDE_DIRS ${PORTAUDIO_INCLUDE_DIR})
    endif()
endif()

if(PORTAUDIO_FOUND)
    message(STATUS "PortAudio found")
else()
    message(WARNING "PortAudio not found - Real Audio I/O disabled")
    message(WARNING "Install: apt-get install portaudio19-dev (Ubuntu) or brew install portaudio (macOS)")
endif()

set(HAVE_LOCAL_TRANSLATION ${ENABLE_LOCAL_TRANSLATION})
set(HAVE_PORTAUDIO ${PORTAUDIO_FOUND})

# Generate config header
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/include/apm/config.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/include/apm/config.h"
)

# ============================================================================
# APM CORE LIBRARY
# ============================================================================

set(APM_CORE_SOURCES
    src/apm_core.cpp
    src/core/apm_system.cpp
    src/ptt_controller.cpp
    src/call_signaling.cpp
    src/dsp/smartglasses_apm/smartglasses_apm.cpp
)

# Add PortAudio audio device (always added, internal ifdef handles missing PA)
list(APPEND APM_CORE_SOURCES src/io/audio_device.cpp)

# Add local translation engine if enabled
if(ENABLE_LOCAL_TRANSLATION)
    list(APPEND APM_CORE_SOURCES
        src/local_translation_engine.cpp
        src/translation/local_translation_adapter.cpp
    )
    message(STATUS "Local translation enabled (Whisper + NLLB)")
endif()

# Add encryption if enabled
if(HAVE_ENCRYPTION)
    list(APPEND APM_CORE_SOURCES src/crypto.cpp)
    message(STATUS "Encryption module enabled")
endif()

add_library(apm_core STATIC ${APM_CORE_SOURCES})
target_compile_features(apm_core PUBLIC cxx_std_20)
if(ENABLE_LOCAL_TRANSLATION)
    find_package(nlohmann_json 3.11 QUIET)
    if(NOT nlohmann_json_FOUND)
        include(FetchContent)
        set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
        set(JSON_Install OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(nlohmann_json
            URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
            URL_HASH SHA256=d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d)
        FetchContent_MakeAvailable(nlohmann_json)
    endif()
    # JSON types stay behind the translation implementation boundary.
    target_link_libraries(apm_core PRIVATE $<BUILD_INTERFACE:nlohmann_json::nlohmann_json>)
endif()

# Compiler warnings
if(MSVC)
    target_compile_options(apm_core PRIVATE /W4)
else()
    target_compile_options(apm_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

# FIXED: Use generator expressions for proper build/install interface
target_include_directories(apm_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# NOTE:
# Avoid global include directories. If apm_system.h must be reachable,
# prefer moving it under include/apm/ or add a scoped include below.
target_include_directories(apm_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
)

target_link_libraries(apm_core PUBLIC Threads::Threads)

# Optional: link PortAudio if present
if(PORTAUDIO_FOUND)
    target_include_directories(apm_core PUBLIC ${PORTAUDIO_INCLUDE_DIRS})
    target_link_libraries(apm_core PUBLIC ${PORTAUDIO_LIBRARIES})
    target_compile_definitions(apm_core PUBLIC HAVE_PORTAUDIO=1)
endif()

# Optional: link FFTW if present
if(HAVE_FFTW3)
    target_include_directories(apm_core PUBLIC ${FFTW3_INCLUDE_DIR})
    target_link_libraries(apm_core PUBLIC ${FFTW3F_LIBRARY})
    target_compile_definitions(apm_core PUBLIC HAVE_FFTW3=1)
    message(STATUS "FFTW3 found and linked")
endif()

# Optional: link TFLite if present
if(HAVE_TFLITE)
    target_include_directories(apm_core PUBLIC ${TFLITE_INCLUDE_DIR})
    target_link_libraries(apm_core PUBLIC ${TFLITE_LIBRARY})
    target_compile_definitions(apm_core PUBLIC HAVE_TFLITE=1)
    message(STATUS "TensorFlow Lite found and linked")
endif()

# Optional: link libsodium if present
if(HAVE_ENCRYPTION)
    target_include_directories(apm_core PUBLIC ${SODIUM_INCLUDE_DIR})
    target_link_libraries(apm_core PUBLIC ${SODIUM_LIBRARY})
    target_compile_definitions(apm_core PUBLIC HAVE_ENCRYPTION=1)
endif()

# Add local translation flag if enabled
if(ENABLE_LOCAL_TRANSLATION)
    target_compile_definitions(apm_core PUBLIC HAVE_LOCAL_TRANSLATION=1)
endif()

# ============================================================================
# MAIN EXECUTABLE
# ============================================================================

add_executable(apm_backend src/core/main.cpp)
target_link_libraries(apm_backend PRIVATE apm_core)

# ============================================================================
# EXAMPLES
# ============================================================================

if(BUILD_EXAMPLES)
    # Translation example - demonstrates full pipeline with real translation
    if(ENABLE_LOCAL_TRANSLATION AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/examples/translation_example.cpp)
        add_executable(translation_example examples/translation_example.cpp)
        target_link_libraries(translation_example PRIVATE apm_core Threads::Threads)
        message(STATUS "Building translation_example")
    endif()
    
    # Encrypted translation example
    if(HAVE_ENCRYPTION AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/examples/encrypted_translation_example.cpp)
        add_executable(encrypted_translation_example examples/encrypted_translation_example.cpp)
        target_link_libraries(encrypted_translation_example PRIVATE apm_core Threads::Threads)
        message(STATUS "Building encrypted_translation_example")
    endif()
    
    # PTT + Call Signaling example
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/examples/ptt_call_example.cpp)
        add_executable(ptt_call_example examples/ptt_call_example.cpp)
        target_link_libraries(ptt_call_example PRIVATE apm_core Threads::Threads)
        message(STATUS "Building ptt_call_example")
    endif()
    
    # Basic APM example (if you have one)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/examples/basic_example.cpp)
        add_executable(basic_example examples/basic_example.cpp)
        target_link_libraries(basic_example PRIVATE apm_core)
        message(STATUS "Building basic_example")
    endif()
    
    message(STATUS "Examples enabled")
endif()

# ============================================================================
# TESTING
# ============================================================================

if(APM_ENABLE_TESTS)
    include(CTest)
    enable_testing()

    find_package(GTest QUIET)

    if(NOT GTest_FOUND)
        message(STATUS "GTest not found. Fetching googletest...")
        include(FetchContent)
        # GoogleTest is build-only; SDK/TGZ installation must not ship it.
        set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(
            googletest
            URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
        )
        FetchContent_MakeAvailable(googletest)
    endif()

    # Core functionality tests
    add_executable(bedrock_test tests/test_bedrock.cpp)
    target_link_libraries(bedrock_test PRIVATE apm_core GTest::gtest_main)
    add_test(NAME bedrock_test COMMAND bedrock_test)
    set_tests_properties(bedrock_test PROPERTIES TIMEOUT 30)
    if(NOT PORTAUDIO_FOUND)
        add_test(NAME audio_dependency_unavailable COMMAND apm_backend)
        set_tests_properties(audio_dependency_unavailable PROPERTIES
            WILL_FAIL TRUE TIMEOUT 10)
    endif()

    add_executable(apm_test tests/test_apm_core.cpp)
    target_link_libraries(apm_test PRIVATE apm_core GTest::gtest_main)
    add_test(NAME apm_core_test COMMAND apm_test)

    # New V7.0.0 Feature tests
    add_executable(apm_features_test tests/test_apm_features.cpp)
    target_link_libraries(apm_features_test PRIVATE apm_core GTest::gtest_main)
    add_test(NAME apm_features_test COMMAND apm_features_test)

    # Smartglasses APM Projection Engine tests
    add_executable(smartglasses_apm_test tests/test_smartglasses_apm.cpp)
    target_link_libraries(smartglasses_apm_test PRIVATE apm_core GTest::gtest_main)
    add_test(NAME smartglasses_apm_test COMMAND smartglasses_apm_test
        --gtest_filter=-SmartglassesApmTest.RealTimeFrameProcessingLatency)

    # Smartglasses APM Math Projection tests
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_smartglasses_apm_projection.cpp)
        add_executable(smartglasses_apm_projection_test tests/test_smartglasses_apm_projection.cpp)
        target_link_libraries(smartglasses_apm_projection_test PRIVATE apm_core GTest::gtest_main)
        add_test(NAME smartglasses_apm_projection_test COMMAND smartglasses_apm_projection_test)
    endif()

    # Smartglasses APM DSP Pipeline & Robustness tests
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_smartglasses_apm_dsp_pipeline.cpp)
        add_executable(smartglasses_apm_dsp_pipeline_test tests/test_smartglasses_apm_dsp_pipeline.cpp)
        target_link_libraries(smartglasses_apm_dsp_pipeline_test PRIVATE apm_core GTest::gtest_main)
        add_test(NAME smartglasses_apm_dsp_pipeline_test COMMAND smartglasses_apm_dsp_pipeline_test
            --gtest_filter=-SmartglassesApmDspPipelineTest.Sub10msLatencyBenchmark)
    endif()

    if(BUILD_BENCHMARKS)
        target_compile_definitions(smartglasses_apm_test PRIVATE APM_ENABLE_PERFORMANCE_TESTS=1)
        add_test(NAME smartglasses_latency_test COMMAND smartglasses_apm_test
            --gtest_filter=SmartglassesApmTest.RealTimeFrameProcessingLatency)
        set_tests_properties(smartglasses_latency_test PROPERTIES
            LABELS performance RUN_SERIAL TRUE TIMEOUT 30)
        if(TARGET smartglasses_apm_dsp_pipeline_test)
            target_compile_definitions(smartglasses_apm_dsp_pipeline_test PRIVATE APM_ENABLE_PERFORMANCE_TESTS=1)
            add_test(NAME smartglasses_dsp_latency_test COMMAND smartglasses_apm_dsp_pipeline_test
                --gtest_filter=SmartglassesApmDspPipelineTest.Sub10msLatencyBenchmark)
            set_tests_properties(smartglasses_dsp_latency_test PROPERTIES
                LABELS performance RUN_SERIAL TRUE TIMEOUT 30)
        endif()
    endif()

    # Crypto tests
    if(HAVE_ENCRYPTION AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_crypto.cpp)
        add_executable(crypto_test tests/test_crypto.cpp)
        target_link_libraries(crypto_test PRIVATE apm_core GTest::gtest_main)
        add_test(NAME crypto_test COMMAND crypto_test)
    endif()

    # Translation integration tests
    if(ENABLE_LOCAL_TRANSLATION AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_translation_integration.cpp)
        add_executable(test_translation tests/integration/test_translation_integration.cpp)
        target_link_libraries(test_translation PRIVATE apm_core Threads::Threads)
        add_test(NAME translation_integration_test COMMAND test_translation)
        set_tests_properties(translation_integration_test PROPERTIES
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
    endif()

    # PTT + Call Signaling tests
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_ptt_signaling.cpp)
        add_executable(ptt_signaling_test tests/test_ptt_signaling.cpp)
        target_link_libraries(ptt_signaling_test PRIVATE apm_core GTest::gtest_main)
        add_test(NAME ptt_signaling_test COMMAND ptt_signaling_test)
    endif()

    # Release test binaries must retain C/C++ assertions used by the bridge
    # runner; NDEBUG belongs to optimized production targets only.
    foreach(test_target bedrock_test apm_test apm_features_test smartglasses_apm_test
            smartglasses_apm_projection_test smartglasses_apm_dsp_pipeline_test
            crypto_test test_translation ptt_signaling_test)
        if(TARGET ${test_target})
            if(MSVC)
                target_compile_options(${test_target} PRIVATE /UNDEBUG)
            else()
                target_compile_options(${test_target} PRIVATE -UNDEBUG)
            endif()
        endif()
    endforeach()
    message(STATUS "Testing enabled")
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
        message(STATUS "CTest performance checks enabled; no standalone apm_bench target")
    endif()
endif()

# ============================================================================
# INSTALLATION
# ============================================================================

# Install binaries
install(TARGETS apm_backend apm_core
    EXPORT APMTargets
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
    install(FILES docs/licenses/nlohmann-json.txt DESTINATION share/doc/apm/licenses)
    install(FILES 
        scripts/translation_bridge.py
        DESTINATION share/apm/scripts
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE 
                    GROUP_READ GROUP_EXECUTE 
                    WORLD_READ WORLD_EXECUTE
    )
    
    install(FILES 
        scripts/setup.sh
        DESTINATION share/apm/scripts
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE 
                    GROUP_READ GROUP_EXECUTE 
                    WORLD_READ WORLD_EXECUTE
    )
    
    # Install requirements file if it exists
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/config/requirements-translation.txt)
        install(FILES config/requirements-translation.txt
            DESTINATION share/apm/
        )
    endif()
    
    message(STATUS "Translation scripts will be installed")
endif()

# Install documentation
install(FILES 
    README.md
    LICENSE
    CHANGELOG.md
    docs/releases/v11.0.0-bedrock.md
    docs/releases/v11.0.0-bedrock-1.1-pass-b.md
    DESTINATION share/doc/apm
)

# Install additional docs if they exist
foreach(doc BUILD.md ROADMAP.md TRANSLATOR_QUICKSTART.md TRANSLATION_BRIDGE.md ENCRYPTION.md PTT_CALL_SIGNALING.md)
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

# Only create package config if template exists
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/cmake/APMConfig.cmake.in)
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
    
    message(STATUS "Package configuration will be installed")
endif()

# Reuse the install rules for release archives.
set(CPACK_PACKAGE_NAME "apm")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_GENERATOR "TGZ")
include(CPack)

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
message(STATUS "  Testing:             ${APM_ENABLE_TESTS}")
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
message(STATUS "  Encryption:          ${HAVE_ENCRYPTION}")
if(HAVE_ENCRYPTION)
    message(STATUS "    ↳ ChaCha20-Poly1305 + X25519")
endif()
message(STATUS "  PTT & Signaling:     Yes")
message(STATUS "    ↳ Push-to-Talk + UDP Call Signaling")
message(STATUS "  TensorFlow Lite:     ${HAVE_TFLITE}")
if(HAVE_FFTW3)
    message(STATUS "  FFTW3:               Yes")
else()
    message(STATUS "  FFTW3:               No (optional)")
endif()
message(STATUS "")
message(STATUS "──────────────────────────────────────────────────────────")
message(STATUS "Targets to Build:")
message(STATUS "──────────────────────────────────────────────────────────")
message(STATUS "  apm_backend          (main executable)")
message(STATUS "  apm_core             (static library)")
if(BUILD_EXAMPLES AND ENABLE_LOCAL_TRANSLATION AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/examples/translation_example.cpp)
    message(STATUS "  translation_example  (full pipeline demo)")
endif()
if(BUILD_EXAMPLES AND HAVE_ENCRYPTION AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/examples/encrypted_translation_example.cpp)
    message(STATUS "  encrypted_translation_example  (secure communications)")
endif()
if(BUILD_EXAMPLES AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/examples/ptt_call_example.cpp)
    message(STATUS "  ptt_call_example     (PTT & call signaling demo)")
endif()
if(APM_ENABLE_TESTS)
    message(STATUS "  apm_test             (core tests)")
    if(HAVE_ENCRYPTION AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_crypto.cpp)
        message(STATUS "  crypto_test          (encryption tests)")
    endif()
    if(ENABLE_LOCAL_TRANSLATION AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_translation_integration.cpp)
        message(STATUS "  test_translation     (integration tests)")
    endif()
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_ptt_signaling.cpp)
        message(STATUS "  ptt_signaling_test   (PTT & signaling tests)")
    endif()
endif()
message(STATUS "")
if(ENABLE_LOCAL_TRANSLATION OR HAVE_ENCRYPTION)
    message(STATUS "──────────────────────────────────────────────────────────")
    message(STATUS "Next Steps:")
    message(STATUS "──────────────────────────────────────────────────────────")
    message(STATUS "  1. Build: cmake --build . -j$(nproc)")
    if(ENABLE_LOCAL_TRANSLATION)
        message(STATUS "  2. Setup translation: ./scripts/setup_translation.sh")
    endif()
    if(APM_ENABLE_TESTS)
        message(STATUS "  3. Run tests: ctest --output-on-failure")
    endif()
    if(BUILD_EXAMPLES)
        if(HAVE_ENCRYPTION)
            message(STATUS "  4. Try: ./encrypted_translation_example")
        else()
            message(STATUS "  4. Try: ./translation_example")
        endif()
    endif()
    message(STATUS "")
endif()
message(STATUS "════════════════════════════════════════════════════════════")
message(STATUS "")
