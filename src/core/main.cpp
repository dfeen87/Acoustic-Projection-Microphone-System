#include "apm/apm_system.h"
#include "apm/io/audio_device.h"
#include <iostream>
#include <thread>
#include <atomic>
#include <csignal>
#include <vector>
#include <deque>
#include <mutex>
#include <condition_variable>
#include <cmath>
#include <algorithm>

using namespace apm;

// Thread-safe queue for buffering audio chunks
template <typename T>
class ThreadSafeQueue {
    std::deque<T> queue_;
    mutable std::mutex mutex_;
    std::condition_variable cond_;
    size_t max_size_;

public:
    explicit ThreadSafeQueue(size_t max_size = 100) : max_size_(max_size) {}

    void push(T value) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.size() >= max_size_) {
            queue_.pop_front(); // Drop oldest if full (overflow)
        }
        queue_.push_back(std::move(value));
        cond_.notify_one();
    }
    
    bool try_pop(T& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) return false;
        value = std::move(queue_.front());
        queue_.pop_front();
        return true;
    }
    
    bool wait_and_pop(T& value, std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (!cond_.wait_for(lock, timeout, [this]{ return !queue_.empty(); })) {
            return false;
        }
        value = std::move(queue_.front());
        queue_.pop_front();
        return true;
    }
};

std::atomic<bool> g_running{true};

void signal_handler(int) {
    g_running = false;
}

int main() {
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    std::cout << "Starting APM System..." << std::endl;

    // Configuration
    APMSystem::Config apm_config;
    apm_config.num_microphones = 4;
    apm_config.num_speakers = 2; // Stereo output
    apm_config.sample_rate = 48000;
    
    // Initialize APM
    APMSystem apm(apm_config);
    
    // Queues
    ThreadSafeQueue<std::vector<float>> input_queue;
    ThreadSafeQueue<std::vector<float>> output_queue;
    
    // Pre-fill output queue with silence to buffer latency
    size_t frame_size = 512;
    for(int i=0; i<4; ++i) {
        output_queue.push(std::vector<float>(frame_size * apm_config.num_speakers, 0.0f));
    }
    
    // Audio Processing Thread
    std::thread processing_thread([&]() {
        std::vector<float> input_chunk;
        float target_angle = 0.0f; // Straight ahead
        
        while (g_running) {
            try {
                // Wait for input audio
                if (!input_queue.wait_and_pop(input_chunk, std::chrono::milliseconds(100))) {
                    continue;
                }

                // Deinterleave input
                size_t frames = input_chunk.size() / apm_config.num_microphones;
                std::vector<AudioFrame> mic_array;
                mic_array.reserve(apm_config.num_microphones);

                for (int ch = 0; ch < apm_config.num_microphones; ++ch) {
                    AudioFrame frame(frames, apm_config.sample_rate, 1);
                    auto samples = frame.samples();
                    for (size_t i = 0; i < frames; ++i) {
                        samples[i] = input_chunk[i * apm_config.num_microphones + ch];
                    }
                    mic_array.push_back(std::move(frame));
                }

                // Create dummy speaker reference (should be fed back from output)
                AudioFrame speaker_ref(frames, apm_config.sample_rate, 1);

                // Process
                auto output_frames = apm.process(mic_array, speaker_ref, target_angle);

                // Interleave output
                if (output_frames.empty()) {
                    // Silence
                    std::vector<float> output_chunk(frames * apm_config.num_speakers, 0.0f);
                    output_queue.push(output_chunk);
                } else {
                    std::vector<float> output_chunk(frames * apm_config.num_speakers);
                    for (size_t i = 0; i < frames; ++i) {
                        for (int ch = 0; ch < apm_config.num_speakers; ++ch) {
                            if (ch < static_cast<int>(output_frames.size())) {
                                auto samples = output_frames[ch].samples();
                                output_chunk[i * apm_config.num_speakers + ch] = samples[i];
                            } else {
                                output_chunk[i * apm_config.num_speakers + ch] = 0.0f;
                            }
                        }
                    }
                    output_queue.push(output_chunk);
                }
            } catch (const std::exception& e) {
                std::cerr << "Processing error: " << e.what() << std::endl;
            }
        }
    });

    // Audio Device
    AudioDevice::Config audio_config;
    audio_config.sample_rate = apm_config.sample_rate;
    audio_config.input_channels = apm_config.num_microphones;
    audio_config.output_channels = apm_config.num_speakers;
    audio_config.frames_per_buffer = frame_size;
    
    AudioDevice audio(audio_config);
    
    std::cout << "Available Audio Devices:\n" << AudioDevice::list_devices() << std::endl;
    
    // Set Callback
    audio.set_callback([&](const std::vector<float>& input, std::vector<float>& output) {
        // Push input to processing queue
        input_queue.push(input);
        
        // Pop output from processing queue
        std::vector<float> ready_output;
        if (output_queue.try_pop(ready_output)) {
            // Copy to output buffer
            size_t copy_size = std::min(output.size(), ready_output.size());
            std::copy(ready_output.begin(), ready_output.begin() + copy_size, output.begin());
            // Zero fill rest
            if (copy_size < output.size()) {
                std::fill(output.begin() + copy_size, output.end(), 0.0f);
            }
        } else {
            // Underrun - silence
            std::fill(output.begin(), output.end(), 0.0f);
            // std::cerr << "U" << std::flush; // Underrun indicator
        }
    });
    
    if (!audio.start()) {
        std::cerr << "Failed to start audio device!" << std::endl;
        g_running = false;
    } else {
        std::cout << "Audio started. Press Ctrl+C to stop." << std::endl;
    }
    
    // Main loop
    while (g_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    std::cout << "Stopping..." << std::endl;
    audio.stop();
    if (processing_thread.joinable()) {
        processing_thread.join();
    }
    
    return 0;
}
