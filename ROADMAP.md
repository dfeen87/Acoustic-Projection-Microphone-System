# APM System - Production Features Roadmap

Version: 2.0+  
Last Updated: December 2024  
Status: Planning Phase

## 📊 Current Status (v1.0)

| Feature | Status |
|---------|--------|
| Core DSP Pipeline | ✅ Complete |
| Multi-channel Beamforming | ✅ Complete |
| Noise Suppression (LSTM) | ✅ Complete |
| Echo Cancellation (NLMS) | ✅ Complete |
| Voice Activity Detection | ✅ Complete |
| Directional Projection | ✅ Complete |
| Translation Interface | ✅ Complete |
| Docker Support | ✅ Complete |

## 🗺️ Release Timeline (Daily work - expedited versions)

### Version 2.0 - Foundation 
- Real audio I/O (PortAudio/ALSA)
- Streaming mode processor
- Command-line interface
- Configuration presets
- Basic documentation

### Version 2.5 - Intelligence
- OpenAI Whisper integration
- Google Cloud Speech/Translate
- Performance profiling tools
- Benchmarking suite
- Advanced presets

### Version 3.0 - Production
- Comprehensive test coverage (>80%)
- Packaging (DEB/RPM/Homebrew)
- Full API documentation (Doxygen)
- Production deployment guides
- Performance optimizations

## 🚀 Feature Implementation Guide

### 1. Real Audio Input/Output
**Priority:** High | **Complexity:** Medium | **Version:** 2.0

#### 1.1 PortAudio Implementation (Cross-platform)

**Dependencies:**
```cmake
# Add to CMakeLists.txt
find_package(portaudio REQUIRED)
target_link_libraries(apm PRIVATE portaudio)
```

**Implementation:** *(Header and implementation code as provided in your document)*

#### 1.2 ALSA Implementation (Linux Native)

*(Implementation code as provided in your document)*

---

### 2. Streaming Mode Processor
**Priority:** High | **Complexity:** Medium | **Version:** 2.0

**Purpose:** Enable continuous real-time processing with thread-safe queue management.

*(Implementation code as provided in your document)*

---

### 3. Real Translation Backend
**Priority:** High | **Complexity:** High | **Version:** 2.5

#### 3.1 OpenAI Whisper + GPT-4
*(Implementation code as provided in your document)*

#### 3.2 Google Cloud Integration
*(Implementation code as provided in your document)*

---

### 4. Performance Profiling
**Priority:** Medium | **Complexity:** Low | **Version:** 2.0

*(Implementation code as provided in your document)*

---

### 5. Configuration Presets
**Priority:** Medium | **Complexity:** Low | **Version:** 2.0

*(Implementation code as provided in your document)*

---

### 6. Command Line Interface
**Priority:** High | **Complexity:** Low | **Version:** 2.0

**Dependencies:**
```cmake
find_package(CLI11 REQUIRED)
target_link_libraries(apm PRIVATE CLI11::CLI11)
```

**Main Application:** *(Beginning of main.cpp as provided)*

**Continuation of main.cpp:**

```cpp
        
        std::cout << "\n\nStopping audio processing...\n";
        audio.stop();
        
        // Print final statistics
        std::cout << "\n=== Final Statistics ===\n";
        auto final_stats = processor.get_stats();
        std::cout << "  Total Frames: " << final_stats.frames_processed << "\n";
        std::cout << "  Queue Overruns: " << final_stats.queue_overruns << "\n";
        std::cout << "  Avg Latency: " << std::fixed << std::setprecision(2) 
                  << final_stats.avg_latency_ms << " ms\n";
        std::cout << "  Max Latency: " << final_stats.max_latency_ms << " ms\n";
        std::cout << "========================\n";
        
        // Print profiling report if enabled
        if (enable_profiling) {
            std::cout << "\n=== Performance Profile ===\n";
            apm::g_profiler.print_report();
            apm::g_profiler.export_json("apm_profile.json");
            std::cout << "Profile saved to: apm_profile.json\n";
        }
        
    } catch (const std::exception& e) {
        std::cerr << "\nError: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}
```

---

### 7. Comprehensive Testing Suite
**Priority:** High | **Complexity:** Medium | **Version:** 3.0

**Test Framework:** Google Test + Google Mock

**Dependencies:**
```cmake
find_package(GTest REQUIRED)
enable_testing()

add_executable(apm_tests
    tests/test_beamformer.cpp
    tests/test_echo_canceller.cpp
    tests/test_noise_suppression.cpp
    tests/test_streaming_processor.cpp
    tests/test_audio_io.cpp
)

target_link_libraries(apm_tests PRIVATE
    apm
    GTest::gtest_main
    GTest::gmock
)

gtest_discover_tests(apm_tests)
```

#### 7.1 Unit Tests

