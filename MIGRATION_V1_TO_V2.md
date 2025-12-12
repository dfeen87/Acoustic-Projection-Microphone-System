# Migrating from v1.0 to v2.0

## What's New in v2.0

### 🎉 Major Features

1. **Real Translation Integration** - C++ now calls Python Whisper+NLLB directly
2. **Complete Examples** - Full end-to-end demos with working translation
3. **Integration Tests** - Verify C++/Python bridge functionality
4. **Enhanced Build System** - Better CMake configuration with package support
5. **Comprehensive Documentation** - Complete guides for all features

### 🔧 Breaking Changes

**None!** v2.0 is fully backward compatible. Your existing v1.0 code will work without modifications.

## Quick Upgrade

```bash
# 1. Pull latest changes
git pull origin main

# 2. Setup translation (new feature)
./scripts/setup_translation.sh

# 3. Rebuild
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)

# 4. Test new features
./test_translation
./translation_example
```

That's it! You're on v2.0.

## Detailed Changes

### New Files Added

```
src/
  └─ local_translation_engine.cpp          ← NEW: Translation bridge implementation

include/apm/
  └─ local_translation_engine.hpp          ← NEW: Translation API

examples/
  └─ translation_example.cpp               ← NEW: Full pipeline demo

tests/
  └─ test_translation_integration.cpp      ← NEW: Integration tests

scripts/
  └─ translation_bridge.py                 ← UPDATED: Enhanced with better error handling

docs/
  ├─ TRANSLATION_BRIDGE.md                 ← NEW: Implementation guide
  ├─ BUILD_V2.md                           ← NEW: Complete build guide
  └─ MIGRATION_V1_TO_V2.md                 ← NEW: This file

requirements-translation.txt                ← NEW: Python dependencies
```

### Modified Files

```
CMakeLists.txt                             ← ENHANCED: Better configuration
README.md                                  ← UPDATED: v2.0 features
TRANSLATOR_QUICKSTART.md                   ← UPDATED: Complete examples
```

### Build System Changes

**v1.0:**
```cmake
# Basic library
add_library(apm_core STATIC src/apm_core.cpp)
```

**v2.0:**
```cmake
# Library with optional translation
set(APM_CORE_SOURCES src/apm_core.cpp)
if(ENABLE_LOCAL_TRANSLATION)
    list(APPEND APM_CORE_SOURCES src/local_translation_engine.cpp)
endif()
add_library(apm_core STATIC ${APM_CORE_SOURCES})

# New: Examples and integration tests
add_executable(translation_example examples/translation_example.cpp)
add_executable(test_translation tests/test_translation_integration.cpp)
```

## Using New Features

### Translation in Your Code

**Before (v1.0):**
```cpp
// Had to manually call Python
system("python3 scripts/translation_bridge.py audio.wav");
// Then parse output yourself
```

**After (v2.0):**
```cpp
#include "apm/local_translation_engine.hpp"

// Simple C++ API
apm::LocalTranslationEngine::Config config;
config.source_language = "en";
config.target_language = "es";

apm::LocalTranslationEngine engine(config);

std::vector<float> audio = load_audio();
auto result = engine.translate(audio, 16000);

if (result.success) {
    std::cout << "Original: " << result.transcribed_text << "\n";
    std::cout << "Translated: " << result.translated_text << "\n";
}
```

### Async Translation (New in v2.0)

```cpp
// Non-blocking translation
auto future = engine.translate_async(audio, 16000);

// Do other work while translation happens
process_other_audio_frames();

// Get result when ready
auto result = future.get();
```

### Full Pipeline Integration (New in v2.0)

```cpp
// Complete APM + Translation pipeline
APMSystem::Config apm_config;
apm_config.num_microphones = 4;

LocalTranslationEngine::Config trans_config;
trans_config.source_language = "en";
trans_config.target_language = "ja";

APMSystem apm(apm_config);
LocalTranslationEngine translator(trans_config);

// Process
auto cleaned = apm.process(mic_array, speaker_ref, angle);
auto translated = translator.translate(cleaned, 48000);
auto projected = project_to_speakers(translated);
```

## Configuration Changes

### Translation Config (New)

```cpp
struct Config {
    std::string source_language{"en"};           // ISO 639-1 code
    std::string target_language{"es"};           // ISO 639-1 code
    std::string script_path{"scripts/translation_bridge.py"};
    std::string whisper_model_path{"small"};     // tiny, base, small, medium, large
    std::string nllb_model_path{"facebook/nllb-200-distilled-600M"};
    bool use_gpu{true};                          // Use CUDA if available
};
```

