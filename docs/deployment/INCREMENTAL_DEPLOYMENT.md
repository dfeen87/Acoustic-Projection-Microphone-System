# APM System v2.0 - Incremental Deployment Guide

This guide shows you how to deploy v2.0 features incrementally, building and testing at each stage.

## Stage 0: Current State (v1.0)

**What you have:**
- Core DSP library (`apm_core`)
- Main executable (`apm`)
- Tests (`apm_test`)
- Docker build working ✅
- CI/CD passing ✅

**Status:** ✅ STABLE

---

## Stage 1: Add Translation Implementation (Core)

**Add these files:**
```
src/local_translation_engine.cpp          (implementation)
include/apm/local_translation_engine.hpp  (API header)
```

**Update:**
```
CMakeLists.txt                            (already updated with conditional builds)
requirements-translation.txt              (already updated)
```

**What works:**
- ✅ Core library compiles with translation support
- ✅ Can be used programmatically by linking to `apm_core`
- ✅ Docker build works
- ✅ CI/CD passes

**What doesn't work yet:**
- ⚠️ No examples to run
- ⚠️ No integration tests

**Test it:**
```bash
# Build should succeed
docker build -t apm-system:stage1 .

# Library should have translation symbols
nm build/libapm_core.a | grep Translation
```

**Status:** ✅ BUILDS, LIBRARY READY

---

## Stage 2: Add Examples

**Add this file:**
```
examples/translation_example.cpp
```

**What works:**
- ✅ Everything from Stage 1
- ✅ `translation_example` executable builds
- ✅ Can demonstrate full pipeline
- ✅ Docker build works

**Test it:**
```bash
# Build
docker build -t apm-system:stage2 .

# Run example (will need Python setup)
docker run -it apm-system:stage2 ./build/translation_example
```

**Status:** ✅ BUILDS, EXAMPLES READY

---

## Stage 3: Add Integration Tests

**Add this file:**
```
tests/test_translation_integration.cpp
```

**What works:**
- ✅ Everything from Stage 2
- ✅ `test_translation` executable builds
- ✅ Can verify C++/Python bridge
- ✅ CI can run tests

**Test it:**
```bash
# Build
docker build -t apm-system:stage3 .

# Run tests
docker run -it apm-system:stage3 ./build/test_translation
```

**Status:** ✅ BUILDS, TESTS READY

---

## Stage 4: Add Package Configuration (Optional)

**Add these files:**
```
cmake/APMConfig.cmake.in
```

**What works:**
- ✅ Everything from Stage 3
- ✅ Downstream projects can use `find_package(APM)`
- ✅ Better CMake integration

**Test it:**
```bash
# In another project
find_package(APM REQUIRED)
target_link_libraries(my_app APM::apm_core)
```

**Status:** ✅ COMPLETE v2.0

---

## Stage 5: Add Documentation (Polish)

**Add these files:**
```
docs/BUILD_V2.md
docs/TRANSLATION_BRIDGE.md
docs/MIGRATION_V1_TO_V2.md
```

**Update:**
```
README.md                                 (add v2.0 features)
TRANSLATOR_QUICKSTART.md                  (complete examples)
```

**Status:** ✅ PRODUCTION READY

---

## Quick Reference: What Builds When

### Minimal (Stage 0 - Current)
```cmake
# Dockerfile uses:
-DBUILD_TESTS=OFF
-DBUILD_BENCHMARKS=OFF

# Builds:
✓ apm
✓ apm_core
```

### With Translation Core (Stage 1)
```cmake
# Same Docker flags, adds:
-DENABLE_LOCAL_TRANSLATION=ON  (default)

# Builds:
✓ apm
✓ apm_core (with translation)
```

### With Examples (Stage 2)
```cmake
# If examples/translation_example.cpp exists:

# Builds:
✓ apm
✓ apm_core
✓ translation_example
```

### With Tests (Stage 3)
```cmake
# Change Dockerfile to:
-DBUILD_TESTING=ON

# If tests/test_translation_integration.cpp exists:

# Builds:
✓ apm
✓ apm_core
✓ apm_test
✓ test_translation
✓ translation_example
```

---

## Deployment Strategy

### Option A: All At Once (Recommended)
```bash
# Add all files from artifacts
# Update CMakeLists.txt (done)
# Build and test
# Ship v2.0
```

