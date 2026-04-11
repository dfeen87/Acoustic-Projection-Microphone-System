// ============================================================================
// APM Core Implementation
// Main orchestrator for the APM system
// ============================================================================

#include "apm/apm_system.h"
#include "apm/config.h"
#ifdef HAVE_LOCAL_TRANSLATION
#include "apm/translation/local_translation_adapter.h"
#endif
#include <iostream>
#include <algorithm>
#include <cmath>
#include <fstream>

namespace apm {

// ============================================================================
// AudioFrame Implementation
// ============================================================================

AudioFrame::AudioFrame(size_t samples, int sr, int ch)
    : data_(samples * ch), sample_rate_(sr), channels_(ch) {
    metadata_.timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch());
}

std::span<float> AudioFrame::samples() {
    return std::span<float>(data_.data(), data_.size());
}

std::span<const float> AudioFrame::samples() const {
    return std::span<const float>(data_.data(), data_.size());
}

size_t AudioFrame::frame_count() const {
    return channels_ > 0 ? data_.size() / channels_ : 0;
}

std::vector<float> AudioFrame::channel(int ch) const {
    if (ch >= channels_) return {};

    std::vector<float> result(frame_count());
    for (size_t i = 0; i < result.size(); ++i) {
        result[i] = data_[i * channels_ + ch];
    }
    return result;
}

void AudioFrame::compute_metadata() {
    if (data_.empty()) {
        metadata_.peak_db = -96.0f;
        metadata_.rms_db = -96.0f;
        metadata_.clipping = false;
        return;
    }

    float peak = 0.0f;
    float sum_sq = 0.0f;

    for (float s : data_) {
        float abs_s = std::abs(s);
        peak = std::max(peak, abs_s);
        sum_sq += s * s;
    }

    metadata_.peak_db = 20.0f * std::log10(peak + 1e-10f);
    metadata_.rms_db = 10.0f * std::log10(sum_sq / data_.size() + 1e-10f);
    metadata_.clipping = (peak > 1.0f);

}

// ============================================================================
// BeamformingEngine Implementation
// ============================================================================

BeamformingEngine::BeamformingEngine(int mics, float spacing)
    : array_size_(mics), spacing_m_(spacing) {}

float BeamformingEngine::lagrange_interpolate(
    std::span<const float> signal,
    float idx) const {

    const int order = 3;
    int base = static_cast<int>(idx);
    float frac = idx - base;

    if (base < order || base + order >= static_cast<int>(signal.size())) {
        return 0.0f;
    }

    float result = 0.0f;
    for (int n = -order; n <= order; ++n) {
        float prod = signal[base + n];
        for (int m = -order; m <= order; ++m) {
            if (m != n) {
                prod *= (frac - m) / (n - m);
            }
        }
        result += prod;
    }
    return result;
}

AudioFrame BeamformingEngine::delay_and_sum(
    const std::vector<AudioFrame>& mic_array,
    float azimuth_rad,
    float elevation_rad) {

    if (mic_array.empty()) {
        return AudioFrame(0, 48000, 1);
    }

    const size_t frame_size = mic_array[0].frame_count();
    const int sr = mic_array[0].sample_rate();
    AudioFrame output(frame_size, sr, 1);
    auto out_span = output.samples();

    std::vector<float> delays(array_size_);
    for (int m = 0; m < array_size_; ++m) {
        float pos = m * spacing_m_;
        delays[m] =
            (pos * std::sin(azimuth_rad) * std::cos(elevation_rad)) /
            speed_of_sound_;
        delays[m] *= sr;
    }

    for (size_t i = 0; i < frame_size; ++i) {
        float sum = 0.0f;
        for (int m = 0;
             m < std::min(array_size_, static_cast<int>(mic_array.size()));
             ++m) {

            float delayed_idx = static_cast<float>(i) - delays[m];
            if (delayed_idx >= 0 &&
                delayed_idx < mic_array[m].frame_count()) {

                sum += lagrange_interpolate(
                    mic_array[m].samples(), delayed_idx);
            }
        }
        out_span[i] = sum / array_size_;
    }

    output.compute_metadata();
    return output;
}

AudioFrame BeamformingEngine::superdirective(
    const std::vector<AudioFrame>& mic_array,
    float azimuth_rad) {

    return delay_and_sum(mic_array, azimuth_rad, 0.0f);
}

