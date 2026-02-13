#include "apm/comms/ptt_controller.hpp"
#include <iostream>
#include <cmath>

namespace apm {

PTTController::PTTController() = default;

PTTController::~PTTController() {
    shutdown();
}

bool PTTController::initialize(InputMethod method) {
    if (initialized_) {
        return true;
    }
    
    input_method_ = method;
    running_ = true;
    initialized_ = true;
    
    std::cout << "PTT Controller initialized (method: " 
              << static_cast<int>(method) << ")\n";
    
    return true;
}

void PTTController::shutdown() {
    if (!initialized_) {
        return;
    }
    
    running_ = false;
    
    if (state_thread_.joinable()) {
        state_thread_.join();
    }
    
    initialized_ = false;
    std::cout << "PTT Controller shutdown\n";
}

// ============================================================================
// PTT CONTROL
// ============================================================================

void PTTController::press() {
    if (!initialized_) {
        std::cerr << "PTT not initialized\n";
        return;
    }
    
    bool should_notify = false;
    bool should_beep = false;
    {
        std::lock_guard<std::mutex> lock(state_mutex_);

        State current = state_.load();

        // Don't allow press during cooldown
        if (current == State::COOLDOWN) {
            return;
        }

        if (current == State::IDLE) {
            press_time_ = std::chrono::steady_clock::now();
            state_.store(State::TRANSMITTING);
            transmission_count_++;
            should_beep = beep_enabled_;
            should_notify = true;
        }
    }

    if (should_beep) {
        play_beep(50); // Short beep on press
    }
    if (should_notify) {
        notify_state_change(State::TRANSMITTING);
        std::cout << "[PTT] Pressed - Transmitting\n";
    }
}

void PTTController::release() {
    if (!initialized_) {
        return;
    }
    
    bool should_beep = false;
    bool enter_cooldown = false;
    bool debounce_release = false;
    int64_t hold_duration = 0;

    {
        std::lock_guard<std::mutex> lock(state_mutex_);

        State current = state_.load();

        if (current == State::TRANSMITTING) {
            release_time_ = std::chrono::steady_clock::now();

            // Check minimum hold time
            hold_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                release_time_ - press_time_
            ).count();

            if (hold_duration < min_hold_ms_) {
                state_.store(State::IDLE);
                debounce_release = true;
            } else {
                state_.store(State::COOLDOWN);
                enter_cooldown = true;
                should_beep = beep_enabled_;
            }
        }
    }

    if (debounce_release) {
        std::cout << "[PTT] Released too quickly (debounced)\n";
        notify_state_change(State::IDLE);
        return;
    }

    if (!enter_cooldown) {
        return;
    }

    if (should_beep) {
        play_beep(50); // Short beep on release
    }

    notify_state_change(State::COOLDOWN);
    std::cout << "[PTT] Released - Cooldown (" << hold_duration << "ms)\n";

    if (state_thread_.joinable()) {
        state_thread_.join();
    }

    // Schedule return to IDLE after cooldown
    state_thread_ = std::thread([this]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(cooldown_ms_));
        if (!running_) {
            return;
        }
        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            if (state_.load() == State::COOLDOWN) {
                state_.store(State::IDLE);
            } else {
                return;
            }
        }
        notify_state_change(State::IDLE);
        std::cout << "[PTT] Cooldown complete - Ready\n";
    });
}

void PTTController::toggle() {
    State current = state_.load();
    
    if (current == State::TRANSMITTING) {
        release();
    } else if (current == State::IDLE) {
        press();
    }
}

// ============================================================================
// AUDIO PROCESSING
// ============================================================================

void PTTController::process_audio(const std::vector<float>& audio_data) {
    if (!is_transmitting()) {
        return; // Drop audio when not transmitting
    }
    
    // Accumulate in buffer
    {
        std::lock_guard<std::mutex> lock(audio_mutex_);
        audio_buffer_.insert(audio_buffer_.end(), audio_data.begin(), audio_data.end());
        total_samples_ += audio_data.size();
    }
    
    // Forward to callback if set
    if (audio_callback_) {
        audio_callback_(audio_data);
    }
}

std::vector<float> PTTController::get_audio_buffer() const {
    std::lock_guard<std::mutex> lock(audio_mutex_);
    return audio_buffer_;
}

void PTTController::clear_audio_buffer() {
    std::lock_guard<std::mutex> lock(audio_mutex_);
    audio_buffer_.clear();
}

// ============================================================================
// STATISTICS
// ============================================================================

int64_t PTTController::get_transmission_duration_ms() const {
    State current = state_.load();
    
    if (current == State::TRANSMITTING) {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            now - press_time_
        ).count();
    } else if (current == State::COOLDOWN) {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            release_time_ - press_time_
        ).count();
    }
    
    return 0;
}

void PTTController::reset_statistics() {
    transmission_count_ = 0;
    total_samples_ = 0;
    clear_audio_buffer();
}

// ============================================================================
// PRIVATE METHODS
// ============================================================================

void PTTController::play_beep(int duration_ms) {
    // Simple beep generation (platform-specific in real implementation)
    // This is a placeholder - would use actual audio output
    std::cout << "[BEEP] " << beep_frequency_ << "Hz for " << duration_ms << "ms\n";
    
    // In a real implementation, you would:
    // 1. Generate sine wave at beep_frequency_
    // 2. Output to audio device
    // 3. Duration of duration_ms
}

void PTTController::notify_state_change(State new_state) {
    if (state_callback_) {
        state_callback_(new_state);
    }
}

} // namespace apm
