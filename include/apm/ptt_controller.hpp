#pragma once

#include <functional>
#include <atomic>
#include <thread>
#include <vector>
#include <chrono>
#include <mutex>

namespace apm {

/**
 * @brief Push-to-Talk (PTT) Controller
 * 
 * Manages audio recording state based on PTT button/key press.
 * Only captures audio when actively transmitting.
 */
class PTTController {
public:
    /**
     * @brief PTT state
     */
    enum class State {
        IDLE,           // Not transmitting
        TRANSMITTING,   // Actively transmitting
        COOLDOWN        // Brief cooldown after release
    };
    
    /**
     * @brief PTT input method
     */
    enum class InputMethod {
        KEYBOARD,       // Keyboard key (e.g., Space bar)
        MOUSE,          // Mouse button
        EXTERNAL,       // External button/pedal
        SOFTWARE        // Software-controlled
    };
    
    /**
     * @brief Callback function types
     */
    using StateCallback = std::function<void(State)>;
    using AudioCallback = std::function<void(const std::vector<float>&)>;
    
    PTTController();
    ~PTTController();
    
    // Disable copy, enable move
    PTTController(const PTTController&) = delete;
    PTTController& operator=(const PTTController&) = delete;
    PTTController(PTTController&&) = default;
    PTTController& operator=(PTTController&&) = default;
    
    /**
     * @brief Initialize PTT controller
     * @param method Input method to use
     * @return true if initialized successfully
     */
    bool initialize(InputMethod method = InputMethod::SOFTWARE);
    
    /**
     * @brief Shutdown PTT controller
     */
    void shutdown();
    
    /**
     * @brief Check if PTT is initialized
     */
    bool is_initialized() const { return initialized_; }
    
    // ========================================================================
    // PTT CONTROL
    // ========================================================================
    
    /**
     * @brief Start transmitting (press PTT)
     */
    void press();
    
    /**
     * @brief Stop transmitting (release PTT)
     */
    void release();
    
    /**
     * @brief Toggle PTT state
     */
    void toggle();
    
    /**
     * @brief Get current PTT state
     */
    State get_state() const { return state_.load(); }
    
    /**
     * @brief Check if currently transmitting
     */
    bool is_transmitting() const { 
        return state_.load() == State::TRANSMITTING; 
    }
    
    // ========================================================================
    // CONFIGURATION
    // ========================================================================
    
    /**
     * @brief Set cooldown period after release (ms)
     * Prevents accidental re-triggering
     */
    void set_cooldown_ms(int ms) { cooldown_ms_ = ms; }
    
    /**
     * @brief Set minimum hold time (ms)
     * Ignores very brief presses (debouncing)
     */
    void set_min_hold_ms(int ms) { min_hold_ms_ = ms; }
    
    /**
     * @brief Enable/disable beep on press/release
     */
    void set_beep_enabled(bool enabled) { beep_enabled_ = enabled; }
    
    /**
     * @brief Set beep frequency (Hz)
     */
    void set_beep_frequency(int hz) { beep_frequency_ = hz; }
    
    // ========================================================================
    // CALLBACKS
    // ========================================================================
    
    /**
     * @brief Set callback for state changes
     */
    void on_state_changed(StateCallback callback) {
        state_callback_ = callback;
    }
    
    /**
     * @brief Set callback for audio data (only when transmitting)
     */
    void on_audio_available(AudioCallback callback) {
        audio_callback_ = callback;
    }
    
    // ========================================================================
    // AUDIO PROCESSING
    // ========================================================================
    
    /**
     * @brief Process audio chunk
     * Only forwards to callback if transmitting
     */
    void process_audio(const std::vector<float>& audio_data);
    
    /**
     * @brief Get audio buffer
     * Returns accumulated audio since press
     */
    std::vector<float> get_audio_buffer() const;
    
    /**
     * @brief Clear audio buffer
     */
    void clear_audio_buffer();
    
    // ========================================================================
    // STATISTICS
    // ========================================================================
    
    /**
     * @brief Get transmission duration (ms)
     */
    int64_t get_transmission_duration_ms() const;
    
    /**
     * @brief Get total transmissions count
     */
    uint64_t get_transmission_count() const { return transmission_count_; }
    
    /**
     * @brief Get total audio samples captured
     */
    uint64_t get_total_samples() const { return total_samples_; }
    
    /**
     * @brief Reset statistics
     */
    void reset_statistics();

private:
    void play_beep(int duration_ms);
    void notify_state_change(State new_state);
    
    std::atomic<bool> initialized_{false};
    std::atomic<bool> running_{false};
    std::atomic<State> state_{State::IDLE};
    
    InputMethod input_method_{InputMethod::SOFTWARE};
    
    // Timing
    int cooldown_ms_{100};
    int min_hold_ms_{50};
    std::chrono::steady_clock::time_point press_time_;
    std::chrono::steady_clock::time_point release_time_;
    
    // Audio
    std::vector<float> audio_buffer_;
    mutable std::mutex audio_mutex_;
    
    // Beep
    bool beep_enabled_{true};
    int beep_frequency_{1000};
    
    // Callbacks
    StateCallback state_callback_;
    AudioCallback audio_callback_;
    
    // Statistics
    uint64_t transmission_count_{0};
    uint64_t total_samples_{0};
    
    // Threading
    std::thread state_thread_;
    std::mutex state_mutex_;
};

} // namespace apm
