#pragma once

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <chrono>
#include <map>

namespace apm {
namespace signaling {

/**
 * @brief Call state
 */
enum class CallState {
    IDLE,           // No active call
    RINGING,        // Incoming call ringing
    CALLING,        // Outgoing call (waiting for answer)
    CONNECTED,      // Call in progress
    ENDED,          // Call ended
    REJECTED,       // Call rejected
    TIMEOUT,        // Call timed out
    ERROR           // Error occurred
};

/**
 * @brief Signal types
 */
enum class SignalType {
    CALL_REQUEST,   // Initial call request
    CALL_ACCEPT,    // Accept incoming call
    CALL_REJECT,    // Reject incoming call
    CALL_END,       // End active call
    HEARTBEAT,      // Keep-alive signal
    KEY_EXCHANGE    // Encryption key exchange
};

/**
 * @brief Call participant information
 */
struct Participant {
    std::string id;                     // Unique identifier
    std::string display_name;           // Display name
    std::string ip_address;             // IP address
    uint16_t port;                      // Port number
    std::vector<uint8_t> public_key;    // Encryption public key
    std::string source_language;        // Preferred language
    std::string target_language;        // Translation target
};

/**
 * @brief Call session information
 */
struct CallSession {
    std::string session_id;             // Unique session ID
    Participant caller;                 // Call initiator
    Participant callee;                 // Call recipient
    std::vector<uint8_t> session_key;   // Shared encryption key
    CallState state;                    // Current call state
    std::chrono::steady_clock::time_point start_time;
    std::chrono::steady_clock::time_point end_time;
};

/**
 * @brief Call signaling protocol
 * 
 * Handles call setup, teardown, and control signaling
 * Uses UDP for signaling, supports encryption key exchange
 */
class CallSignaling {
public:
    /**
     * @brief Callback function types
     */
    using IncomingCallCallback = std::function<void(const CallSession&)>;
    using CallStateCallback = std::function<void(const std::string& session_id, CallState state)>;
    using ErrorCallback = std::function<void(const std::string& error)>;
    
    CallSignaling();
    ~CallSignaling();
    
    // Disable copy, enable move
    CallSignaling(const CallSignaling&) = delete;
    CallSignaling& operator=(const CallSignaling&) = delete;
    CallSignaling(CallSignaling&&) = default;
    CallSignaling& operator=(CallSignaling&&) = default;
    
    // ========================================================================
    // INITIALIZATION
    // ========================================================================
    
    /**
     * @brief Initialize signaling system
     * @param local_participant This user's information
     * @param listen_port Port to listen on (default: 5060)
     * @return true if initialized successfully
     */
    bool initialize(const Participant& local_participant, uint16_t listen_port = 5060);
    
    /**
     * @brief Shutdown signaling system
     */
    void shutdown();
    
    /**
     * @brief Check if initialized
     */
    bool is_initialized() const { return initialized_; }
    
    // ========================================================================
    // CALL CONTROL
    // ========================================================================
    
    /**
     * @brief Initiate a call to remote participant
     * @param remote Remote participant information
     * @return Session ID if successful, empty string on failure
     */
    std::string initiate_call(const Participant& remote);
    
    /**
     * @brief Accept an incoming call
     * @param session_id Session ID of the incoming call
     * @return true if accepted successfully
     */
    bool accept_call(const std::string& session_id);
    
    /**
     * @brief Reject an incoming call
     * @param session_id Session ID of the incoming call
     * @return true if rejected successfully
     */
    bool reject_call(const std::string& session_id);
    
    /**
     * @brief End an active call
     * @param session_id Session ID of the call to end
     * @return true if ended successfully
     */
    bool end_call(const std::string& session_id);
    
    // ========================================================================
    // SESSION MANAGEMENT
    // ========================================================================
    
    /**
     * @brief Get active call session
     * @return Pointer to session or nullptr if no active call
     */
    const CallSession* get_active_session() const;
    
    /**
     * @brief Get session by ID
     * @param session_id Session ID
     * @return Pointer to session or nullptr if not found
     */
    const CallSession* get_session(const std::string& session_id) const;
    
    /**
     * @brief Get all sessions
     */
    std::vector<CallSession> get_all_sessions() const;
    