**tests/test_beamformer.cpp:**
```cpp
#include <gtest/gtest.h>
#include "apm/beamformer.hpp"

class BeamformerTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_.num_microphones = 4;
        config_.mic_spacing_m = 0.05f;
        config_.sample_rate = 48000;
    }
    
    apm::Beamformer::Config config_;
};

TEST_F(BeamformerTest, DelayAndSumBasic) {
    apm::Beamformer bf(config_);
    
    // Create test signal
    std::vector<apm::AudioFrame> mics;
    for (int i = 0; i < 4; ++i) {
        apm::AudioFrame frame(960, 48000, 1);
        // Fill with test data
        auto samples = frame.samples();
        for (size_t j = 0; j < samples.size(); ++j) {
            samples[j] = std::sin(2.0f * M_PI * 1000.0f * j / 48000.0f);
        }
        mics.push_back(std::move(frame));
    }
    
    auto output = bf.delay_and_sum(mics, 0.0f, 0.0f);
    
    ASSERT_EQ(output.frame_count(), 960);
    ASSERT_EQ(output.channels(), 1);
    EXPECT_GT(output.rms_level(), 0.0f);
}

TEST_F(BeamformerTest, SteeringVector) {
    apm::Beamformer bf(config_);
    
    float azimuth = 0.0f;
    float elevation = 0.0f;
    
    auto delays = bf.compute_steering_delays(azimuth, elevation);
    
    ASSERT_EQ(delays.size(), 4);
    EXPECT_GE(delays[0], 0.0f);
}

TEST_F(BeamformerTest, InvalidConfiguration) {
    config_.num_microphones = 0;
    EXPECT_THROW(apm::Beamformer(config_), std::invalid_argument);
}
```

**tests/test_streaming_processor.cpp:**
```cpp
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "apm/streaming_processor.hpp"
#include <chrono>
#include <thread>

using ::testing::_;
using ::testing::Invoke;

class StreamingProcessorTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_.num_microphones = 4;
        config_.mic_spacing_m = 0.05f;
        config_.num_speakers = 2;
        config_.speaker_spacing_m = 0.10f;
        config_.sample_rate = 48000;
    }
    
    apm::APMSystem::Config config_;
};

TEST_F(StreamingProcessorTest, EnqueueAndProcess) {
    apm::StreamingAPMProcessor processor(config_);
    
    std::atomic<int> callback_count{0};
    processor.set_output_callback([&](const auto& frames) {
        callback_count++;
    });
    
    // Enqueue frames
    for (int i = 0; i < 10; ++i) {
        apm::AudioFrame frame(960, 48000, 4);
        processor.enqueue_frame(std::move(frame));
    }
    
    // Wait for processing
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    EXPECT_GT(callback_count, 0);
    
    auto stats = processor.get_stats();
    EXPECT_GT(stats.frames_processed, 0);
    EXPECT_GE(stats.avg_latency_ms, 0.0);
}

TEST_F(StreamingProcessorTest, QueueOverrun) {
    apm::StreamingAPMProcessor processor(config_);
    
    // Enqueue many frames quickly
    for (int i = 0; i < 20; ++i) {
        apm::AudioFrame frame(960, 48000, 4);
        processor.enqueue_frame(std::move(frame));
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    auto stats = processor.get_stats();
    EXPECT_GT(stats.queue_overruns, 0);
}

TEST_F(StreamingProcessorTest, StatisticsReset) {
    apm::StreamingAPMProcessor processor(config_);
    
    for (int i = 0; i < 5; ++i) {
        apm::AudioFrame frame(960, 48000, 4);
        processor.enqueue_frame(std::move(frame));
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    processor.reset_stats();
    auto stats = processor.get_stats();
    
    EXPECT_EQ(stats.frames_processed, 0);
    EXPECT_EQ(stats.queue_overruns, 0);
}
```

#### 7.2 Integration Tests

**tests/integration/test_full_pipeline.cpp:**
```cpp
#include <gtest/gtest.h>
#include "apm/apm_system.hpp"
#include "apm/audio_io.hpp"
#include "apm/streaming_processor.hpp"

class FullPipelineTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_.num_microphones = 4;
        config_.mic_spacing_m = 0.05f;
        config_.num_speakers = 2;
        config_.speaker_spacing_m = 0.10f;
        config_.sample_rate = 48000;
        config_.source_language = "en-US";
        config_.target_language = "es-ES";
    }
    
    apm::APMSystem::Config config_;
};

TEST_F(FullPipelineTest, EndToEndProcessing) {
    apm::APMSystem apm(config_);
    
    // Create multichannel input
    std::vector<apm::AudioFrame> mics;
    for (int i = 0; i < 4; ++i) {
        apm::AudioFrame frame(960, 48000, 1);
        mics.push_back(std::move(frame));
    }
    
    apm::AudioFrame speaker_ref(960, 48000, 1);
    
    auto output = apm.process(mics, speaker_ref, 0.0f);
    
    ASSERT_EQ(output.size(), 2);  // Stereo output
    EXPECT_EQ(output[0].frame_count(), 960);
    EXPECT_EQ(output[1].frame_count(), 960);
}
```

