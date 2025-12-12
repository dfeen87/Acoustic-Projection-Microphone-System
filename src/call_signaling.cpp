#include "apm/call_signaling.hpp"
#include <iostream>
#include <random>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <thread>
#include <atomic>

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

namespace apm {
namespace signaling {

CallSignaling::CallSignaling() = default;

CallSignaling::~CallSignaling() {
    shutdown();
}

// ============================================================================
// INITIALIZATION
// ============================================================================

bool CallSignaling::initialize(const Participant& local_participant, uint16_t listen_port) {
    if (initialized_) {
        return true;
    }
    
    local_participant_ = local_participant;
    listen_port_ = listen_port;
    
#ifdef _WIN32
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        std::cerr << "WSAStartup failed\n";
        return false;
    }
#endif
    
    // Create UDP socket
    socket_fd_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socket_fd_ == INVALID_SOCKET) {
        std::cerr << "Failed to create socket\n";
        return false;
    }
    
    // Bind to listen port
    sockaddr_in local_addr{};
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = INADDR_ANY;
    local_addr.sin_port = htons(listen_port_);
    
    if (bind(socket_fd_, (sockaddr*)&local_addr, sizeof(local_addr)) == SOCKET_ERROR) {
        std::cerr << "Failed to bind socket to port " << listen_port_ << "\n";
        closesocket(socket_fd_);
        socket_fd_ = INVALID_SOCKET;
        return false;
    }
    
    running_ = true;
    initialized_ = true;
    
    // Start listening thread
    listen_thread_ = std::thread(&CallSignaling::listen_loop, this);
    
    // Start heartbeat thread
    heartbeat_thread_ = std::thread(&CallSignaling::heartbeat_loop, this);
    
    std::cout << "Call Signaling initialized on port " << listen_port_ << "\n";
    std::cout << "Local participant: " << local_participant_.display_name 
              << " (" << local_participant_.id << ")\n";
    
    return true;
}

void CallSignaling::shutdown() {
    if (!initialized_) {
        return;
    }
    
    running_ = false;
    
    // End all active calls
    std::vector<std::string> session_ids;
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        for (const auto& [id, _] : sessions_) {
            session_ids.push_back(id);
        }
    }
    
    for (const auto& id : session_ids) {
        end_call(id);
    }
    
    // Close socket
    if (socket_fd_ != INVALID_SOCKET) {
        closesocket(socket_fd_);
        socket_fd_ = INVALID_SOCKET;
    }
    
    // Join threads
    if (listen_thread_.joinable()) {
        listen_thread_.join();
    }
    
    if (heartbeat_thread_.joinable()) {
        heartbeat_thread_.join();
    }
    
#ifdef _WIN32
    WSACleanup();
#endif
    
    initialized_ = false;
    std::cout << "Call Signaling shutdown\n";
}

// ============================================================================
// CALL CONTROL
// ============================================================================

std::string CallSignaling::initiate_call(const Participant& remote) {
    if (!initialized_) {
        std::cerr << "Call signaling not initialized\n";
        return "";
    }
    
    // Check if already in a call
    if (is_in_call()) {
        std::cerr << "Already in a call\n";
        return "";
    }
    
    // Create new session
    CallSession session;
    session.session_id = generate_session_id();
    session.caller = local_participant_;
    session.callee = remote;
    session.state = CallState::CALLING;
    session.start_time = std::chrono::steady_clock::now();
    
    // Store session
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        sessions_[session.session_id] = session;
        active_session_id_ = session.session_id;
    }
    
    // Send CALL_REQUEST signal
    SignalMessage msg;
    msg.type = SignalType::CALL_REQUEST;
    msg.session_id = session.session_id;
    msg.sender = local_participant_;
    msg.timestamp = std::chrono::steady_clock::now();
    
    send_signal(remote.ip_address, remote.port, msg);
    
    std::cout << "[CALL] Initiating call to " << remote.display_name 
              << " (session: " << session.session_id << ")\n";
    
    if (call_state_callback_) {
        call_state_callback_(session.session_id, CallState::CALLING);
    }
    
    return session.session_id;
}