AudioFrame BeamformingEngine::adaptive_null_steering(
    const std::vector<AudioFrame>& mic_array,
    float target_azimuth,
    const std::vector<float>& /* interference_azimuths */) {

    return delay_and_sum(mic_array, target_azimuth, 0.0f);
}

// ============================================================================
// NoiseSuppressionEngine Implementation
// ============================================================================

struct NoiseSuppressionEngine::LSTMState {
    std::vector<float> hidden;
    std::vector<float> cell;
    int size;

    LSTMState(int sz)
        : hidden(sz, 0.0f), cell(sz, 0.0f), size(sz) {}

    void reset() {
        std::fill(hidden.begin(), hidden.end(), 0.0f);
        std::fill(cell.begin(), cell.end(), 0.0f);
    }
};

NoiseSuppressionEngine::NoiseSuppressionEngine()
    : lstm_state_(std::make_unique<LSTMState>(256)) {
    init_window();
    prev_frame_.resize(FFT_SIZE, 0.0f);
}

NoiseSuppressionEngine::~NoiseSuppressionEngine() = default;

void NoiseSuppressionEngine::init_window() {
    hann_window_.resize(FFT_SIZE);
    for (int i = 0; i < FFT_SIZE; ++i) {
        hann_window_[i] =
            0.5f * (1.0f - std::cos(2.0f * M_PI * i / FFT_SIZE));
    }
}

std::vector<float> NoiseSuppressionEngine::compute_features(
    const std::vector<float>& frame) {

    std::vector<float> features;

    for (float sample : frame) {
        features.push_back(std::log(std::abs(sample) + 1e-10f));
    }

    float energy = 0.0f;
    float centroid = 0.0f;

    for (size_t i = 0; i < frame.size(); ++i) {
        float mag = std::abs(frame[i]);
        energy += mag * mag;
        centroid += i * mag;
    }

    features.push_back(energy);
    features.push_back(centroid / (energy + 1e-10f));

    return features;
}

std::vector<float> NoiseSuppressionEngine::lstm_forward(
    const std::vector<float>& input) {

    std::vector<float> output(FFT_SIZE / 2);

    for (size_t i = 0; i < output.size(); ++i) {
        float f =
            1.0f /
            (1.0f + std::exp(-(input[i % input.size()] +
                               lstm_state_->hidden[i % lstm_state_->size])));

        float in =
            1.0f /
            (1.0f + std::exp(-(input[i % input.size()] * 0.5f)));

        lstm_state_->cell[i % lstm_state_->size] =
            f * lstm_state_->cell[i % lstm_state_->size] +
            in * std::tanh(input[i % input.size()]);

        float o =
            1.0f /
            (1.0f + std::exp(-input[i % input.size()]));

        lstm_state_->hidden[i % lstm_state_->size] =
            o * std::tanh(lstm_state_->cell[i % lstm_state_->size]);

        output[i] =
            1.0f /
            (1.0f + std::exp(-lstm_state_->hidden[i % lstm_state_->size]));
    }

    return output;
}

AudioFrame NoiseSuppressionEngine::suppress(const AudioFrame& noisy) {
    AudioFrame output = noisy;

    auto in_samples = noisy.samples();
    auto out_samples = output.samples();

    for (size_t pos = 0;
         pos + FFT_SIZE <= in_samples.size();
         pos += HOP_SIZE) {

        std::vector<float> frame(FFT_SIZE);

        for (int i = 0; i < FFT_SIZE; ++i) {
            frame[i] = in_samples[pos + i] * hann_window_[i];
        }

        auto features = compute_features(frame);
        auto mask = lstm_forward(features);

        for (size_t i = 0;
             i < HOP_SIZE && pos + i < out_samples.size();
             ++i) {

            float avg_mask =
                std::accumulate(mask.begin(), mask.end(), 0.0f) /
                mask.size();

            out_samples[pos + i] =
                in_samples[pos + i] * avg_mask;
        }
    }

    output.compute_metadata();
    return output;
}

void NoiseSuppressionEngine::reset_state() {
    lstm_state_->reset();
    std::fill(prev_frame_.begin(), prev_frame_.end(), 0.0f);
}

// ============================================================================
// EchoCancellationEngine Implementation
// ============================================================================

