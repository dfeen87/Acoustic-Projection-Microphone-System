# APM System Build Instructions
 
Production-grade build system for the Acoustic Projection Microphone system.

## Prerequisites

### Required Dependencies

```bash
# Ubuntu/Debian
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    git \
    libfftw3-dev \
    libfftw3-single3

# macOS (Homebrew)
brew install cmake fftw

# Windows (vcpkg)
vcpkg install fftw3:x64-windows
```

### Optional Dependencies

#### TensorFlow Lite (for real translation)

```bash
# Ubuntu/Debian
# Build from source (recommended)
git clone https://github.com/tensorflow/tensorflow.git
cd tensorflow
bazel build -c opt //tensorflow/lite:libtensorflowlite.so

# Or use pre-built packages
pip install tensorflow
# Then copy libtensorflowlite.so from Python packages

# macOS
brew install tensorflow-lite

# Windows
# Download pre-built from TensorFlow releases
```

## Project Structure

```
apm/
├── CMakeLists.txt              # Main build configuration
├── include/
│   └── apm/
│       ├── config.h.in         # Configuration template
│       ├── audio_frame.h
│       ├── beamforming_engine.h
│       ├── noise_suppression_engine.h
│       ├── echo_cancellation_engine.h
│       ├── voice_activity_detector.h
│       ├── directional_projector.h
│       ├── translation_engine.h
│       ├── tflite_translation_engine.h
│       ├── fft_processor.h
│       └── apm_system.h
├── src/
│   ├── audio_frame.cpp
│   ├── beamforming_engine.cpp
│   ├── noise_suppression_engine.cpp
│   ├── echo_cancellation_engine.cpp
│   ├── voice_activity_detector.cpp
│   ├── directional_projector.cpp
│   ├── apm_system.cpp
│   ├── fft_processor.cpp
│   ├── tflite_translation_engine.cpp
│   └── mock_translation_engine.cpp
├── tests/
│   ├── test_audio_frame.cpp
│   ├── test_beamforming.cpp
│   ├── test_noise_suppression.cpp
│   ├── test_echo_cancellation.cpp
│   ├── test_vad.cpp
│   ├── test_fft.cpp
│   └── test_integration.cpp
├── examples/
│   └── main.cpp
└── benchmarks/
    ├── bench_beamforming.cpp
    └── bench_noise_suppression.cpp
```

## Building

### Basic Build

```bash
# Clone the repository
git clone <your-repo-url>
cd apm

# Create build directory
mkdir build && cd build

# Configure
cmake ..

# Build
cmake --build . -j$(nproc)

# Run tests
ctest --output-on-failure
```

### Build with TensorFlow Lite

```bash
mkdir build && cd build

cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DTFLITE_INCLUDE_DIR=/path/to/tensorflow/lite \
    -DTFLITE_LIBRARY=/path/to/libtensorflowlite.so

cmake --build . -j$(nproc)
```

### Build Options

```bash
# Enable all options
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTS=ON \
    -DBUILD_BENCHMARKS=ON \
    -DENABLE_SANITIZERS=OFF \
    -DENABLE_COVERAGE=OFF

# Debug build with sanitizers
cmake .. \
    -DCMAKE_BUILD_TYPE=Debug \
    -DENABLE_SANITIZERS=ON

# Release build with optimizations
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_FLAGS="-O3 -march=native"
```

### Build Configurations

| Option | Default | Description |
|--------|---------|-------------|
| `BUILD_TESTS` | ON | Build unit tests |
| `BUILD_BENCHMARKS` | OFF | Build performance benchmarks |
| `ENABLE_SANITIZERS` | OFF | Enable AddressSanitizer and UBSan |
| `ENABLE_COVERAGE` | OFF | Enable code coverage instrumentation |

## Running Tests

```bash
# Run all tests
cd build
ctest

# Run specific test
./apm_tests --gtest_filter=AudioFrameTest.*

# Run with verbose output
ctest -V

# Run tests in parallel
ctest -j$(nproc)
```

## Running Benchmarks

```bash
# Build with benchmarks enabled
cmake .. -DBUILD_BENCHMARKS=ON
cmake --build .

# Run benchmarks
./apm_bench
```

## Installation

```bash
# Install to system
cd build
sudo cmake --install .

# Install to custom prefix
cmake .. -DCMAKE_INSTALL_PREFIX=/opt/apm
cmake --build .
cmake --install .
```