### CMake Options (New)

```bash
# Enable/disable translation at build time
cmake .. -DENABLE_LOCAL_TRANSLATION=ON   # Default: ON

# Build examples and tests
cmake .. -DBUILD_EXAMPLES=ON             # Default: ON
cmake .. -DBUILD_TESTING=ON              # Default: ON

# Enable code coverage
cmake .. -DENABLE_COVERAGE=ON            # Default: OFF
```

## Performance Comparison

### v1.0
- DSP Pipeline: 4.2ms per 20ms frame (4.8x real-time)
- Translation: Manual Python call (user responsibility)

### v2.0
- DSP Pipeline: 4.2ms per 20ms frame (4.8x real-time) ← **Same**
- Translation: 2-4 seconds (GPU) or 5-8 seconds (CPU) ← **NEW**
- Async API: Non-blocking translation ← **NEW**

## Backward Compatibility

### Your v1.0 Code Still Works

```cpp
// This v1.0 code works unchanged in v2.0
APMSystem::Config config;
config.num_microphones = 4;
APMSystem apm(config);

auto output = apm.process(mic_array, speaker_ref, angle);
```

### Optional Translation

Translation is **opt-in**:

```bash
# Build WITHOUT translation support (v1.0 behavior)
cmake .. -DENABLE_LOCAL_TRANSLATION=OFF

# Your code works exactly as before
```

## Testing Your Upgrade

### 1. Verify Build

```bash
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)

# Should see:
# ✓ Building apm_core
# ✓ Building translation_example (new)
# ✓ Building test_translation (new)
```

### 2. Run Tests

```bash
# Core tests (should pass as before)
./apm_test

# New translation tests
./test_translation
```

### 3. Try Examples

```bash
# New: Full pipeline with translation
./translation_example

# Output should show:
# ✓ APM System initialized
# ✓ Translation engine initialized
# ✓ Processing audio stream...
# ✓ Translation completed
```

## Troubleshooting Upgrade Issues

### "local_translation_engine.hpp not found"

```bash
# Make sure you pulled all new files
git pull origin main

# Verify file exists
ls include/apm/local_translation_engine.hpp
```

### "Translation not working"

```bash
# Setup translation environment
./scripts/setup_translation.sh

# Verify Python dependencies
source venv/bin/activate
python3 -c "import whisper, transformers; print('OK')"
```

### "Build errors after upgrade"

```bash
# Clean rebuild
rm -rf build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
```

### "My v1.0 code broke"

This shouldn't happen! If it does:

1. File a bug report with your code
2. Temporary workaround:
   ```bash
   # Build without translation
   cmake .. -DENABLE_LOCAL_TRANSLATION=OFF
   ```

## Deprecations

### None!

All v1.0 APIs are maintained. Nothing is deprecated.

## What's Coming in v2.5

From the roadmap:

- **Persistent Python process** (10x faster translation startup)
- **pybind11 integration** (zero-copy audio transfer)
- **Streaming translation** (lower latency)
- **Cloud API fallback** (Google/Azure backup)
- **TTS integration** (text-to-speech output)

## Getting Help

Having issues with the upgrade?

1. **Check documentation:**
   - [BUILD_V2.md](BUILD_V2.md) - Build instructions
   - [TRANSLATION_BRIDGE.md](TRANSLATION_BRIDGE.md) - Technical details
   - [TRANSLATOR_QUICKSTART.md](TRANSLATOR_QUICKSTART.md) - Usage guide

2. **Run diagnostics:**
   ```bash
   ./scripts/setup_translation.sh --check  # Verify setup
   ./test_translation --verbose            # Test translation
   ```

3. **Get support:**
   - GitHub Issues: https://github.com/feen87/acoustic-projection-microphone-system/issues
   - Discussions: https://github.com/dfeen87/acoustic-projection-microphone-system/discussions
   - Email: dfeen87@gmail.com

## Feedback Welcome

Found a bug? Have a feature request? Want to contribute?

- 🐛 Bug reports: File an issue
- 💡 Feature requests: Start a discussion
- 🔧 Pull requests: Always welcome!

---

**Enjoy v2.0!** 🎉

The APM System is now a complete, production-ready platform for multilingual spatial audio communication.

**- Don Michael Feeney Jr.**