#### 7.3 Performance Benchmarks

**tests/benchmarks/bench_beamforming.cpp:**
```cpp
#include <benchmark/benchmark.h>
#include "apm/beamformer.hpp"

static void BM_DelayAndSum(benchmark::State& state) {
    apm::Beamformer::Config config;
    config.num_microphones = state.range(0);
    config.mic_spacing_m = 0.05f;
    config.sample_rate = 48000;
    
    apm::Beamformer bf(config);
    
    std::vector<apm::AudioFrame> mics;
    for (int i = 0; i < config.num_microphones; ++i) {
        apm::AudioFrame frame(960, 48000, 1);
        mics.push_back(std::move(frame));
    }
    
    for (auto _ : state) {
        auto output = bf.delay_and_sum(mics, 0.0f, 0.0f);
        benchmark::DoNotOptimize(output);
    }
    
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_DelayAndSum)->Range(2, 8);

BENCHMARK_MAIN();
```

---

### 8. Packaging and Distribution
**Priority:** High | **Complexity:** Medium | **Version:** 3.0

#### 8.1 Debian Package

**debian/control:**
```
Source: apm-system
Section: sound
Priority: optional
Maintainer: Your Name <your.email@example.com>
Build-Depends: debhelper (>= 11),
               cmake (>= 3.15),
               libportaudio2 (>= 19),
               libasound2-dev,
               libcurl4-openssl-dev,
               nlohmann-json3-dev
Standards-Version: 4.5.0
Homepage: https://github.com/your-repo/apm-system

Package: apm-system
Architecture: any
Depends: ${shlibs:Depends}, ${misc:Depends},
         libportaudio2,
         libasound2,
         libcurl4
Description: Acoustic Projection Microphone System
 Real-time audio processing system with beamforming,
 echo cancellation, noise suppression, and translation.
```

**debian/rules:**
```makefile
#!/usr/bin/make -f
%:
	dh $@ --buildsystem=cmake

override_dh_auto_configure:
	dh_auto_configure -- \
		-DCMAKE_BUILD_TYPE=Release \
		-DBUILD_TESTING=OFF
```

#### 8.2 RPM Package

**apm-system.spec:**
```spec
Name:           apm-system
Version:        2.0.0
Release:        1%{?dist}
Summary:        Acoustic Projection Microphone System

License:        MIT
URL:            https://github.com/your-repo/apm-system
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  cmake >= 3.15
BuildRequires:  gcc-c++
BuildRequires:  portaudio-devel
BuildRequires:  alsa-lib-devel
BuildRequires:  libcurl-devel
BuildRequires:  json-devel

Requires:       portaudio
Requires:       alsa-lib
Requires:       libcurl

%description
Real-time audio processing system with multi-channel beamforming,
adaptive echo cancellation, LSTM-based noise suppression, and 
real-time translation capabilities.

%prep
%autosetup

%build
%cmake -DCMAKE_BUILD_TYPE=Release
%cmake_build

%install
%cmake_install

%files
%license LICENSE
%doc README.md
%{_bindir}/apm
%{_libdir}/libapm.so.*
%{_includedir}/apm/

%changelog
* Thu Dec 12 2024 Your Name <your.email@example.com> - 2.0.0-1
- Initial package release
```

#### 8.3 Homebrew Formula

**Formula/apm-system.rb:**
```ruby
class ApmSystem < Formula
  desc "Acoustic Projection Microphone System"
  homepage "https://github.com/your-repo/apm-system"
  url "https://github.com/your-repo/apm-system/archive/v2.0.0.tar.gz"
  sha256 "..."
  license "MIT"

  depends_on "cmake" => :build
  depends_on "portaudio"
  depends_on "curl"
  depends_on "nlohmann-json"

  def install
    mkdir "build" do
      system "cmake", "..", *std_cmake_args
      system "make", "install"
    end
  end

  test do
    system "#{bin}/apm", "--version"
  end
end
```

---

### 9. Documentation System
**Priority:** High | **Complexity:** Low | **Version:** 3.0

#### 9.1 Doxygen Configuration

**Doxyfile:**
```
PROJECT_NAME           = "APM System"
PROJECT_NUMBER         = "2.0"
OUTPUT_DIRECTORY       = docs
INPUT                  = include/ src/ README.md
RECURSIVE              = YES
EXTRACT_ALL            = YES
EXTRACT_PRIVATE        = NO
EXTRACT_STATIC         = YES
GENERATE_HTML          = YES
GENERATE_LATEX         = NO
HTML_OUTPUT            = html
USE_MDFILE_AS_MAINPAGE = README.md
```

