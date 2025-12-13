# APM System v2.0 - Build Instructions

Complete guide for building APM System with local translation support.

## Prerequisites

### System Requirements

**Minimum:**
- C++20 compatible compiler (GCC 10+, Clang 11+, MSVC 2019+)
- CMake 3.18+
- Python 3.8+
- 8GB RAM
- 5GB disk space

**Recommended:**
- GCC 11+ or Clang 13+
- CMake 3.20+
- Python 3.10+
- 16GB RAM
- NVIDIA GPU with CUDA support
- 10GB disk space

### Ubuntu/Debian

```bash
# Build essentials
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    git \
    python3 \
    python3-pip \
    python3-venv

# Optional: FFTW for FFT acceleration
sudo apt-get install -y libfftw3-dev

# Optional: CUDA for GPU acceleration
# Follow: https://developer.nvidia.com/cuda-downloads
```

### macOS

```bash
# Install Xcode Command Line Tools
xcode-select --install

# Install Homebrew (if not already installed)
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Install dependencies
brew install cmake python@3.11 fftw
```

### Windows

```powershell
# Install Visual Studio 2022 with C++ support
# Install CMake: https://cmake.org/download/
# Install Python: https://www.python.org/downloads/

# Or use MSYS2:
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake python3
```

## Quick Start (5 Minutes)

```bash
# 1. Clone repository
git clone https://github.com/yourusername/apm-system.git
cd apm-system

# 2. Setup translation models
chmod +x scripts/setup_translation.sh
./scripts/setup_translation.sh

# 3. Build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)

# 4. Test
./test_translation

# 5. Run example
./translation_example
```

## Detailed Build Steps

### Step 1: Clone Repository

```bash
git clone https://github.com/yourusername/apm-system.git
cd apm-system
```

### Step 2: Setup Translation Environment

This installs Whisper and NLLB models (~2GB download):

```bash
./scripts/setup_translation.sh
```

What this does:
- Creates Python virtual environment in `venv/`
- Installs OpenAI Whisper (speech recognition)
- Installs Meta NLLB (translation, 200+ languages)
- Downloads model weights
- Configures GPU if available

**Manual setup (if script fails):**

```bash
# Create virtual environment
python3 -m venv venv
source venv/bin/activate

# Install dependencies
pip install -r requirements-translation.txt

# Test installation
python3 -c "import whisper; import transformers; print('OK')"
```

### Step 3: Configure Build

```bash
mkdir build && cd build
cmake .. [OPTIONS]
```

**Build Options:**

| Option | Default | Description |
|--------|---------|-------------|
| `CMAKE_BUILD_TYPE` | `Release` | `Debug`, `Release`, `RelWithDebInfo` |
| `BUILD_TESTING` | `ON` | Build test suite |
| `BUILD_EXAMPLES` | `ON` | Build example programs |
| `BUILD_BENCHMARKS` | `OFF` | Build performance benchmarks |
| `ENABLE_LOCAL_TRANSLATION` | `ON` | Enable Whisper+NLLB translation |
| `ENABLE_TFLITE` | `OFF` | Enable TensorFlow Lite (optional) |
| `ENABLE_COVERAGE` | `OFF` | Enable code coverage reporting |

**Example configurations:**

```bash
# Debug build with tests
cmake .. -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON

# Release build without translation
cmake .. -DCMAKE_BUILD_TYPE=Release -DENABLE_LOCAL_TRANSLATION=OFF

# Build with coverage for development
cmake .. -DCMAKE_BUILD_TYPE=Debug -DENABLE_COVERAGE=ON

# Minimal build (library only)
cmake .. -DBUILD_TESTING=OFF -DBUILD_EXAMPLES=OFF
```

### Step 4: Build

```bash
# Build with all CPU cores
cmake --build . -j$(nproc)

# Or manually specify core count
cmake --build . -j4

# Build specific target
cmake --build . --target translation_example

# Verbose build (see compiler commands)
cmake --build . --verbose
```

**Build targets:**

- `apm` - Main executable
- `apm_core` - Static library
- `translation_example` - Full pipeline demo
- `test_translation` - Integration tests
- `apm_test` - Core functionality tests

### Step 5: Test

```bash
# Run all tests
ctest --output-on-failure

# Or run individually
./apm_test                    # Core tests
./test_translation            # Translation tests

# Run with verbose output
./test_translation --verbose
```

### Step 6: Install (Optional)

```bash
# Install to system (requires sudo)
sudo cmake --install .

# Or install to custom location
cmake --install . --prefix=/opt/apm

# Verify installation
which apm
apm --version
```

## Usage After Build

### Run Translation Example

```bash
# From build directory
./translation_example
```

This demonstrates:
- 4-channel microphone array simulation
- Beamforming at 30° angle
- Echo cancellation
- Noise suppression
- Voice activity detection
- **Real translation (EN → ES)**
- Directional audio projection

### Use as Library

```cpp
#include <apm/local_translation_engine.hpp>

int main() {
    // Configure
    apm::LocalTranslationEngine::Config config;
    config.source_language = "en";
    config.target_language = "es";
    
    // Create engine
    apm::LocalTranslationEngine engine(config);
    
    // Translate
    std::vector<float> audio = load_audio("recording.wav");
    auto result = engine.translate(audio, 16000);
    
    if (result.success) {
        std::cout << "Original: " << result.transcribed_text << "\n";
        std::cout << "Translated: " << result.translated_text << "\n";
    }
    
    return 0;
}
```

