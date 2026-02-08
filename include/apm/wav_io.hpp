#pragma once
#include <vector>
#include <string>

namespace apm {

struct WavData {
    std::vector<float> samples;
    int sample_rate = 0;
    int channels = 0;
};

bool load_wav(const std::string& path, WavData& out);
bool save_wav(const std::string& path,
              const std::vector<float>& samples,
              int sample_rate,
              int channels);

} // namespace apm
