#include "apm/apm_core.hpp"
#include <algorithm>
#include <cmath>

namespace apm {

std::string APMCore::get_version() const {
    return "1.0.0";
}

bool APMCore::initialize(int sample_rate, int num_channels) {
    if (sample_rate <= 0 || num_channels <= 0) {
        return false;
    }

    sample_rate_ = sample_rate;
    num_channels_ = num_channels;
    initialized_ = true;

    return true;
}

std::vector<float> APMCore::process(const std::vector<float>& input) {
    if (!initialized_) {
        return {};
    }

    // Simple pass-through for now
    // In the future, this will do beamforming, echo cancellation, etc.
    std::vector<float> output = input;

    // Apply a simple gain (placeholder processing)
    for (auto& sample : output) {
        sample *= 0.95f; // Slight attenuation
    }

    return output;
}

} // namespace apm