EchoCancellationEngine::EchoCancellationEngine(int filter_len)
    : filter_length_(filter_len) {

    adaptive_weights_.resize(filter_length_, 0.0f);
    // Deque does not need pre-sizing, will grow dynamically
}

AudioFrame EchoCancellationEngine::cancel_echo(
    const AudioFrame& microphone,
    const AudioFrame& speaker_reference) {

    AudioFrame output = microphone;

    auto mic_samples = microphone.samples();
    auto ref_samples = speaker_reference.samples();
    auto out_samples = output.samples();

    size_t min_size =
        std::min(mic_samples.size(), ref_samples.size());

    for (size_t i = 0; i < min_size; ++i) {

        // Use push_front for O(1) insertion instead of vector insert
        reference_buffer_.push_front(ref_samples[i]);

        if (reference_buffer_.size() > static_cast<size_t>(filter_length_)) {
            reference_buffer_.pop_back();
        }

        float echo_estimate = 0.0f;

        size_t limit = std::min(
            static_cast<size_t>(filter_length_),
            reference_buffer_.size());

        for (size_t j = 0; j < limit; ++j) {
            echo_estimate +=
                adaptive_weights_[j] * reference_buffer_[j];
        }

        float error = mic_samples[i] - echo_estimate;
        out_samples[i] = error;

        float ref_power = 0.0f;
        for (float r : reference_buffer_) {
            ref_power += r * r;
        }
        ref_power = std::max(ref_power, 1e-6f);

        float step = mu_ * error / ref_power;

        for (size_t j = 0; j < limit; ++j) {
            adaptive_weights_[j] +=
                step * reference_buffer_[j];
        }
    }

    output.compute_metadata();
    return output;
}

bool EchoCancellationEngine::detect_double_talk(
    const AudioFrame& mic,
    const AudioFrame& ref) const {

    float mic_energy = 0.0f;
    float ref_energy = 0.0f;

    for (float s : mic.samples()) mic_energy += s * s;
    for (float s : ref.samples()) ref_energy += s * s;

    mic_energy /= mic.samples().size();
    ref_energy /= ref.samples().size();

    return mic_energy > 4.0f * ref_energy;
}

void EchoCancellationEngine::reset() {
    std::fill(adaptive_weights_.begin(),
              adaptive_weights_.end(), 0.0f);
    std::fill(reference_buffer_.begin(),
              reference_buffer_.end(), 0.0f);
}

// ============================================================================
// VoiceActivityDetector Implementation
// ============================================================================

float VoiceActivityDetector::compute_energy_db(
    const AudioFrame& frame) const {

    float energy = 0.0f;
    for (float s : frame.samples()) {
        energy += s * s;
    }

    return 10.0f *
           std::log10(energy / frame.samples().size() + 1e-10f);
}

int VoiceActivityDetector::compute_zero_crossing_rate(
    const AudioFrame& frame) const {

    int crossings = 0;
    auto samples = frame.samples();

    for (size_t i = 1; i < samples.size(); ++i) {
        if ((samples[i] >= 0.0f) !=
            (samples[i - 1] >= 0.0f)) {
            crossings++;
        }
    }
    return crossings;
}

VoiceActivityDetector::VadResult
VoiceActivityDetector::detect(const AudioFrame& frame) {

    float energy_db = compute_energy_db(frame);
    int zcr = compute_zero_crossing_rate(frame);

    bool energy_check = energy_db > energy_threshold_db_;
    bool zcr_check = (zcr > 50) && (zcr < 300);
    bool is_speech = energy_check && zcr_check;

    if (is_speech) {
        current_hangover_ = hangover_frames_;
    } else if (current_hangover_ > 0) {
        current_hangover_--;
        is_speech = true;
    }

    float confidence =
        is_speech
            ? std::min(1.0f,
                       (energy_db - energy_threshold_db_) / 20.0f)
            : 0.0f;

    return {
        .speech_detected = is_speech,
        .confidence = std::clamp(confidence, 0.0f, 1.0f),
        .snr_db = energy_db - energy_threshold_db_,
        .energy_db = energy_db
    };
}

void VoiceActivityDetector::adapt_threshold(
    float ambient_noise_db) {

    energy_threshold_db_ = ambient_noise_db + 15.0f;
}

void VoiceActivityDetector::reset() {
    current_hangover_ = 0;
}

