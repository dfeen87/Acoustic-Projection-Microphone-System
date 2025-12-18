#pragma once
/*
 * APM Extension Interface
 *
 * Defines safe, optional extension hooks for the
 * Acoustic Projection Microphone System.
 *
 * Extensions must not modify core timing,
 * signal integrity, or safety guarantees.
 *
 * License: MIT
 */
#include <string>
#include <stdexcept>
#include <string_view>

namespace apm {

// Exception type for extension-related errors
class ExtensionError : public std::runtime_error {
public:
    explicit ExtensionError(const std::string& msg) 
        : std::runtime_error(msg) {}
};

class Extension {
public:
    virtual ~Extension() noexcept = default;
    
    // Prevent copying to avoid double-registration issues
    Extension(const Extension&) = delete;
    Extension& operator=(const Extension&) = delete;
    
    // Allow moving for modern C++ patterns
    Extension(Extension&&) noexcept = default;
    Extension& operator=(Extension&&) noexcept = delete;
    
    // Called once during system startup
    // Must be idempotent - safe to call multiple times
    // Throw ExtensionError on failure
    virtual void on_initialize() {}
    
    // Called before audio/signaling processing begins
    // Must complete quickly (< 100ms recommended)
    // Throw ExtensionError on failure
    virtual void on_pre_run() {}
    
    // Called after system shutdown
    // Must be noexcept - cannot fail during cleanup
    // Log errors instead of throwing
    virtual void on_shutdown() noexcept {}
    
    // Human-readable identifier for diagnostics
    // Must return valid, non-empty string
    // Should be thread-safe (return constant or thread-local)
    virtual std::string_view name() const noexcept = 0;
    
    // Optional: Version info for compatibility checking
    virtual int version() const noexcept { return 1; }
    
    // Optional: Health check for runtime monitoring
    virtual bool is_healthy() const noexcept { return true; }

protected:
    // Protected default constructor - only derived classes can instantiate
    Extension() noexcept = default;
};

// Validation helper
inline void validate_extension(const Extension* ext) {
    if (!ext) {
        throw ExtensionError("Null extension pointer");
    }
    if (ext->name().empty()) {
        throw ExtensionError("Extension name cannot be empty");
    }
}

} // namespace apm
