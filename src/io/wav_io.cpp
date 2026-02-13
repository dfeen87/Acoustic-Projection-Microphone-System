#include "apm/wav_io.hpp"
#include <sndfile.h>
#include <iostream>

namespace apm {

bool load_wav(const std::string& path, WavData& out) {
    SF_INFO info{};
    SNDFILE* f = sf_open(path.c_str(), SFM_READ, &info);
    if (!f) {
        std::cerr << "Failed to open WAV file: " << path 
                  << " - " << sf_strerror(nullptr) << std::endl;
        return false;
    }

    out.samples.resize(info.frames * info.channels);
    sf_readf_float(f, out.samples.data(), info.frames);
    sf_close(f);

    out.sample_rate = info.samplerate;
    out.channels = info.channels;
    return true;
}

bool save_wav(const std::string& path,
              const std::vector<float>& samples,
              int sample_rate,
              int channels) {

    SF_INFO info{};
    info.samplerate = sample_rate;
    info.channels = channels;
    info.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;

    SNDFILE* f = sf_open(path.c_str(), SFM_WRITE, &info);
    if (!f) {
        std::cerr << "Failed to create WAV file: " << path 
                  << " - " << sf_strerror(nullptr) << std::endl;
        return false;
    }

    sf_writef_float(f, samples.data(), samples.size() / channels);
    sf_close(f);
    return true;
}

} // namespace apm