// ============================================================================
// MockTranslationEngine Implementation
// ============================================================================

std::future<TranslationEngine::TranslationResult>
MockTranslationEngine::translate_async(
    const TranslationRequest& request) {

    return std::async(std::launch::async, [request]() {
        std::this_thread::sleep_for(
            std::chrono::milliseconds(200));

        TranslationResult result;
        result.translated_audio = AudioFrame(
            request.audio.frame_count(),
            request.audio.sample_rate(),
            1);

        result.source_text = "Hello world";
        result.translated_text = "Hola mundo";
        result.confidence = 0.95f;
        result.latency_ms = 200;

        return result;
    });
}

// ============================================================================
// DirectionalProjector Implementation
// ============================================================================

DirectionalProjector::DirectionalProjector(
    int speakers,
    float spacing)
    : speaker_array_size_(speakers),
      spacing_m_(spacing) {}

std::vector<AudioFrame>
DirectionalProjector::create_projection_signals(
    const AudioFrame& source,
    float target_azimuth_rad,
    float target_distance_m) {

    std::vector<AudioFrame> speaker_signals;

    for (int sp = 0; sp < speaker_array_size_; ++sp) {
        AudioFrame signal = source;
        auto samples = signal.samples();

        float pos = sp * spacing_m_;
        float delay_sec =
            (pos * std::sin(target_azimuth_rad)) /
            speed_of_sound_;

        int delay_samples =
            static_cast<int>(delay_sec * source.sample_rate());

        if (delay_samples > 0 &&
            delay_samples < static_cast<int>(samples.size())) {

            std::rotate(samples.begin(),
                        samples.begin() + delay_samples,
                        samples.end());
        }

        float attenuation =
            1.0f /
            (target_distance_m * target_distance_m + 1.0f);

        for (float& s : samples) {
            s *= attenuation;
        }

        speaker_signals.push_back(signal);
    }

    return speaker_signals;
}

// ============================================================================
// ProfileManager Implementation
// ============================================================================

ProfileManager::ProfileManager() {
    // Add default profile
    Profile default_profile;
    default_profile.name = "Default";
    profiles_.push_back(default_profile);
    current_profile_name_ = "Default";
}

bool ProfileManager::save_profile(const Profile& profile, const std::string& filepath) {
    // Simple Key-Value serialization
    std::ofstream out(filepath);
    if (!out.is_open()) return false;

    out << "name=" << profile.name << "\n";
    out << "rms_noise_floor_db=" << profile.calibration.rms_noise_floor_db << "\n";
    out << "peak_noise_floor_db=" << profile.calibration.peak_noise_floor_db << "\n";
    out << "recommended_input_gain=" << profile.calibration.recommended_input_gain << "\n";
    out << "estimated_latency_ms=" << profile.calibration.estimated_latency_ms << "\n";
    out << "calibration_valid=" << (profile.calibration.valid ? "1" : "0") << "\n";
    out << "max_feedback_attenuation_db=" << profile.max_feedback_attenuation_db << "\n";
    out << "max_feedback_notches=" << profile.max_feedback_notches << "\n";
    out << "feedback_suppression_enabled=" << (profile.feedback_suppression_enabled ? "1" : "0") << "\n";
    out << "user_eq_low_gain=" << profile.user_eq_low_gain << "\n";
    out << "user_eq_mid_gain=" << profile.user_eq_mid_gain << "\n";
    out << "user_eq_high_gain=" << profile.user_eq_high_gain << "\n";

    return true;
}

std::optional<Profile> ProfileManager::load_profile(const std::string& filepath) {
    std::ifstream in(filepath);
    if (!in.is_open()) return std::nullopt;

    Profile profile;
    std::string line;
    while (std::getline(in, line)) {
        auto pos = line.find('=');
        if (pos == std::string::npos) continue;

        std::string key = line.substr(0, pos);
        std::string value = line.substr(pos + 1);

        try {
            if (key == "name") profile.name = value;
            else if (key == "rms_noise_floor_db") profile.calibration.rms_noise_floor_db = std::stof(value);
            else if (key == "peak_noise_floor_db") profile.calibration.peak_noise_floor_db = std::stof(value);
            else if (key == "recommended_input_gain") profile.calibration.recommended_input_gain = std::stof(value);
            else if (key == "estimated_latency_ms") profile.calibration.estimated_latency_ms = std::stof(value);
            else if (key == "calibration_valid") profile.calibration.valid = (value == "1");
            else if (key == "max_feedback_attenuation_db") profile.max_feedback_attenuation_db = std::stof(value);
            else if (key == "max_feedback_notches") profile.max_feedback_notches = std::stoi(value);
            else if (key == "feedback_suppression_enabled") profile.feedback_suppression_enabled = (value == "1");
            else if (key == "user_eq_low_gain") profile.user_eq_low_gain = std::stof(value);
            else if (key == "user_eq_mid_gain") profile.user_eq_mid_gain = std::stof(value);
            else if (key == "user_eq_high_gain") profile.user_eq_high_gain = std::stof(value);
        } catch (...) {
            // Ignore parse errors on individual lines
        }
    }
    return profile;
}

