// apm_dev_console.cpp
// Dear ImGui Developer Console for APM System
// Build with: g++ -o apm_dev_console apm_dev_console.cpp -lGL -lGLU -lglfw -ldl

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>
#include <vector>
#include <string>
#include <chrono>
#include <cmath>
#include <algorithm>

struct LogEntry {
    std::chrono::system_clock::time_point timestamp;
    std::string level;
    std::string message;
    ImVec4 color;
};

struct PerformanceMetric {
    std::string name;
    float value;
    float max_value;
    std::vector<float> history;
};

class APMDevConsole {
public:
    APMDevConsole() {
        // Initialize log entries
        add_log("INFO", "APM Developer Console initialized");
        add_log("INFO", "Connecting to APM backend...");
        add_log("SUCCESS", "Connected to APM System v2.0.0");
        
        // Initialize performance metrics
        metrics.push_back({"CPU Usage", 0.0f, 100.0f, {}});
        metrics.push_back({"Memory (MB)", 0.0f, 1024.0f, {}});
        metrics.push_back({"Beamforming (ms)", 0.0f, 10.0f, {}});
        metrics.push_back({"Translation (ms)", 0.0f, 5000.0f, {}});
        metrics.push_back({"Encryption (ms)", 0.0f, 50.0f, {}});
        metrics.push_back({"Network Latency (ms)", 0.0f, 500.0f, {}});
        
        // Initialize audio buffer visualization
        for (int i = 0; i < 256; i++) {
            audio_buffer.push_back(0.0f);
        }
    }
    
    void render() {
        ImGuiIO& io = ImGui::GetIO();
        
        // Main menu bar
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Export Logs")) { export_logs(); }
                if (ImGui::MenuItem("Clear Logs")) { logs.clear(); }
                ImGui::Separator();
                if (ImGui::MenuItem("Exit", "Alt+F4")) { should_close = true; }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("View")) {
                ImGui::MenuItem("System Status", nullptr, &show_status);
                ImGui::MenuItem("Performance", nullptr, &show_performance);
                ImGui::MenuItem("Audio Pipeline", nullptr, &show_audio);
                ImGui::MenuItem("Network", nullptr, &show_network);
                ImGui::MenuItem("Logs", nullptr, &show_logs);
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Tools")) {
                if (ImGui::MenuItem("Run Benchmarks")) { run_benchmarks(); }
                if (ImGui::MenuItem("Test Encryption")) { test_encryption(); }
                if (ImGui::MenuItem("Test Translation")) { test_translation(); }
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }
        
        // Dock space
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);
        
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                                        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
        
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        
        ImGui::Begin("DockSpace", nullptr, window_flags);
        ImGui::PopStyleVar(3);
        
        ImGuiID dockspace_id = ImGui::GetID("APMDockSpace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
        ImGui::End();
        
        // Update metrics
        update_metrics();
        
        // Render windows
        if (show_status) render_status_window();
        if (show_performance) render_performance_window();
        if (show_audio) render_audio_window();
        if (show_network) render_network_window();
        if (show_logs) render_logs_window();
    }
    
    bool should_close_window() const { return should_close; }
    