#### 9.2 API Documentation Examples

**Example header with documentation:**
```cpp
/**
 * @file beamformer.hpp
 * @brief Multi-channel beamforming implementation
 * 
 * Provides delay-and-sum beamforming with configurable
 * microphone array geometry and steering capabilities.
 */

namespace apm {

/**
 * @class Beamformer
 * @brief Implements delay-and-sum beamforming
 * 
 * The Beamformer class processes multi-channel audio
 * to enhance signals from specific directions while
 * attenuating others.
 * 
 * @example
 * @code
 * Beamformer::Config config;
 * config.num_microphones = 4;
 * config.mic_spacing_m = 0.05f;
 * Beamformer bf(config);
 * 
 * auto output = bf.delay_and_sum(mics, azimuth, elevation);
 * @endcode
 */
class Beamformer {
public:
    /**
     * @brief Configuration parameters for beamformer
     */
    struct Config {
        int num_microphones;    ///< Number of microphones
        float mic_spacing_m;    ///< Spacing between mics (meters)
        int sample_rate;        ///< Sample rate (Hz)
    };
    
    /**
     * @brief Construct beamformer with configuration
     * @param config Configuration parameters
     * @throws std::invalid_argument if config is invalid
     */
    explicit Beamformer(const Config& config);
    
    /**
     * @brief Perform delay-and-sum beamforming
     * 
     * @param mic_array Array of microphone signals
     * @param azimuth Target azimuth angle (radians)
     * @param elevation Target elevation angle (radians)
     * @return Beamformed output signal
     * 
     * @note All input frames must have same length
     */
    AudioFrame delay_and_sum(
        const std::vector<AudioFrame>& mic_array,
        float azimuth,
        float elevation);
};

} // namespace apm
```

---

### 10. Deployment Guides
**Priority:** Medium | **Complexity:** Low | **Version:** 3.0

#### 10.1 Docker Production Deployment

**docker-compose.prod.yml:**
```yaml
version: '3.8'

services:
  apm:
    image: apm-system:2.0
    container_name: apm-prod
    restart: unless-stopped
    devices:
      - /dev/snd:/dev/snd
    environment:
      - APM_PRESET=Conference Room
      - APM_SOURCE_LANG=en-US
      - APM_TARGET_LANG=es-ES
      - APM_API_KEY=${APM_API_KEY}
      - APM_LOG_LEVEL=info
    volumes:
      - ./config:/etc/apm
      - ./logs:/var/log/apm
      - ./profiles:/var/lib/apm/profiles
    command: >
      --preset "Conference Room"
      --profile
      --log-level info
    healthcheck:
      test: ["CMD", "apm", "--version"]
      interval: 30s
      timeout: 10s
      retries: 3
    logging:
      driver: "json-file"
      options:
        max-size: "10m"
        max-file: "3"
```

#### 10.2 Systemd Service

**/etc/systemd/system/apm.service:**
```ini
[Unit]
Description=APM System Service
After=sound.target network.target
Wants=sound.target

[Service]
Type=simple
User=apm
Group=audio
ExecStart=/usr/bin/apm --preset "Conference Room" --profile
Restart=on-failure
RestartSec=10
StandardOutput=journal
StandardError=journal
Environment="APM_API_KEY=your-api-key"

# Security hardening
NoNewPrivileges=true
PrivateTmp=true
ProtectSystem=strict
ProtectHome=true
ReadWritePaths=/var/log/apm

[Install]
WantedBy=multi-user.target
```

---

## 📈 Performance Targets

| Metric | Target | Current |
|--------|--------|---------|
| End-to-end Latency | < 50ms | TBD |
| CPU Usage (4 mics) | < 40% (1 core) | TBD |
| Memory Footprint | < 100MB | TBD |
| Queue Overruns | < 0.1% | TBD |
| Translation Accuracy | > 95% WER | TBD |

## 🔐 Security Considerations

### API Key Management
- Store keys in environment variables or secure vaults
- Never commit keys to version control
- Rotate keys regularly
- Use separate keys for dev/prod

### Audio Data Privacy
- Process audio locally when possible
- Encrypt data in transit to translation APIs
- Implement data retention policies
- Provide user consent mechanisms

## 📚 Additional Resources

- [Architecture Documentation](docs/architecture.md)
- [Contributing Guide](CONTRIBUTING.md)
- [Performance Tuning](docs/performance.md)
- [Troubleshooting Guide](docs/troubleshooting.md)
- [API Reference](https://apm-system.readthedocs.io)

## 🤝 Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for development workflow and coding standards.

## 📄 License

MIT License - See [LICENSE](LICENSE) file for details.

---

**Last Updated:** December 11, 2024  
**Document Version:** 2.0  
**Status:** Draft