void ProfileManager::add_profile(const Profile& profile) {
    auto it = std::find_if(profiles_.begin(), profiles_.end(),
                           [&](const Profile& p) { return p.name == profile.name; });
    if (it != profiles_.end()) {
        *it = profile;
    } else {
        profiles_.push_back(profile);
    }
}

std::vector<std::string> ProfileManager::get_profile_names() const {
    std::vector<std::string> names;
    for (const auto& p : profiles_) names.push_back(p.name);
    return names;
}

std::optional<Profile> ProfileManager::get_profile(const std::string& name) const {
    auto it = std::find_if(profiles_.begin(), profiles_.end(),
                           [&](const Profile& p) { return p.name == name; });
    if (it != profiles_.end()) return *it;
    return std::nullopt;
}

void ProfileManager::set_active_profile(const std::string& name) {
    if (get_profile(name)) {
        current_profile_name_ = name;
    }
}

std::string ProfileManager::get_active_profile_name() const {
    return current_profile_name_;
}

// ============================================================================
// AutoCalibrationEngine Implementation
// ============================================================================

void AutoCalibrationEngine::start_calibration() {
    current_step_ = Step::MeasureNoiseFloor;
    current_profile_ = CalibrationProfile{};
    frames_processed_ = 0;
    target_frames_ = 100; // ~2 seconds at 50fps
    acc_energy_ = 0.0f;
    max_peak_ = -96.0f;
}

void AutoCalibrationEngine::cancel_calibration() {
    current_step_ = Step::Idle;
}

void AutoCalibrationEngine::advance_step() {
    switch (current_step_) {
        case Step::MeasureNoiseFloor:
            current_profile_.rms_noise_floor_db = 10.0f * std::log10(std::max(acc_energy_ / frames_processed_, 1e-10f));
            current_profile_.peak_noise_floor_db = max_peak_;
            current_step_ = Step::MeasureGain;
            break;
        case Step::MeasureGain:
            // Calculate headroom from peak, aim for -12dBFS
            current_profile_.recommended_input_gain = std::pow(10.0f, (-12.0f - max_peak_) / 20.0f);
            current_step_ = Step::MeasureLatency;
            break;
        case Step::MeasureLatency:
            // Mock latency estimation
            current_profile_.estimated_latency_ms = 45.0f;
            current_profile_.valid = true;
            current_step_ = Step::Complete;
            break;
        default:
            break;
    }

    // Reset accumulators for next step
    frames_processed_ = 0;
    acc_energy_ = 0.0f;
    max_peak_ = -96.0f;
}

void AutoCalibrationEngine::process_frame(const AudioFrame& frame) {
    if (current_step_ == Step::Idle || current_step_ == Step::Complete) return;

    float energy = 0.0f;
    float peak = 0.0f;

    auto samples = frame.samples();
    if (samples.empty()) return;

    for (float s : samples) {
        energy += s * s;
        peak = std::max(peak, std::abs(s));
    }
    energy /= samples.size();

    acc_energy_ += energy;

    float peak_db = 20.0f * std::log10(std::max(peak, 1e-10f));
    max_peak_ = std::max(max_peak_, peak_db);

    frames_processed_++;

    if (frames_processed_ >= target_frames_) {
        advance_step();
    }
}