## Using the Library

### CMake Integration

```cmake
find_package(apm REQUIRED)

add_executable(myapp main.cpp)
target_link_libraries(myapp PRIVATE apm::apm)
```

### Manual Linking

```bash
g++ -std=c++20 main.cpp \
    -I/usr/local/include \
    -L/usr/local/lib \
    -lapm -lfftw3f -pthread \
    -o myapp
```

## Example Usage

```cpp
#include <apm/apm_system.h>

int main() {
    // Configure system
    apm::APMSystem::Config config;
    config.num_microphones = 4;
    config.num_speakers = 3;
    config.source_language = "en-US";
    config.target_language = "es-ES";
    
    apm::APMSystem apm(config);
    
    // Create audio input (20ms frames)
    const int frame_size = 960; // 20ms at 48kHz
    std::vector<apm::AudioFrame> mic_array;
    
    for (int i = 0; i < 4; ++i) {
        mic_array.emplace_back(frame_size, 48000, 1);
        // Fill with audio data...
    }
    
    apm::AudioFrame speaker_ref(frame_size, 48000, 1);
    
    // Process
    float target_angle = 30.0f * M_PI / 180.0f;
    auto output = apm.process(mic_array, speaker_ref, target_angle);
    
    // Use output signals
    for (size_t i = 0; i < output.size(); ++i) {
        // Send output[i] to speaker i
    }
    
    return 0;
}
```

## Performance Tuning

### Compiler Optimizations

```bash
# Maximum optimization
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_FLAGS="-O3 -march=native -flto"

# With profiling
cmake .. \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_CXX_FLAGS="-O2 -g"
```

### FFTW Optimization

```bash
# Use FFTW with AVX2
cmake .. -DFFTW3F_LIBRARY=/usr/local/lib/libfftw3f_avx2.a

# Generate FFTW wisdom for your system
fftwf-wisdom -v -c -n -o wisdom.fftw
export FFTW_WISDOM_FILE=wisdom.fftw
```

### Threading

The system uses C++20 async tasks. Control thread count with:

```cpp
// In your code
#include <thread>
std::cout << "Hardware threads: " 
          << std::thread::hardware_concurrency() << std::endl;
```

## Troubleshooting

### FFTW Not Found

```bash
# Verify installation
pkg-config --modversion fftw3f

# Manual specification
cmake .. \
    -DFFTW3F_LIBRARY=/usr/lib/x86_64-linux-gnu/libfftw3f.so \
    -DFFTW3_INCLUDE_DIR=/usr/include
```

### TensorFlow Lite Not Found

```bash
# Check library
ldconfig -p | grep tensorflow

# Specify manually
cmake .. \
    -DTFLITE_LIBRARY=/path/to/libtensorflowlite.so \
    -DTFLITE_INCLUDE_DIR=/path/to/tensorflow
```

### Tests Failing

```bash
# Run with detailed output
ctest -V

# Run specific test with gtest flags
./apm_tests --gtest_filter=BeamformingTest.* --gtest_output=xml
```

## Code Coverage

```bash
# Build with coverage
cmake .. -DENABLE_COVERAGE=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build .

# Run tests
ctest

# Generate report (lcov)
lcov --capture --directory . --output-file coverage.info
lcov --remove coverage.info '/usr/*' --output-file coverage.info
genhtml coverage.info --output-directory coverage_html

# View report
firefox coverage_html/index.html
```

## Continuous Integration

Example GitHub Actions workflow:

```yaml
name: CI

on: [push, pull_request]

jobs:
  build-test:
    runs-on: ubuntu-latest
    
    steps:
    - uses: actions/checkout@v3
    
    - name: Install dependencies
      run: |
        sudo apt-get update
        sudo apt-get install -y libfftw3-dev
    
    - name: Configure
      run: cmake -B build -DCMAKE_BUILD_TYPE=Release
    
    - name: Build
      run: cmake --build build -j$(nproc)
    
    - name: Test
      run: cd build && ctest --output-on-failure
```

## License

MIT License - See original source file for full license text.

## Support

For issues and questions:
- GitHub Discussion Board
- Email: dfeen87@gmail.com

## Credits 

- Original Author: Don Michael Feeney Jr.
- Dedicated to: Marcel Krüger
- Enhanced with: Claude (Anthropic)