bool CallSignaling::accept_call(const std::string& session_id) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    
    auto it = sessions_.find(session_id);
    if (it == sessions_.end()) {
        std::cerr << "Session not found: " << session_id << "\n";
        return false;
    }
    
    CallSession& session = it->second;
    
    if (session.state != CallState::RINGING) {
        std::cerr << "Call not in RINGING state\n";
        return false;
    }
    
    // Update state
    session.state = CallState::CONNECTED;
    active_session_id_ = session_id;
    
    // Stop ring tone
    stop_ring_tone();
    
    // Send CALL_ACCEPT signal
    SignalMessage msg;
    msg.type = SignalType::CALL_ACCEPT;
    msg.session_id = session_id;
    msg.sender = local_participant_;
    msg.timestamp = std::chrono::steady_clock::now();
    
    send_signal(session.caller.ip_address, session.caller.port, msg);
    
    std::cout << "[CALL] Accepted call from " << session.caller.display_name << "\n";
    
    if (call_state_callback_) {
        call_state_callback_(session_id, CallState::CONNECTED);
    }
    
    return true;
}

bool CallSignaling::reject_call(const std::string& session_id) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    
    auto it = sessions_.find(session_id);
    if (it == sessions_.end()) {
        return false;
    }
    
    CallSession& session = it->second;
    session.state = CallState::REJECTED;
    session.end_time = std::chrono::steady_clock::now();
    
    // Stop ring tone
    stop_ring_tone();
    
    // Send CALL_REJECT signal
    SignalMessage msg;
    msg.type = SignalType::CALL_REJECT;
    msg.session_id = session_id;
    msg.sender = local_participant_;
    msg.timestamp = std::chrono::steady_clock::now();
    
    send_signal(session.caller.ip_address, session.caller.port, msg);
    
    std::cout << "[CALL] Rejected call from " << session.caller.display_name << "\n";
    
    if (call_state_callback_) {
        call_state_callback_(session_id, CallState::REJECTED);
    }
    
    if (active_session_id_ == session_id) {
        active_session_id_.clear();
    }
    
    return true;
}

bool CallSignaling::end_call(const std::string& session_id) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    
    auto it = sessions_.find(session_id);
    if (it == sessions_.end()) {
        return false;
    }
    
    CallSession& session = it->second;
    
    if (session.state != CallState::CONNECTED && session.state != CallState::CALLING) {
        return false;
    }
    
    session.state = CallState::ENDED;
    session.end_time = std::chrono::steady_clock::now();
    
    // Send CALL_END signal to other party
    SignalMessage msg;
    msg.type = SignalType::CALL_END;
    msg.session_id = session_id;
    msg.sender = local_participant_;
    msg.timestamp = std::chrono::steady_clock::now();
    
    const Participant& other = (session.caller.id == local_participant_.id) 
                               ? session.callee : session.caller;
    
    send_signal(other.ip_address, other.port, msg);
    
    play_end_tone();
    
    std::cout << "[CALL] Ended call with " << other.display_name << "\n";
    
    if (call_state_callback_) {
        call_state_callback_(session_id, CallState::ENDED);
    }
    
    if (active_session_id_ == session_id) {
        active_session_id_.clear();
    }
    
    return true;
}

// ============================================================================
// SESSION MANAGEMENT
// ============================================================================

const CallSession* CallSignaling::get_active_session() const {
    if (active_session_id_.empty()) {
        return nullptr;
    }
    
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    auto it = sessions_.find(active_session_id_);
    return (it != sessions_.end()) ? &it->second : nullptr;
}

const CallSession* CallSignaling::get_session(const std::string& session_id) const {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    auto it = sessions_.find(session_id);
    return (it != sessions_.end()) ? &it->second : nullptr;
}

std::vector<CallSession> CallSignaling::get_all_sessions() const {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    std::vector<CallSession> result;
    for (const auto& [_, session] : sessions_) {
        result.push_back(session);
    }
    return result;
}

bool CallSignaling::is_in_call() const {
    return !active_session_id_.empty();
}

// ============================================================================
// DISCOVERY
// ============================================================================

void CallSignaling::enable_discovery(bool enable) {
    discovery_enabled_ = enable;
    std::cout << "[DISCOVERY] " << (enable ? "Enabled" : "Disabled") << "\n";
}

std::vector<Participant> CallSignaling::get_discovered_peers() const {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    return discovered_peers_;
}

void CallSignaling::add_peer(const Participant& peer) {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    discovered_peers_.push_back(peer);
    std::cout << "[PEER] Added: " << peer.display_name << " (" << peer.ip_address << ")\n";
}

void CallSignaling::remove_peer(const std::string& peer_id) {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    discovered_peers_.erase(
        std::remove_if(discovered_peers_.begin(), discovered_peers_.end(),
            [&](const Participant& p) { return p.id == peer_id; }),
        discovered_peers_.end()
    );
}

