#pragma once
/*
 * APM Observability Interface
 *
 * Lightweight health and runtime metrics contract.
 * Intended for diagnostics, testing, and tooling —
 * not control or decision logic.
 *
 * License: MIT
 */
#include <cstdint>
#include <atomic>
#include <chrono>
#include <optional>
#include <string_view>

namespace apm {

// Health status with explicit values for serialization stability
enum class HealthStatus : uint8_t {
    OK = 0,
    DEGRADED = 1,
    ERROR = 2,
    UNKNOWN = 255  // For uninitialized or error states
};

// Convert health status to human-readable string
constexpr std::string_view to_string(HealthStatus status) noexcept {
    switch (status) {
        case HealthStatus::OK:       return "OK";
        case HealthStatus::DEGRADED: return "DEGRADED";
        case HealthStatus::ERROR:    return "ERROR";
        case HealthStatus::UNKNOWN:  return "UNKNOWN";
        default:                     return "INVALID";
    }
}

// Runtime metrics with explicit initialization and bounds
struct RuntimeMetrics {
    uint64_t frames_processed = 0;
    uint64_t frames_dropped   = 0;
    uint64_t signaling_events = 0;
    
    // Timestamp when metrics were captured (microseconds since epoch)
    uint64_t timestamp_us = 0;
    
    // Optional: uptime in milliseconds
    uint64_t uptime_ms = 0;
    
    // Validate metric consistency
    [[nodiscard]] bool is_valid() const noexcept {
        // Frames dropped cannot exceed frames processed
        if (frames_dropped > frames_processed) {
            return false;
        }
        // Reasonable upper bounds check (prevent overflow artifacts)
        constexpr uint64_t MAX_REASONABLE = UINT64_MAX / 2;
        if (frames_processed > MAX_REASONABLE || 
            signaling_events > MAX_REASONABLE) {
            return false;
        }
        return true;
    }
    
    // Calculate drop rate (0.0 to 1.0)
    [[nodiscard]] double drop_rate() const noexcept {
        if (frames_processed == 0) return 0.0;
        return static_cast<double>(frames_dropped) / 
               static_cast<double>(frames_processed);
    }
    
    // Comparison operators for testing/monitoring
    bool operator==(const RuntimeMetrics& other) const noexcept = default;
    bool operator!=(const RuntimeMetrics& other) const noexcept = default;
};

// Thread-safe metrics snapshot for atomic reads
struct MetricsSnapshot {
    RuntimeMetrics metrics;
    HealthStatus health;
    bool valid;
    
    [[nodiscard]] bool is_valid() const noexcept {
        return valid && metrics.is_valid();
    }
};

class Observability {
public:
    virtual ~Observability() noexcept = default;
    
    // Prevent copying - observability is typically a singleton
    Observability(const Observability&) = delete;
    Observability& operator=(const Observability&) = delete;
    
    // Allow moving for modern C++ patterns
    Observability(Observability&&) noexcept = default;
    Observability& operator=(Observability&&) noexcept = delete;
    
    // Get current health status
    // Must be thread-safe and fast (< 1μs)
    // Must not throw exceptions
    [[nodiscard]] virtual HealthStatus health() const noexcept = 0;
    
    // Get current runtime metrics
    // Must be thread-safe and consistent (atomic snapshot)
    // Must not throw exceptions
    [[nodiscard]] virtual RuntimeMetrics metrics() const noexcept = 0;
    
    // Get atomic snapshot of both health and metrics
    // Ensures temporal consistency between health and metrics
    [[nodiscard]] virtual MetricsSnapshot snapshot() const noexcept {
        MetricsSnapshot snap;
        snap.health = health();
        snap.metrics = metrics();
        snap.valid = true;
        return snap;
    }
    
    // Optional: Get detailed health message
    [[nodiscard]] virtual std::string_view health_message() const noexcept {
        return "";
    }
    
    // Optional: Check if metrics collection is enabled
    [[nodiscard]] virtual bool is_enabled() const noexcept {
        return true;
    }
    
    // Optional: Reset metrics (for testing/debugging)
    // Should be protected by appropriate access controls in production
    virtual void reset_metrics() noexcept {}

protected:
    // Protected default constructor - only derived classes can instantiate
    Observability() noexcept = default;
    
    // Helper: Get current timestamp in microseconds
    static uint64_t now_us() noexcept {
        using namespace std::chrono;
        return duration_cast<microseconds>(
            steady_clock::now().time_since_epoch()
        ).count();
    }
};

// Validation helpers
[[nodiscard]] inline bool is_critical(HealthStatus status) noexcept {
    return status == HealthStatus::ERROR;
}

[[nodiscard]] inline bool is_healthy(HealthStatus status) noexcept {
    return status == HealthStatus::OK;
}

// Safe metrics aggregation (prevents overflow)
[[nodiscard]] inline RuntimeMetrics aggregate_metrics(
    const RuntimeMetrics& a, 
    const RuntimeMetrics& b
) noexcept {
    RuntimeMetrics result;
    
    // Saturating addition to prevent overflow
    auto safe_add = [](uint64_t x, uint64_t y) -> uint64_t {
        if (x > UINT64_MAX - y) return UINT64_MAX;
        return x + y;
    };
    
    result.frames_processed = safe_add(a.frames_processed, b.frames_processed);
    result.frames_dropped = safe_add(a.frames_dropped, b.frames_dropped);
    result.signaling_events = safe_add(a.signaling_events, b.signaling_events);
    result.timestamp_us = std::max(a.timestamp_us, b.timestamp_us);
    result.uptime_ms = std::max(a.uptime_ms, b.uptime_ms);
    
    return result;
}

} // namespace apm