Compile your program:

```bash
g++ -std=c++20 my_program.cpp -I/usr/local/include -L/usr/local/lib -lapm_core -pthread
```

## Troubleshooting

### Build Errors

**"CMake 3.18 or higher is required"**
```bash
# Ubuntu: Add Kitware repository for latest CMake
wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc | sudo apt-key add -
sudo apt-add-repository 'deb https://apt.kitware.com/ubuntu/ focal main'
sudo apt-get update
sudo apt-get install cmake
```

**"C++20 not supported"**
```bash
# Update compiler
sudo apt-get install g++-11
export CXX=g++-11
cmake .. -DCMAKE_CXX_COMPILER=g++-11
```

**"fftw3.h not found" (optional)**
```bash
sudo apt-get install libfftw3-dev
```

### Translation Setup Errors

**"Whisper not installed"**
```bash
source venv/bin/activate
pip install openai-whisper
```

**"CUDA out of memory"**
```bash
# Use smaller Whisper model
# Edit scripts/translation_bridge.py:
# whisper_model="tiny"  # instead of "small"
```

**"Translation timeout"**
```bash
# First run is slow (loads models)
# Subsequent runs are faster (models cached in RAM)
# CPU translation: 5-8 seconds per sentence (expected)
# GPU translation: 2-4 seconds per sentence (expected)
```

### Runtime Errors

**"Python script not found"**
```bash
# Ensure you're running from project root, not build/
cd /path/to/apm-system
./build/translation_example
```

**"Translation failed"**
```bash
# Verify Python setup
source venv/bin/activate
python3 scripts/translation_bridge.py --help

# Test manually
python3 scripts/translation_bridge.py test_audio.wav --source en --target es
```

## Performance Optimization

### CPU Optimization

```bash
# Build with native CPU optimizations
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS="-march=native -O3"

# Use smaller Whisper model for speed
# Edit config or use "tiny" model (fastest)
```

### GPU Optimization

```bash
# Verify CUDA is available
nvidia-smi
python3 -c "import torch; print(torch.cuda.is_available())"

# Install PyTorch with CUDA
pip install torch torchvision torchaudio --index-url https://download.pytorch.org/whl/cu118

# Enable GPU in config
config.use_gpu = true;
```

### Memory Optimization

```bash
# Use smaller models
config.whisper_model_path = "tiny";  # 39MB vs 244MB

# Or distilled NLLB
config.nllb_model_path = "facebook/nllb-200-distilled-600M";  # 1.2GB vs 3.3GB
```

## Development Workflow

### Building for Development

```bash
# Debug build with assertions
cmake .. -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON

# Build and run tests after changes
cmake --build . -j$(nproc) && ctest --output-on-failure
```

### Code Coverage

```bash
# Configure with coverage
cmake .. -DCMAKE_BUILD_TYPE=Debug -DENABLE_COVERAGE=ON

# Build and run tests
cmake --build . -j$(nproc)
ctest

# Generate coverage report
lcov --capture --directory . --output-file coverage.info
lcov --remove coverage.info '/usr/*' --output-file coverage.info
genhtml coverage.info --output-directory coverage_html
```

### Adding New Features

1. Add source files to `src/`
2. Add headers to `include/apm/`
3. Update `CMakeLists.txt`:
   ```cmake
   set(APM_CORE_SOURCES
       src/apm_core.cpp
       src/local_translation_engine.cpp
       src/your_new_file.cpp  # Add here
   )
   ```
4. Rebuild: `cmake --build . -j$(nproc)`
5. Test: Add tests in `tests/`

## Docker Build (Alternative)

```bash
# Build Docker image
docker build -t apm-system .

# Run in container
docker run -it --rm apm-system ./translation_example

# Development environment
docker run -it --rm -v $(pwd):/workspace/apm apm-system bash
```

## CI/CD Integration

### GitHub Actions Example

```yaml
name: Build and Test

on: [push, pull_request]

jobs:
  build:
    runs-on: ubuntu-latest
    steps:
    - uses: actions/checkout@v3
    
    - name: Install dependencies
      run: |
        sudo apt-get update
        sudo apt-get install -y build-essential cmake libfftw3-dev
    
    - name: Setup translation
      run: ./scripts/setup_translation.sh
    
    - name: Build
      run: |
        mkdir build && cd build
        cmake .. -DCMAKE_BUILD_TYPE=Release
        cmake --build . -j$(nproc)
    
    - name: Test
      run: |
        cd build
        ctest --output-on-failure
```

## Next Steps

- ✅ Built successfully? Try `./translation_example`
- 📖 Read [TRANSLATOR_QUICKSTART.md](TRANSLATOR_QUICKSTART.md) for usage
- 🐛 Issues? Open a GitHub issue

## Getting Help

- 📧 Email: dfeen87@gmail.com
- 💬 GitHub Discussions: https://github.com/dfeen87/acoustic-projection-microphone-system/discussions
- 🐛 Bug Reports: https://github.com/dfeen87/acoustic-projection-microphone-system/issues

---

**Built with ❤️ by Don Michael Feeney Jr.**  
**Dedicated to Marcel Krüger**  
**Enhanced with Claude (Anthropic)**
