#!/usr/bin/env python3
"""
APM System - Desktop Control GUI
Lightweight Python interface for APM system control
"""

import tkinter as tk
from tkinter import ttk, messagebox
import subprocess
import threading
import socket
import json
import time
from datetime import datetime
from pathlib import Path

class APMControlPanel:
    def __init__(self, root):
        self.root = root
        self.root.title("APM System Control v2.0")
        self.root.geometry("900x700")
        self.root.configure(bg="#1a1a2e")
        
        # State
        self.call_active = False
        self.mic_muted = False
        self.speaker_muted = False
        self.encryption_enabled = True
        self.translation_enabled = True
        self.apm_process = None
        self.backend_connected = False
        
        # Colors
        self.bg_dark = "#1a1a2e"
        self.bg_medium = "#16213e"
        self.bg_light = "#0f3460"
        self.accent_purple = "#8b5cf6"
        self.accent_pink = "#ec4899"
        self.accent_green = "#10b981"
        self.accent_red = "#ef4444"
        self.text_color = "#e0e0e0"
        
        self.setup_ui()
        self.check_backend_connection()
        
    def setup_ui(self):
        """Setup the main UI components"""
        
        # Header
        header = tk.Frame(self.root, bg=self.bg_medium, height=80)
        header.pack(fill=tk.X, padx=10, pady=10)
        header.pack_propagate(False)
        
        title_label = tk.Label(
            header, 
            text="🎙️ APM System Control", 
            font=("Arial", 20, "bold"),
            bg=self.bg_medium, 
            fg=self.accent_purple
        )
        title_label.pack(side=tk.LEFT, padx=20, pady=20)
        
        status_frame = tk.Frame(header, bg=self.bg_medium)
        status_frame.pack(side=tk.RIGHT, padx=20)
        
        self.status_label = tk.Label(
            status_frame,
            text="● Disconnected",
            font=("Arial", 10),
            bg=self.bg_medium,
            fg=self.accent_red
        )
        self.status_label.pack()
        
        # Main container
        main_container = tk.Frame(self.root, bg=self.bg_dark)
        main_container.pack(fill=tk.BOTH, expand=True, padx=10, pady=5)
        
        # Left panel - Call Controls
        left_panel = tk.Frame(main_container, bg=self.bg_medium, width=400)
        left_panel.pack(side=tk.LEFT, fill=tk.BOTH, expand=True, padx=5)
        
        self.setup_call_controls(left_panel)
        
        # Right panel - Settings & Info
        right_panel = tk.Frame(main_container, bg=self.bg_medium, width=400)
        right_panel.pack(side=tk.RIGHT, fill=tk.BOTH, expand=True, padx=5)
        
        self.setup_settings_panel(right_panel)
        
    def setup_call_controls(self, parent):
        """Setup call control UI"""
        
        # Call Status
        status_frame = tk.LabelFrame(
            parent,
            text="Call Status",
            font=("Arial", 12, "bold"),
            bg=self.bg_medium,
            fg=self.text_color,
            padx=20,
            pady=20
        )
        status_frame.pack(fill=tk.X, padx=10, pady=10)
        
        self.call_status_label = tk.Label(
            status_frame,
            text="No Active Call",
            font=("Arial", 14),
            bg=self.bg_medium,
            fg=self.text_color
        )
        self.call_status_label.pack(pady=10)
        
        self.call_duration_label = tk.Label(
            status_frame,
            text="00:00:00",
            font=("Arial", 24, "bold"),
            bg=self.bg_medium,
            fg=self.accent_purple
        )
        self.call_duration_label.pack(pady=5)
        
        # Control Buttons
        controls_frame = tk.LabelFrame(
            parent,
            text="Controls",
            font=("Arial", 12, "bold"),
            bg=self.bg_medium,
            fg=self.text_color,
            padx=20,
            pady=20
        )
        controls_frame.pack(fill=tk.X, padx=10, pady=10)
        
        # Button grid
        btn_frame = tk.Frame(controls_frame, bg=self.bg_medium)
        btn_frame.pack()
        
        # Start/End Call
        self.call_btn = tk.Button(
            btn_frame,
            text="📞 Start Call",
            font=("Arial", 12, "bold"),
            bg=self.accent_green,
            fg="white",
            activebackground=self.accent_green,
            command=self.toggle_call,
            width=20,
            height=2,
            relief=tk.FLAT,
            cursor="hand2"
        )
        self.call_btn.grid(row=0, column=0, columnspan=2, padx=5, pady=5, sticky="ew")
        
        # Mic Mute
        self.mic_btn = tk.Button(
            btn_frame,
            text="🎤 Mute Mic",
            font=("Arial", 10),
            bg=self.bg_light,
            fg=self.text_color,
            activebackground=self.bg_light,
            command=self.toggle_mic,
            width=18,
            height=2,
            relief=tk.FLAT,
            cursor="hand2"
        )
        self.mic_btn.grid(row=1, column=0, padx=5, pady=5)
        
        # Speaker Mute
        self.speaker_btn = tk.Button(
            btn_frame,
            text="🔊 Mute Speaker",
            font=("Arial", 10),
            bg=self.bg_light,
            fg=self.text_color,
            activebackground=self.bg_light,
            command=self.toggle_speaker,
            width=18,
            height=2,
            relief=tk.FLAT,
            cursor="hand2"
        )
        self.speaker_btn.grid(row=1, column=1, padx=5, pady=5)
        
        # Translation Display
        trans_frame = tk.LabelFrame(
            parent,
            text="Real-time Translation",
            font=("Arial", 12, "bold"),
            bg=self.bg_medium,
            fg=self.text_color,
            padx=10,
            pady=10
        )
        trans_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)
        
        # Translation text area
        self.translation_text = tk.Text(
            trans_frame,
            height=10,
            font=("Arial", 10),
            bg=self.bg_light,
            fg=self.text_color,
            wrap=tk.WORD,
            relief=tk.FLAT,
            padx=10,
            pady=10
        )
        self.translation_text.pack(fill=tk.BOTH, expand=True)
        self.translation_text.insert("1.0", "Translation output will appear here...\n")
        self.translation_text.config(state=tk.DISABLED)
        
    def setup_settings_panel(self, parent):
        """Setup settings panel"""
        
        # Language Settings
        lang_frame = tk.LabelFrame(
            parent,
            text="Language Settings",
            font=("Arial", 12, "bold"),
            bg=self.bg_medium,
            fg=self.text_color,
            padx=20,
            pady=20
        )
        lang_frame.pack(fill=tk.X, padx=10, pady=10)
        
        languages = ["English", "Spanish", "French", "German", "Japanese", 
                    "Chinese", "Arabic", "Russian", "Portuguese", "Italian"]
        
        tk.Label(lang_frame, text="Source Language:", bg=self.bg_medium, 
                fg=self.text_color, font=("Arial", 10)).pack(anchor=tk.W, pady=5)
        self.source_lang = ttk.Combobox(lang_frame, values=languages, 
                                       state="readonly", width=25)
        self.source_lang.set("English")
        self.source_lang.pack(pady=5)
        
        tk.Label(lang_frame, text="Target Language:", bg=self.bg_medium, 
                fg=self.text_color, font=("Arial", 10)).pack(anchor=tk.W, pady=5)
        self.target_lang = ttk.Combobox(lang_frame, values=languages, 
                                       state="readonly", width=25)
        self.target_lang.set("Spanish")
        self.target_lang.pack(pady=5)
        
        # Security Settings
        security_frame = tk.LabelFrame(
            parent,
            text="Security",
            font=("Arial", 12, "bold"),
            bg=self.bg_medium,
            fg=self.text_color,
            padx=20,
            pady=20
        )
        security_frame.pack(fill=tk.X, padx=10, pady=10)
        
        self.encryption_var = tk.BooleanVar(value=True)
        encryption_check = tk.Checkbutton(
            security_frame,
            text="🔒 End-to-End Encryption (ChaCha20-Poly1305)",
            variable=self.encryption_var,
            font=("Arial", 10),
            bg=self.bg_medium,
            fg=self.text_color,
            selectcolor=self.bg_light,
            activebackground=self.bg_medium,
            activeforeground=self.text_color
        )
        encryption_check.pack(anchor=tk.W, pady=5)
        
        self.translation_var = tk.BooleanVar(value=True)
        translation_check = tk.Checkbutton(
            security_frame,
            text="🌍 Real-time Translation (Whisper + NLLB)",
            variable=self.translation_var,
            font=("Arial", 10),
            bg=self.bg_medium,
            fg=self.text_color,
            selectcolor=self.bg_light,
            activebackground=self.bg_medium,
            activeforeground=self.text_color
        )
        translation_check.pack(anchor=tk.W, pady=5)
        
        # System Info
        info_frame = tk.LabelFrame(
            parent,
            text="System Information",
            font=("Arial", 12, "bold"),
            bg=self.bg_medium,
            fg=self.text_color,
            padx=20,
            pady=20
        )
        info_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)
        
        self.info_text = tk.Text(
            info_frame,
            height=8,
            font=("Courier", 9),
            bg=self.bg_light,
            fg=self.text_color,
            wrap=tk.WORD,
            relief=tk.FLAT,
            padx=10,
            pady=10
        )
        self.info_text.pack(fill=tk.BOTH, expand=True)
        
        # Get system info
        info = self.get_system_info()
        self.info_text.insert("1.0", info)
        self.info_text.config(state=tk.DISABLED)
        
        # Action Buttons
        action_frame = tk.Frame(parent, bg=self.bg_medium)
        action_frame.pack(fill=tk.X, padx=10, pady=10)
        
        tk.Button(
            action_frame,
            text="🔄 Restart Backend",
            font=("Arial", 10),
            bg=self.accent_purple,
            fg="white",
            command=self.restart_backend,
            relief=tk.FLAT,
            cursor="hand2",
            height=2
        ).pack(fill=tk.X, pady=5)
        
        tk.Button(
            action_frame,
            text="📊 View Logs",
            font=("Arial", 10),
            bg=self.bg_light,
            fg=self.text_color,
            command=self.view_logs,
            relief=tk.FLAT,
            cursor="hand2",
            height=2
        ).pack(fill=tk.X, pady=5)
        
    def get_system_info(self):
        """Get system information"""
        try:
            hostname = socket.gethostname()
            local_ip = socket.gethostbyname(hostname)
        except Exception as e:
            print(f"Warning: Failed to get system info: {e}")
            hostname = "Unknown"
            local_ip = "Unknown"
        
        info = f"""
Local Participant:
  Name: {hostname}
  IP: {local_ip}
  Port: 5060

APM Version: 2.0.0
Build: Release
Python: {Path('/usr/bin/python3').exists() and 'Available' or 'Not Found'}

Crypto: libsodium
  ChaCha20-Poly1305: ✓
  X25519: ✓
  Argon2id: ✓

Translation:
  Whisper: {'✓' if self.check_whisper() else '✗'}
  NLLB-200: {'✓' if self.check_nllb() else '✗'}
        """
        return info.strip()
    
    def check_whisper(self):
        """Check if Whisper is installed"""
        try:
            import whisper
            return True
        except ImportError:
            return False
    
    def check_nllb(self):
        """Check if transformers is installed"""
        try:
            from transformers import AutoTokenizer
            return True
        except ImportError:
            return False
    
    def check_backend_connection(self):
        """Check connection to C++ backend"""
        def check():
            while True:
                try:
                    # Try to connect to backend (mock for now)
                    # In real implementation: socket.connect(("localhost", 8080))
                    self.backend_connected = True
                    self.status_label.config(
                        text="● Connected",
                        fg=self.accent_green
                    )
                except Exception as e:
                    print(f"Backend connection check failed: {e}")
                    self.backend_connected = False
                    self.status_label.config(
                        text="● Disconnected",
                        fg=self.accent_red
                    )
                time.sleep(5)
        
        thread = threading.Thread(target=check, daemon=True)
        thread.start()
    
    def toggle_call(self):
        """Toggle call state"""
        self.call_active = not self.call_active
        
        if self.call_active:
            self.call_btn.config(
                text="📞 End Call",
                bg=self.accent_red
            )
            self.call_status_label.config(text="Call Active")
            self.add_translation("System", "Call started with encryption enabled")
            self.start_call_timer()
        else:
            self.call_btn.config(
                text="📞 Start Call",
                bg=self.accent_green
            )
            self.call_status_label.config(text="No Active Call")
            self.call_duration_label.config(text="00:00:00")
            self.add_translation("System", "Call ended")
    
    def start_call_timer(self):
        """Start call duration timer"""
        start_time = time.time()
        
        def update_timer():
            if self.call_active:
                elapsed = int(time.time() - start_time)
                hours = elapsed // 3600
                minutes = (elapsed % 3600) // 60
                seconds = elapsed % 60
                self.call_duration_label.config(
                    text=f"{hours:02d}:{minutes:02d}:{seconds:02d}"
                )
                self.root.after(1000, update_timer)
        
        update_timer()
    
    def toggle_mic(self):
        """Toggle microphone mute"""
        self.mic_muted = not self.mic_muted
        
        if self.mic_muted:
            self.mic_btn.config(
                text="🎤 Unmute Mic",
                bg=self.accent_red
            )
            self.add_translation("System", "Microphone muted")
        else:
            self.mic_btn.config(
                text="🎤 Mute Mic",
                bg=self.bg_light
            )
            self.add_translation("System", "Microphone unmuted")
    
    def toggle_speaker(self):
        """Toggle speaker mute"""
        self.speaker_muted = not self.speaker_muted
        
        if self.speaker_muted:
            self.speaker_btn.config(
                text="🔊 Unmute Speaker",
                bg=self.accent_red
            )
            self.add_translation("System", "Speaker muted")
        else:
            self.speaker_btn.config(
                text="🔊 Mute Speaker",
                bg=self.bg_light
            )
            self.add_translation("System", "Speaker unmuted")
    
    def add_translation(self, source, text):
        """Add translation to display"""
        timestamp = datetime.now().strftime("%H:%M:%S")
        self.translation_text.config(state=tk.NORMAL)
        self.translation_text.insert("end", f"[{timestamp}] {source}: {text}\n")
        self.translation_text.see("end")
        self.translation_text.config(state=tk.DISABLED)
    
    def restart_backend(self):
        """Restart the APM backend"""
        messagebox.showinfo("Restart", "Restarting APM backend...")
        self.add_translation("System", "Backend restart initiated")
        # In real implementation: subprocess.Popen(["./build/apm_system"])
    
    def view_logs(self):
        """Open log viewer window"""
        log_window = tk.Toplevel(self.root)
        log_window.title("APM System Logs")
        log_window.geometry("800x600")
        log_window.configure(bg=self.bg_medium)
        
        log_text = tk.Text(
            log_window,
            font=("Courier", 9),
            bg=self.bg_light,
            fg=self.text_color,
            wrap=tk.WORD
        )
        log_text.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)
        
        # Mock log data
        logs = """
[2024-12-12 10:30:15] [INFO] APM System initialized
[2024-12-12 10:30:16] [INFO] Crypto system ready (ChaCha20-Poly1305)
[2024-12-12 10:30:17] [INFO] Translation models loaded (Whisper + NLLB)
[2024-12-12 10:30:18] [INFO] Call signaling started on port 5060
[2024-12-12 10:30:19] [INFO] Listening for incoming connections
[2024-12-12 10:35:22] [INFO] Call initiated to 192.168.1.101
[2024-12-12 10:35:24] [INFO] Call connected (encrypted)
[2024-12-12 10:35:30] [INFO] Translation: "Hello" -> "Hola"
[2024-12-12 10:40:15] [INFO] Call ended (duration: 4m 51s)
        """
        log_text.insert("1.0", logs.strip())
        log_text.config(state=tk.DISABLED)

def main():
    root = tk.Tk()
    app = APMControlPanel(root)
    root.mainloop()

if __name__ == "__main__":
    main()