float AutoCalibrationEngine::get_progress() const {
    if (current_step_ == Step::Idle) return 0.0f;
    if (current_step_ == Step::Complete) return 1.0f;

    float step_base = 0.0f;
    switch (current_step_) {
        case Step::MeasureNoiseFloor: step_base = 0.0f; break;
        case Step::MeasureGain: step_base = 0.33f; break;
        case Step::MeasureLatency: step_base = 0.66f; break;
        default: break;
    }

    float step_progress = static_cast<float>(frames_processed_) / target_frames_;
    return step_base + (step_progress * 0.33f);
}

// ============================================================================
// FeedbackSuppressionEngine Implementation
// ============================================================================

void FeedbackSuppressionEngine::BiquadFilter::set_notch(float freq, float sr, float q) {
    freq_hz = freq;
    float w0 = 2.0f * M_PI * freq / sr;
    float alpha = std::sin(w0) / (2.0f * q);

    float a0 = 1.0f + alpha;
    b0 = 1.0f / a0;
    b1 = (-2.0f * std::cos(w0)) / a0;
    b2 = 1.0f / a0;
    a1 = (-2.0f * std::cos(w0)) / a0;
    a2 = (1.0f - alpha) / a0;

    x1 = x2 = y1 = y2 = 0.0f;
}

float FeedbackSuppressionEngine::BiquadFilter::process(float in) {
    float out = b0 * in + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
    x2 = x1;
    x1 = in;
    y2 = y1;
    y1 = out;
    return out;
}

FeedbackSuppressionEngine::FeedbackSuppressionEngine(int max_notches, float max_attenuation)
    : max_notches_(max_notches), max_attenuation_db_(max_attenuation) {
}

void FeedbackSuppressionEngine::configure(int sample_rate, int max_notches, float max_attenuation) {
    sample_rate_ = sample_rate;
    max_notches_ = max_notches;
    max_attenuation_db_ = max_attenuation;
}

float FeedbackSuppressionEngine::detect_feedback_freq(const AudioFrame& frame) const {
    auto samples = frame.samples();
    if (samples.empty()) return 0.0f;

    // Simple ZCR heuristic for narrowband feedback detection
    int zero_crossings = 0;
    for (size_t i = 1; i < samples.size(); ++i) {
        if ((samples[i] >= 0.0f && samples[i - 1] < 0.0f) ||
            (samples[i] < 0.0f && samples[i - 1] >= 0.0f)) {
            zero_crossings++;
        }
    }

    float duration_sec = static_cast<float>(samples.size()) / sample_rate_;
    float freq = (zero_crossings / 2.0f) / duration_sec;

    // Only return confident high frequencies (typical feedback)
    if (freq > 1000.0f && freq < 10000.0f) {
        // Very basic purity check: peak must be high
        float peak = 0.0f;
        for (float s : samples) peak = std::max(peak, std::abs(s));
        if (peak > 0.8f) return freq;
    }

    return 0.0f;
}

AudioFrame FeedbackSuppressionEngine::process(const AudioFrame& frame) {
    AudioFrame output = frame;
    auto samples = output.samples();

    // 1. Detect feedback
    frames_since_update_++;
    if (frames_since_update_ > 10) { // Check every ~10 frames
        float freq = detect_feedback_freq(frame);
        if (freq > 0.0f) {
            // Add or update notch
            if (notches_.size() < static_cast<size_t>(max_notches_)) {
                BiquadFilter notch;
                notch.set_notch(freq, sample_rate_);
                notches_.push_back(notch);
            } else {
                // Replace oldest
                notches_.front().set_notch(freq, sample_rate_);
                std::rotate(notches_.begin(), notches_.begin() + 1, notches_.end());
            }
        }
        frames_since_update_ = 0;
    }

    // 2. Apply notches
    for (float& s : samples) {
        for (auto& notch : notches_) {
            s = notch.process(s);
        }
    }

    output.compute_metadata();
    return output;
}

void FeedbackSuppressionEngine::reset() {
    notches_.clear();
    frames_since_update_ = 0;
    zcr_history_.clear();
}

// ============================================================================
// DiagnosticsEngine Implementation
// ============================================================================

HealthStatus DiagnosticsEngine::run_startup_checks(int expected_sample_rate, int expected_channels) {
    HealthStatus status;
    status.ok = true;

    // Mock checks - in reality these would query PortAudio directly
    // Assuming PortAudio handles these inside its initialize, we just simulate the checks here
    if (expected_sample_rate <= 0) {
        status.ok = false;
        status.sample_rate_ok = false;
        status.message = "Invalid sample rate configured";
    }
    if (expected_channels <= 0) {
        status.ok = false;
        status.channel_mapping_ok = false;
        status.message = "Invalid channel mapping";
    }

    return status;
}