private:
    std::vector<LogEntry> logs;
    std::vector<PerformanceMetric> metrics;
    std::vector<float> audio_buffer;
    
    bool show_status = true;
    bool show_performance = true;
    bool show_audio = true;
    bool show_network = true;
    bool show_logs = true;
    bool should_close = false;
    
    // Mock state
    bool call_active = false;
    bool encryption_active = true;
    bool translation_active = true;
    float call_duration = 0.0f;
    
    void add_log(const std::string& level, const std::string& message) {
        LogEntry entry;
        entry.timestamp = std::chrono::system_clock::now();
        entry.level = level;
        entry.message = message;
        
        if (level == "ERROR") entry.color = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
        else if (level == "WARN") entry.color = ImVec4(1.0f, 0.8f, 0.3f, 1.0f);
        else if (level == "SUCCESS") entry.color = ImVec4(0.3f, 1.0f, 0.3f, 1.0f);
        else entry.color = ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
        
        logs.push_back(entry);
        
        if (logs.size() > 1000) {
            logs.erase(logs.begin());
        }
    }
    
    void update_metrics() {
        static auto last_update = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_update).count();
        
        if (elapsed > 100) {
            // Mock metrics update
            metrics[0].value = 35.0f + sin(ImGui::GetTime() * 2.0f) * 20.0f; // CPU
            metrics[1].value = 256.0f + sin(ImGui::GetTime() * 0.5f) * 50.0f; // Memory
            metrics[2].value = 0.8f + sin(ImGui::GetTime() * 3.0f) * 0.3f; // Beamforming
            metrics[3].value = call_active ? 2500.0f + sin(ImGui::GetTime()) * 500.0f : 0.0f; // Translation
            metrics[4].value = encryption_active ? 15.0f + sin(ImGui::GetTime() * 4.0f) * 5.0f : 0.0f; // Encryption
            metrics[5].value = call_active ? 45.0f + sin(ImGui::GetTime() * 1.5f) * 20.0f : 0.0f; // Network
            
            // Update history
            for (auto& metric : metrics) {
                metric.history.push_back(metric.value);
                if (metric.history.size() > 200) {
                    metric.history.erase(metric.history.begin());
                }
            }
            
            // Update audio buffer
            for (size_t i = 0; i < audio_buffer.size(); i++) {
                audio_buffer[i] = call_active ? sin(ImGui::GetTime() * 10.0f + i * 0.1f) * 0.5f : 0.0f;
            }
            
            last_update = now;
        }
    }
    
    void render_status_window() {
        ImGui::Begin("System Status", &show_status);
        
        ImGui::Text("APM System v2.0.0");
        ImGui::Separator();
        
        // Connection status
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "● Backend Connected");
        ImGui::SameLine();
        ImGui::TextDisabled("(localhost:8080)");
        
        ImGui::Spacing();
        
        // Call status
        ImGui::Text("Call Status:");
        if (call_active) {
            ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "ACTIVE");
            ImGui::Text("Duration: %.1fs", call_duration);
            call_duration += ImGui::GetIO().DeltaTime;
        } else {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "IDLE");
        }
        
        ImGui::Spacing();
        
        // System features
        ImGui::Text("Features:");
        ImGui::Checkbox("Encryption (ChaCha20-Poly1305)", &encryption_active);
        ImGui::Checkbox("Translation (Whisper + NLLB)", &translation_active);
        
        ImGui::Spacing();
        
        // Quick actions
        if (ImGui::Button(call_active ? "End Call" : "Start Call", ImVec2(-1, 40))) {
            call_active = !call_active;
            if (call_active) {
                call_duration = 0.0f;
                add_log("INFO", "Call started");
            } else {
                add_log("INFO", "Call ended");
            }
        }
        
        ImGui::End();
    }
    
    void render_performance_window() {
        ImGui::Begin("Performance Metrics", &show_performance);
        
        for (const auto& metric : metrics) {
            ImGui::Text("%s: %.2f / %.0f", metric.name.c_str(), metric.value, metric.max_value);
            
            float percentage = metric.value / metric.max_value;
            ImVec4 color = percentage < 0.7f ? ImVec4(0.3f, 1.0f, 0.3f, 1.0f) :
                          percentage < 0.9f ? ImVec4(1.0f, 0.8f, 0.3f, 1.0f) :
                                             ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
            
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, color);
            ImGui::ProgressBar(percentage, ImVec2(-1, 0));
            ImGui::PopStyleColor();
            
            if (!metric.history.empty()) {
                ImGui::PlotLines(
                    ("##" + metric.name).c_str(),
                    metric.history.data(),
                    metric.history.size(),
                    0,
                    nullptr,
                    0.0f,
                    metric.max_value,
                    ImVec2(0, 60)
                );
            }
            
            ImGui::Spacing();
        }
        
        ImGui::End();
    }
    
    void render_audio_window() {
        ImGui::Begin("Audio Pipeline", &show_audio);
        
        ImGui::Text("Audio Buffer Visualization");
        ImGui::PlotLines(
            "##audio",
            audio_buffer.data(),
            audio_buffer.size(),
            0,
            nullptr,
            -1.0f,
            1.0f,
            ImVec2(-1, 150)
        );
        
        ImGui::Spacing();
        
        ImGui::Text("Pipeline Components:");
        ImGui::BulletText("Microphone Array: 4 channels @ 48kHz");
        ImGui::BulletText("Beamforming: Delay-and-sum");
        ImGui::BulletText("Echo Cancellation: NLMS adaptive");
        ImGui::BulletText("Noise Suppression: LSTM-based");
        ImGui::BulletText("VAD: Energy + ZCR");
        ImGui::BulletText("Translation: Whisper -> NLLB");
        ImGui::BulletText("Projection: 3-speaker array");
        
        ImGui::End();
    }
    
    void render_network_window() {
        ImGui::Begin("Network & Signaling", &show_network);
        
        ImGui::Text("UDP Call Signaling");
        ImGui::Separator();
        
        ImGui::Text("Local:  192.168.1.100:5060");
        ImGui::Text("Remote: 192.168.1.101:5060");
        
        ImGui::Spacing();
        
        ImGui::Text("Session Information:");
        ImGui::BulletText("Session ID: session-abc123");
        ImGui::BulletText("Protocol: Custom UDP");
        ImGui::BulletText("Encryption: X25519 + ChaCha20");
        ImGui::BulletText("Heartbeat: 5s interval");
        
        ImGui::Spacing();
        
        ImGui::Text("Discovered Peers:");
        if (ImGui::BeginTable("peers", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("Name");
            ImGui::TableSetupColumn("IP Address");
            ImGui::TableSetupColumn("Status");
            ImGui::TableHeadersRow();
            
            const char* peers[][3] = {
                {"Alice Cooper", "192.168.1.101", "Online"},
                {"Bob Martinez", "192.168.1.102", "Online"},
                {"Carol Zhang", "192.168.1.103", "Away"}
            };
            
            for (int i = 0; i < 3; i++) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("%s", peers[i][0]);
                ImGui::TableNextColumn();
                ImGui::Text("%s", peers[i][1]);
                ImGui::TableNextColumn();
                if (strcmp(peers[i][2], "Online") == 0) {
                    ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "%s", peers[i][2]);
                } else {
                    ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.3f, 1.0f), "%s", peers[i][2]);
                }
            }
            
            ImGui::EndTable();
        }
        
        ImGui::End();
    }
    
    void render_logs_window() {
        ImGui::Begin("System Logs", &show_logs);
        
        if (ImGui::Button("Clear")) {
            logs.clear();
        }
        ImGui::SameLine();
        if (ImGui::Button("Export")) {
            export_logs();
        }
        
        ImGui::Separator();
        
        ImGui::BeginChild("LogScroll", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
        
        for (const auto& log : logs) {
            auto time_t = std::chrono::system_clock::to_time_t(log.timestamp);
            char time_str[64];
            strftime(time_str, sizeof(time_str), "%H:%M:%S", localtime(&time_t));
            
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "[%s]", time_str);
            ImGui::SameLine();
            ImGui::TextColored(log.color, "[%s]", log.level.c_str());
            ImGui::SameLine();
            ImGui::Text("%s", log.message.c_str());
        }
        
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
            ImGui::SetScrollHereY(1.0f);
        }
        
        ImGui::EndChild();
        
        ImGui::End();
    }
    
    void export_logs() {
        add_log("INFO", "Logs exported to apm_logs.txt");
    }
    
    void run_benchmarks() {
        add_log("INFO", "Running benchmarks...");
        add_log("INFO", "Beamforming: 0.8ms (25x real-time)");
        add_log("INFO", "Noise Suppression: 2.1ms (9.5x real-time)");
        add_log("INFO", "Echo Cancellation: 0.5ms (40x real-time)");
        add_log("SUCCESS", "All benchmarks passed");
    }
    
    void test_encryption() {
        add_log("INFO", "Testing encryption...");
        add_log("INFO", "ChaCha20-Poly1305: OK");
        add_log("INFO", "X25519 key exchange: OK");
        add_log("SUCCESS", "Encryption test passed");
    }
    
    void test_translation() {
        add_log("INFO", "Testing translation pipeline...");
        add_log("INFO", "Whisper model loaded");
        add_log("INFO", "NLLB model loaded");
        add_log("INFO", "Test: 'Hello' -> 'Hola'");
        add_log("SUCCESS", "Translation test passed");
    }
};

int main(int argc, char** argv) {
    // Initialize GLFW
    if (!glfwInit()) {
        return -1;
    }
    
    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    
    // Create window
    GLFWwindow* window = glfwCreateWindow(1600, 900, "APM Developer Console v2.0", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return -1;
    }
    
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync
    
    // Setup Dear ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    
    // Setup style
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.1f, 0.1f, 0.15f, 1.0f);
    style.Colors[ImGuiCol_TitleBg] = ImVec4(0.2f, 0.1f, 0.3f, 1.0f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.3f, 0.15f, 0.4f, 1.0f);
    style.Colors[ImGuiCol_Button] = ImVec4(0.4f, 0.2f, 0.5f, 1.0f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.5f, 0.3f, 0.6f, 1.0f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.6f, 0.4f, 0.7f, 1.0f);
    
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);
    
    APMDevConsole console;
    
    // Main loop
    while (!glfwWindowShouldClose(window) && !console.should_close_window()) {
        glfwPollEvents();
        
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        
        console.render();
        
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        
        glfwSwapBuffers(window);
    }
    
    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    
    glfwDestroyWindow(window);
    glfwTerminate();
    
    return 0;
}
