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
#include <iomanip>
#include <sstream>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #define SOCKET int
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
    #define closesocket close
#endif

using namespace apm;

std::string serialize_metrics(const apm::MonitoringMetrics& metrics) {
    std::ostringstream json;
    json << "{\"peak_db\":" << metrics.peak_db
         << ",\"rms_db\":" << metrics.rms_db
         << ",\"snr_db\":" << metrics.snr_db
         << ",\"clipping\":" << (metrics.clipping ? "true" : "false")
         << ",\"latency_ms\":" << metrics.latency_ms << "}\n";
    return json.str();
}

class TelemetryServer {
    SOCKET server_socket_{INVALID_SOCKET};
    SOCKET client_socket_{INVALID_SOCKET};
    std::mutex client_mutex_;
    std::atomic<bool> running_{false};
    std::thread accept_thread_;

    void accept_loop() {
        while (running_) {
            fd_set read_fds;
            FD_ZERO(&read_fds);
            FD_SET(server_socket_, &read_fds);

            struct timeval tv;
            tv.tv_sec = 0;
            tv.tv_usec = 100000; // 100ms timeout

            int select_res = select(server_socket_ + 1, &read_fds, nullptr, nullptr, &tv);
            if (select_res > 0 && FD_ISSET(server_socket_, &read_fds)) {
                struct sockaddr_in client_addr;
                socklen_t client_len = sizeof(client_addr);
                SOCKET new_client = accept(server_socket_, (struct sockaddr*)&client_addr, &client_len);

                if (new_client != INVALID_SOCKET) {
                    std::lock_guard<std::mutex> lock(client_mutex_);
                    // Close existing connection if any (only support one telemetry listener)
                    if (client_socket_ != INVALID_SOCKET) {
                        closesocket(client_socket_);
                    }
                    client_socket_ = new_client;
                }
            }
        }
    }

public:
    TelemetryServer() = default;

    ~TelemetryServer() {
        stop();
    }

    bool start(int port = 50055) {
#ifdef _WIN32
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return false;
#endif

        server_socket_ = socket(AF_INET, SOCK_STREAM, 0);
        if (server_socket_ == INVALID_SOCKET) return false;

        int opt = 1;
        setsockopt(server_socket_, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

        struct sockaddr_in server_addr;
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = INADDR_ANY;
        server_addr.sin_port = htons(port);

        if (bind(server_socket_, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
            closesocket(server_socket_);
            return false;
        }

        if (listen(server_socket_, 1) == SOCKET_ERROR) {
            closesocket(server_socket_);
            return false;
        }

        running_ = true;
        accept_thread_ = std::thread(&TelemetryServer::accept_loop, this);
        return true;
    }

    void stop() {
        running_ = false;
        if (accept_thread_.joinable()) {
            accept_thread_.join();
        }
        {
            std::lock_guard<std::mutex> lock(client_mutex_);
            if (client_socket_ != INVALID_SOCKET) {
                closesocket(client_socket_);
                client_socket_ = INVALID_SOCKET;
            }
        }
        if (server_socket_ != INVALID_SOCKET) {
            closesocket(server_socket_);
            server_socket_ = INVALID_SOCKET;
        }
#ifdef _WIN32
        WSACleanup();
#endif
    }

    bool send_metrics(const apm::MonitoringMetrics& metrics) {
        std::lock_guard<std::mutex> lock(client_mutex_);
        if (client_socket_ == INVALID_SOCKET) return false;

        std::string payload = serialize_metrics(metrics);
#ifdef _WIN32
        int flags = 0;
#else
        int flags = MSG_NOSIGNAL; // Ignore SIGPIPE on Linux
#endif
        int sent = send(client_socket_, payload.c_str(), payload.length(), flags);
        if (sent == SOCKET_ERROR) {
            closesocket(client_socket_);
            client_socket_ = INVALID_SOCKET;
            return false;
        }
        return true;
    }
};

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
    
    // Run diagnostics
    auto health = DiagnosticsEngine::run_startup_checks(apm_config.sample_rate, apm_config.num_microphones);
    if (!health.ok) {
        std::cerr << "Diagnostics failed: " << health.message << std::endl;
        // In a real app we might exit or prompt, but here we'll just log
    } else {
        std::cout << "Diagnostics passed. " << health.message << std::endl;
    }

    // Initialize APM
    APMSystem apm(apm_config);
    
    // Load default profile
    auto profile = apm.get_profile_manager().load_profile("config/default_profile.cfg");
    if (profile) {
        apm.get_profile_manager().add_profile(*profile);
        apm.get_profile_manager().set_active_profile(profile->name);
        std::cout << "Loaded profile: " << profile->name << std::endl;
    } else {
        std::cout << "Using default generated profile." << std::endl;
    }

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
    
    // Start telemetry server
    TelemetryServer telemetry;
    if (telemetry.start(50055)) {
        std::cout << "Telemetry server started on port 50055." << std::endl;
    } else {
        std::cerr << "Failed to start telemetry server." << std::endl;
    }

    // Main loop
    int print_counter = 0;
    while (g_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20)); // ~50Hz update rate

        auto metrics = apm.get_monitoring_metrics();
        telemetry.send_metrics(metrics);

        print_counter++;
        if (print_counter >= 100) { // Every 2 seconds (100 * 20ms)
            std::cout << "\r[Monitoring] Peak: " << std::fixed << std::setprecision(1) << metrics.peak_db
                      << "dB, SNR: " << metrics.snr_db << "dB, Latency: " << metrics.latency_ms << "ms     "
                      << std::flush;
            print_counter = 0;
        }
    }
    std::cout << "\nStopping..." << std::endl;
    telemetry.stop();
    audio.stop();
    if (processing_thread.joinable()) {
        processing_thread.join();
    }
    
    return 0;
}
