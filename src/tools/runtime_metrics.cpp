#include "apm_observability.hpp"
#include <atomic>
#include <chrono>
#include <algorithm>

namespace apm {

class DefaultObservability final : public Observability {
public:
    DefaultObservability() noexcept 
        : start_time_(now_us())
        , frames_processed_(0)
        , frames_dropped_(0)
        , signaling_events_(0)
        , health_status_(HealthStatus::OK)
        , error_count_(0)
    {}
    
    // Observability interface implementation
    HealthStatus health() const noexcept override {
        return health_status_.load(std::memory_order_acquire);
    }
    
    RuntimeMetrics metrics() const noexcept override {
        // Atomic snapshot with consistent memory ordering
        RuntimeMetrics m;
        m.frames_processed = frames_processed_.load(std::memory_order_relaxed);
        m.frames_dropped = frames_dropped_.load(std::memory_order_relaxed);
        m.signaling_events = signaling_events_.load(std::memory_order_relaxed);
        m.timestamp_us = now_us();
        m.uptime_ms = (m.timestamp_us - start_time_) / 1000;
        
        return m;
    }
    
    MetricsSnapshot snapshot() const noexcept override {
        MetricsSnapshot snap;
        
        // Single atomic read for health
        snap.health = health_status_.load(std::memory_order_acquire);
        
        // Consistent metrics snapshot
        snap.metrics = metrics();
        snap.valid = true;
        
        return snap;
    }
    
    std::string_view health_message() const noexcept override {
        return health_msg_;
    }
    
    bool is_enabled() const noexcept override {
        return enabled_.load(std::memory_order_relaxed);
    }
    
    void reset_metrics() noexcept override {
        frames_processed_.store(0, std::memory_order_relaxed);
        frames_dropped_.store(0, std::memory_order_relaxed);
        signaling_events_.store(0, std::memory_order_relaxed);
        error_count_.store(0, std::memory_order_relaxed);
        start_time_ = now_us();
        update_health(HealthStatus::OK, "Reset");
    }
    
    // Metric increment operations with overflow protection
    void inc_processed() noexcept {
        if (!enabled_.load(std::memory_order_relaxed)) return;
        
        uint64_t prev = frames_processed_.fetch_add(1, std::memory_order_relaxed);
        
        // Check for overflow (saturate at max)
        if (prev == UINT64_MAX) {
            frames_processed_.store(UINT64_MAX, std::memory_order_relaxed);
        }
    }
    
    void inc_dropped() noexcept {
        if (!enabled_.load(std::memory_order_relaxed)) return;
        
        uint64_t prev = frames_dropped_.fetch_add(1, std::memory_order_relaxed);
        
        // Check for overflow
        if (prev == UINT64_MAX) {
            frames_dropped_.store(UINT64_MAX, std::memory_order_relaxed);
        }
        
        // Update health based on drop rate
        check_drop_rate();
    }
    
    void inc_signal() noexcept {
        if (!enabled_.load(std::memory_order_relaxed)) return;
        
        uint64_t prev = signaling_events_.fetch_add(1, std::memory_order_relaxed);
        
        // Check for overflow
        if (prev == UINT64_MAX) {
            signaling_events_.store(UINT64_MAX, std::memory_order_relaxed);
        }
    }
    
    // Batch increment for efficiency (lock-free)
    void add_processed(uint64_t count) noexcept {
        if (!enabled_.load(std::memory_order_relaxed) || count == 0) return;
        
        uint64_t prev = frames_processed_.fetch_add(count, std::memory_order_relaxed);
        
        // Saturate on overflow
        if (prev > UINT64_MAX - count) {
            frames_processed_.store(UINT64_MAX, std::memory_order_relaxed);
        }
    }
    
    void add_dropped(uint64_t count) noexcept {
        if (!enabled_.load(std::memory_order_relaxed) || count == 0) return;
        
        uint64_t prev = frames_dropped_.fetch_add(count, std::memory_order_relaxed);
        
        // Saturate on overflow
        if (prev > UINT64_MAX - count) {
            frames_dropped_.store(UINT64_MAX, std::memory_order_relaxed);
        }
        
        check_drop_rate();
    }
    
    // Health management
    void report_error() noexcept {
        error_count_.fetch_add(1, std::memory_order_relaxed);
        update_health(HealthStatus::ERROR, "Error reported");
    }
    
    void report_degraded(std::string_view reason = "") noexcept {
        update_health(HealthStatus::DEGRADED, reason);
    }
    
    void report_healthy() noexcept {
        error_count_.store(0, std::memory_order_relaxed);
        update_health(HealthStatus::OK, "System healthy");
    }
    
    // Configuration
    void set_enabled(bool enabled) noexcept {
        enabled_.store(enabled, std::memory_order_relaxed);
    }
    
    void set_drop_rate_thresholds(double degraded, double error) noexcept {
        // Clamp to valid range [0.0, 1.0]
        drop_rate_degraded_ = std::clamp(degraded, 0.0, 1.0);
        drop_rate_error_ = std::clamp(error, 0.0, 1.0);
        
        // Ensure error threshold >= degraded threshold
        if (drop_rate_error_ < drop_rate_degraded_) {
            drop_rate_error_ = drop_rate_degraded_;
        }
    }
    
    // Query operations
    uint64_t get_error_count() const noexcept {
        return error_count_.load(std::memory_order_relaxed);
    }
    
    double get_drop_rate() const noexcept {
        uint64_t processed = frames_processed_.load(std::memory_order_relaxed);
        uint64_t dropped = frames_dropped_.load(std::memory_order_relaxed);
        
        if (processed == 0) return 0.0;
        return static_cast<double>(dropped) / static_cast<double>(processed);
    }

private:
    // Atomics with appropriate alignment for cache efficiency
    alignas(64) std::atomic<uint64_t> frames_processed_;
    alignas(64) std::atomic<uint64_t> frames_dropped_;
    alignas(64) std::atomic<uint64_t> signaling_events_;
    alignas(64) std::atomic<HealthStatus> health_status_;
    alignas(64) std::atomic<uint64_t> error_count_;
    alignas(64) std::atomic<bool> enabled_{true};
    
    // Non-atomic fields (read-only after construction or rarely modified)
    uint64_t start_time_;
    double drop_rate_degraded_ = 0.05;  // 5% drops = degraded
    double drop_rate_error_ = 0.15;     // 15% drops = error
    
    // Health message buffer (not thread-safe, but best-effort)
    char health_msg_[128] = "OK";
    
    void update_health(HealthStatus new_status, std::string_view message) noexcept {
        health_status_.store(new_status, std::memory_order_release);
        
        // Best-effort message update (not critical if racy)
        size_t len = std::min(message.size(), sizeof(health_msg_) - 1);
        std::copy_n(message.data(), len, health_msg_);
        health_msg_[len] = '\0';
    }
    
    void check_drop_rate() noexcept {
        // Only check periodically to avoid overhead
        static thread_local uint64_t check_counter = 0;
        if (++check_counter % 1000 != 0) return;
        
        double rate = get_drop_rate();
        
        if (rate >= drop_rate_error_) {
            update_health(HealthStatus::ERROR, "High drop rate");
        } else if (rate >= drop_rate_degraded_) {
            update_health(HealthStatus::DEGRADED, "Elevated drop rate");
        } else if (health_status_.load(std::memory_order_relaxed) != HealthStatus::OK) {
            // Auto-recover if drop rate improves
            update_health(HealthStatus::OK, "Drop rate recovered");
        }
    }
};

} // namespace apm