// ============================================================================
// APMSystem Implementation
// ============================================================================

APMSystem::APMSystem(const Config& cfg)
    : config_(cfg),
      beamformer_(cfg.num_microphones, cfg.mic_spacing_m),
      echo_canceller_(2048),
      projector_(cfg.num_speakers, cfg.speaker_spacing_m) {

    feedback_suppressor_.configure(cfg.sample_rate, 3, 12.0f);

#ifdef HAVE_LOCAL_TRANSLATION
    try {
        LocalTranslationEngine::Config trans_config;
        trans_config.source_language = cfg.source_language.substr(0, 2); // Extract 'en' from 'en-US'
        trans_config.target_language = cfg.target_language.substr(0, 2);

        translator_ = std::make_unique<LocalTranslationAdapter>(trans_config);
        std::cout << "APMSystem: Initialized Local Translation Engine ("
                  << trans_config.source_language << " -> "
                  << trans_config.target_language << ")" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "APMSystem: Failed to initialize Local Translation Engine: " << e.what() << std::endl;
        std::cerr << "APMSystem: Falling back to Mock Translation Engine" << std::endl;
        translator_ = std::make_unique<MockTranslationEngine>();
    }
#else
    translator_ = std::make_unique<MockTranslationEngine>();
    std::cout << "APMSystem: Using Mock Translation Engine (Local translation disabled)" << std::endl;
#endif
}

std::future<std::vector<AudioFrame>>
APMSystem::process_async(
    const std::vector<AudioFrame>& microphone_array,
    const AudioFrame& speaker_reference,
    float target_direction_rad) {

    return std::async(
        std::launch::async,
        [this, microphone_array, speaker_reference, target_direction_rad]() {
            return process(
                microphone_array,
                speaker_reference,
                target_direction_rad);
        });
}

std::vector<AudioFrame> APMSystem::process(
    const std::vector<AudioFrame>& microphone_array,
    const AudioFrame& speaker_reference,
    float target_direction_rad) {

    // ---- DSP PIPELINE ONLY ----
    auto beamformed =
        beamformer_.delay_and_sum(
            microphone_array,
            target_direction_rad,
            0.0f);

    auto echo_cancelled =
        echo_canceller_.cancel_echo(
            beamformed,
            speaker_reference);

    auto denoised =
        noise_suppressor_.suppress(
            echo_cancelled);

    auto feedback_suppressed =
        feedback_suppressor_.process(denoised);

    auto vad_result =
        vad_.detect(feedback_suppressed);

    // Update monitoring metrics
    {
        std::lock_guard<std::mutex> lock(metrics_mutex_);
        current_metrics_.peak_db = feedback_suppressed.metadata().peak_db;
        current_metrics_.rms_db = feedback_suppressed.metadata().rms_db;
        current_metrics_.snr_db = vad_result.snr_db;
        current_metrics_.clipping = feedback_suppressed.metadata().clipping;
        // Mock latency update
        current_metrics_.latency_ms = 4.2f;
    }

    // Call calibration engine
    if (calibration_engine_.get_current_step() != AutoCalibrationEngine::Step::Idle &&
        calibration_engine_.get_current_step() != AutoCalibrationEngine::Step::Complete) {
        calibration_engine_.process_frame(feedback_suppressed);
    }

    if (!vad_result.speech_detected) {
        return {};
    }
    // ---- END DSP ----

    // Async / slow path (explicitly excluded from metric)
    TranslationEngine::TranslationRequest trans_req;
    trans_req.audio = denoised;
    trans_req.source_lang = config_.source_language;
    trans_req.target_lang = config_.target_language;

    auto translation_result =
        translator_->translate_async(trans_req).get();

    return projector_.create_projection_signals(
        translation_result.translated_audio,
        target_direction_rad,
        1.5f);
}

void APMSystem::reset_all() {
    echo_canceller_.reset();
    noise_suppressor_.reset_state();
    vad_.reset();
    feedback_suppressor_.reset();
}

MonitoringMetrics APMSystem::get_monitoring_metrics() const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    return current_metrics_;
}

} // namespace apm