    /**
     * @brief Check if currently in a call
     */
    bool is_in_call() const;
    
    // ========================================================================
    // DISCOVERY
    // ========================================================================
    
    /**
     * @brief Enable peer discovery on local network
     * @param enable True to enable, false to disable
     */
    void enable_discovery(bool enable);
    
    /**
     * @brief Get discovered peers on network
     */
    std::vector<Participant> get_discovered_peers() const;
    
    /**
     * @brief Manually add a peer
     */
    void add_peer(const Participant& peer);
    
    /**
     * @brief Remove a peer
     */
    void remove_peer(const std::string& peer_id);
    
    // ========================================================================
    // CALLBACKS
    // ========================================================================
    
    /**
     * @brief Set callback for incoming calls
     */
    void on_incoming_call(IncomingCallCallback callback) {
        incoming_call_callback_ = callback;
    }
    
    /**
     * @brief Set callback for call state changes
     */
    void on_call_state_changed(CallStateCallback callback) {
        call_state_callback_ = callback;
    }
    
    /**
     * @brief Set callback for errors
     */
    void on_error(ErrorCallback callback) {
        error_callback_ = callback;
    }
    
    // ========================================================================
    // CONFIGURATION
    // ========================================================================
    
    /**
     * @brief Set call timeout (seconds)
     */
    void set_call_timeout(int seconds) { call_timeout_seconds_ = seconds; }
    
    /**
     * @brief Set heartbeat interval (seconds)
     */
    void set_heartbeat_interval(int seconds) { heartbeat_interval_ = seconds; }
    
    /**
     * @brief Enable/disable ring tone
     */
    void set_ring_enabled(bool enabled) { ring_enabled_ = enabled; }
    
    /**
     * @brief Set ring tone frequency (Hz)
     */
    void set_ring_frequency(int hz) { ring_frequency_ = hz; }
    
    // ========================================================================
    // AUDIO CONTROL
    // ========================================================================
    
    /**
     * @brief Play ring tone
     */
    void play_ring_tone();
    
    /**
     * @brief Stop ring tone
     */
    void stop_ring_tone();
    
    /**
     * @brief Play busy tone
     */
    void play_busy_tone();
    
    /**
     * @brief Play call ended tone
     */
    void play_end_tone();

private:
    struct SignalMessage {
        SignalType type;
        std::string session_id;
        Participant sender;
        std::vector<uint8_t> data;
        std::chrono::steady_clock::time_point timestamp;
    };
    
    void listen_loop();
    void heartbeat_loop();
    void process_signal(const SignalMessage& msg);
    void send_signal(const std::string& target_ip, uint16_t target_port, const SignalMessage& msg);
    std::string generate_session_id();
    void update_session_state(const std::string& session_id, CallState new_state);
    void cleanup_old_sessions();
    
    bool initialized_{false};
    bool running_{false};
    
    Participant local_participant_;
    uint16_t listen_port_{5060};
    int socket_fd_{-1};
    
    // Sessions
    std::map<std::string, CallSession> sessions_;
    mutable std::mutex sessions_mutex_;
    std::string active_session_id_;
    
    // Discovery
    bool discovery_enabled_{false};
    std::vector<Participant> discovered_peers_;
    mutable std::mutex peers_mutex_;
    
    // Configuration
    int call_timeout_seconds_{30};
    int heartbeat_interval_{5};
    bool ring_enabled_{true};
    int ring_frequency_{440};
    
    // Callbacks
    IncomingCallCallback incoming_call_callback_;
    CallStateCallback call_state_callback_;
    ErrorCallback error_callback_;
    
    // Threading
    std::thread listen_thread_;
    std::thread heartbeat_thread_;
    std::atomic<bool> ring_playing_{false};
};

/**
 * @brief Helper function to convert CallState to string
 */
inline const char* call_state_to_string(CallState state) {
    switch (state) {
        case CallState::IDLE: return "IDLE";
        case CallState::RINGING: return "RINGING";
        case CallState::CALLING: return "CALLING";
        case CallState::CONNECTED: return "CONNECTED";
        case CallState::ENDED: return "ENDED";
        case CallState::REJECTED: return "REJECTED";
        case CallState::TIMEOUT: return "TIMEOUT";
        case CallState::ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

} // namespace signaling
} // namespace apm