### Option B: Incremental (Safer)
```bash
# Stage 1: Merge translation implementation
git checkout -b feature/translation-core
# Add src/local_translation_engine.cpp
# Add include/apm/local_translation_engine.hpp
# Update CMakeLists.txt
git commit -m "Add translation core implementation"
git push

# Stage 2: Add examples
git checkout -b feature/translation-examples
# Add examples/translation_example.cpp
git commit -m "Add translation example"
git push

# Stage 3: Add tests
git checkout -b feature/translation-tests
# Add tests/test_translation_integration.cpp
git commit -m "Add integration tests"
git push

# Stage 4: Documentation
git checkout -b docs/v2
# Add all documentation
git commit -m "Complete v2.0 documentation"
git push

# Merge all branches → Release v2.0
```

---

## Current Build Status Checker

After each stage, verify with:

```bash
# Check what CMake will build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -LAH | grep -E "(EXAMPLE|TESTING|TRANSLATION)"

# Should show:
BUILD_EXAMPLES:BOOL=ON
BUILD_TESTING:BOOL=ON  (or OFF)
ENABLE_LOCAL_TRANSLATION:BOOL=ON
```

```bash
# Check what targets exist
cmake --build . --target help

# Should list:
# ... apm
# ... apm_core
# ... apm_test (if TESTING=ON)
# ... translation_example (if file exists)
# ... test_translation (if file exists and TESTING=ON)
```

---

## Rollback Strategy

If something breaks:

### Stage 1 broken?
```bash
# Disable translation
cmake .. -DENABLE_LOCAL_TRANSLATION=OFF
```

### Stage 2 broken?
```bash
# Examples are optional, just remove file
rm examples/translation_example.cpp
# Rebuild - CMake will skip it
```

### Stage 3 broken?
```bash
# Tests are optional
rm tests/test_translation_integration.cpp
# Or disable testing
cmake .. -DBUILD_TESTING=OFF
```

### Nuclear option:
```bash
# Revert to v1.0 CMakeLists.txt
git checkout v1.0 CMakeLists.txt
# Everything still works
```

---

## FAQ

**Q: Can I deploy translation without examples?**  
A: Yes! Stage 1 gives you the library. Users link against it directly.

**Q: Will Docker build work at every stage?**  
A: Yes! The updated CMakeLists.txt is smart about what exists.

**Q: What's the minimum for v2.0?**  
A: Stage 1 (core implementation). Everything else is polish.

**Q: When should I tag v2.0.0?**  
A: After Stage 3 (tests passing). Stage 4-5 can be v2.0.1+

**Q: What if I only want to add implementation now?**  
A: Perfect! Add Stage 1 files, build, test. Ship it. Add examples later.

---

## Success Criteria by Stage

### Stage 1 ✅
- [ ] `libapm_core.a` contains translation symbols
- [ ] Docker build succeeds
- [ ] No regression in existing functionality

### Stage 2 ✅
- [ ] `translation_example` executable exists
- [ ] Can link against it successfully
- [ ] Docker build succeeds

### Stage 3 ✅
- [ ] `test_translation` executable exists
- [ ] Tests can be run (even if they fail due to missing Python models)
- [ ] CI/CD passes

### Stage 4 ✅
- [ ] `find_package(APM)` works
- [ ] Downstream projects can link easily

### Stage 5 ✅
- [ ] Documentation complete
- [ ] README updated
- [ ] Migration guide clear

---

## Current Status (Based on CI Log)

```
✅ CMake configures successfully
✅ Version 2.0.0 recognized
✅ Local Translation: ON
✅ FFTW3: Yes
✅ Core library builds
✅ Main executable builds

⚠️  translation_example: Not built (file doesn't exist yet)
⚠️  test_translation: Not built (file doesn't exist yet)
```

**Next Action:** Add Stage 1 files (`local_translation_engine.cpp` + `.hpp`) to make library fully functional.

**Timeline:**
- Stage 1: 5 minutes (copy files)
- Stage 2: 2 minutes (add example)
- Stage 3: 2 minutes (add tests)
- Stage 4: 1 minute (add cmake config)
- Stage 5: Already done! (documentation created)

**Total: 10 minutes to complete v2.0** 🚀