// ============================================================================
// AUDIO CONTROL
// ============================================================================

void CallSignaling::play_ring_tone() {
    if (ring_playing_) {
        return;
    }
    
    ring_playing_ = true;
    
    std::cout << "[AUDIO] Playing ring tone (" << ring_frequency_ << "Hz)\n";
    
    // In real implementation, would generate and play audio
    // This is a placeholder
}

void CallSignaling::stop_ring_tone() {
    if (!ring_playing_) {
        return;
    }
    
    ring_playing_ = false;
    std::cout << "[AUDIO] Stopped ring tone\n";
}

void CallSignaling::play_busy_tone() {
    std::cout << "[AUDIO] Playing busy tone\n";
}

void CallSignaling::play_end_tone() {
    std::cout << "[AUDIO] Playing end tone\n";
}

// ============================================================================
// PRIVATE METHODS
// ============================================================================

void CallSignaling::listen_loop() {
    std::cout << "[LISTEN] Started listening thread\n";
    
    char buffer[4096];
    sockaddr_in sender_addr{};
    socklen_t addr_len = sizeof(sender_addr);
    
    while (running_) {
        // Set timeout for recv
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(socket_fd_, &read_fds);
        
        timeval timeout{};
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        int result = select(socket_fd_ + 1, &read_fds, nullptr, nullptr, &timeout);
        
        if (result > 0) {
            int bytes = recvfrom(socket_fd_, buffer, sizeof(buffer), 0,
                               (sockaddr*)&sender_addr, &addr_len);
            
            if (bytes > 0) {
                // Parse and process signal (simplified)
                std::cout << "[SIGNAL] Received " << bytes << " bytes\n";
                // In real implementation, would deserialize and process
            }
        }
    }
    
    std::cout << "[LISTEN] Stopped listening thread\n";
}

void CallSignaling::heartbeat_loop() {
    std::cout << "[HEARTBEAT] Started heartbeat thread\n";
    
    while (running_) {
        std::this_thread::sleep_for(std::chrono::seconds(heartbeat_interval_));
        
        if (!is_in_call()) {
            continue;
        }
        
        // Send heartbeat to active call participant
        auto session = get_active_session();
        if (session && session->state == CallState::CONNECTED) {
            SignalMessage msg;
            msg.type = SignalType::HEARTBEAT;
            msg.session_id = session->session_id;
            msg.sender = local_participant_;
            msg.timestamp = std::chrono::steady_clock::now();
            
            const Participant& other = (session->caller.id == local_participant_.id)
                                      ? session->callee : session->caller;
            
            send_signal(other.ip_address, other.port, msg);
        }
        
        cleanup_old_sessions();
    }
    
    std::cout << "[HEARTBEAT] Stopped heartbeat thread\n";
}

void CallSignaling::send_signal(const std::string& target_ip, uint16_t target_port, 
                                const SignalMessage& msg) {
    sockaddr_in target_addr{};
    target_addr.sin_family = AF_INET;
    target_addr.sin_port = htons(target_port);
    inet_pton(AF_INET, target_ip.c_str(), &target_addr.sin_addr);
    
    // Simplified serialization (in real implementation, use proper protocol)
    std::string data = "SIGNAL:" + std::to_string(static_cast<int>(msg.type)) 
                     + ":" + msg.session_id;
    
    sendto(socket_fd_, data.c_str(), data.length(), 0,
          (sockaddr*)&target_addr, sizeof(target_addr));
    
    std::cout << "[SIGNAL] Sent " << static_cast<int>(msg.type) 
              << " to " << target_ip << ":" << target_port << "\n";
}

std::string CallSignaling::generate_session_id() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);
    
    std::stringstream ss;
    ss << std::hex;
    for (int i = 0; i < 32; i++) {
        ss << dis(gen);
    }
    
    return ss.str();
}

void CallSignaling::cleanup_old_sessions() {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    
    auto now = std::chrono::steady_clock::now();
    
    for (auto it = sessions_.begin(); it != sessions_.end();) {
        const auto& session = it->second;
        
        if (session.state == CallState::ENDED || 
            session.state == CallState::REJECTED ||
            session.state == CallState::TIMEOUT) {
            
            auto age = std::chrono::duration_cast<std::chrono::minutes>(
                now - session.end_time
            ).count();
            
            if (age > 5) { // Remove sessions older than 5 minutes
                it = sessions_.erase(it);
                continue;
            }
        }
        
        ++it;
    }
}

} // namespace signaling
} // namespace apm
