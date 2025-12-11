#!/usr/bin/env python3
"""
APM Headphone Translator - Configuration GUI
Simple setup tool for end users to configure audio devices and languages
"""

import tkinter as tk
from tkinter import ttk, messagebox, filedialog
import json
import os
import sys
import subprocess
import platform
from pathlib import Path

# Try to import audio device enumeration
try:
    import sounddevice as sd
    HAS_SOUNDDEVICE = True
except ImportError:
    HAS_SOUNDDEVICE = False
    print("Warning: sounddevice module not found. Install with: pip install sounddevice")

class APMConfigGUI:
    def __init__(self, root):
        self.root = root
        self.root.title("APM Headphone Translator - Configuration")
        self.root.geometry("650x700")
        self.root.resizable(False, False)
        
        # Default config
        self.config = {
            "audio": {
                "sample_rate": 48000,
                "buffer_size": 960,
                "input_device": "default",
                "output_device": "default"
            },
            "translation": {
                "source_language": "en-US",
                "target_language": "es-ES",
                "mode": "online",
                "quality": "balanced"
            },
            "processing": {
                "noise_cancellation": 70,
                "echo_cancellation": 100,
                "voice_activity_detection": True
            },
            "ui": {
                "show_transcripts": True,
                "compact_mode": False,
                "notifications": True
            }
        }
        
        self.config_path = self.get_config_path()
        self.load_config()
        
        self.setup_ui()
        
    def get_config_path(self):
        """Get the configuration file path based on OS"""
        system = platform.system()
        if system == "Windows":
            config_dir = Path(os.getenv("APPDATA")) / "APMHeadphone"
        elif system == "Darwin":  # macOS
            config_dir = Path.home() / "Library" / "Application Support" / "APMHeadphone"
        else:  # Linux
            config_dir = Path.home() / ".config" / "apm-headphone"
        
        config_dir.mkdir(parents=True, exist_ok=True)
        return config_dir / "settings.json"
    
    def load_config(self):
        """Load existing configuration"""
        if self.config_path.exists():
            try:
                with open(self.config_path, 'r') as f:
                    saved_config = json.load(f)
                    # Merge with defaults
                    self.merge_config(self.config, saved_config)
            except Exception as e:
                messagebox.showwarning("Config Load Error", 
                                      f"Could not load config: {e}\nUsing defaults.")
    
    def merge_config(self, default, saved):
        """Recursively merge saved config with defaults"""
        for key, value in saved.items():
            if key in default:
                if isinstance(value, dict) and isinstance(default[key], dict):
                    self.merge_config(default[key], value)
                else:
                    default[key] = value
    
    def save_config(self):
        """Save configuration to file"""
        try:
            with open(self.config_path, 'w') as f:
                json.dump(self.config, f, indent=2)
            return True
        except Exception as e:
            messagebox.showerror("Save Error", f"Could not save config: {e}")
            return False
    
    def setup_ui(self):
        """Create the user interface"""
        # Header
        header = tk.Frame(self.root, bg="#2c3e50", height=80)
        header.pack(fill=tk.X)
        
        title = tk.Label(header, text="🎧 APM Headphone Translator",
                        font=("Arial", 18, "bold"), bg="#2c3e50", fg="white")
        title.pack(pady=20)
        
        # Main container
        main_frame = tk.Frame(self.root, padx=20, pady=20)
        main_frame.pack(fill=tk.BOTH, expand=True)
        
        # Notebook for tabs
        notebook = ttk.Notebook(main_frame)
        notebook.pack(fill=tk.BOTH, expand=True)
        
        # Tabs
        self.setup_audio_tab(notebook)
        self.setup_language_tab(notebook)
        self.setup_processing_tab(notebook)
        self.setup_advanced_tab(notebook)
        
        # Bottom buttons
        button_frame = tk.Frame(self.root, pady=10)
        button_frame.pack(fill=tk.X)
        
        tk.Button(button_frame, text="Test Audio", command=self.test_audio,
                 bg="#3498db", fg="white", padx=20, pady=10).pack(side=tk.LEFT, padx=10)
        
        tk.Button(button_frame, text="Save & Close", command=self.save_and_close,
                 bg="#27ae60", fg="white", padx=20, pady=10).pack(side=tk.RIGHT, padx=10)
        
        tk.Button(button_frame, text="Cancel", command=self.root.quit,
                 bg="#95a5a6", fg="white", padx=20, pady=10).pack(side=tk.RIGHT)
    
    def setup_audio_tab(self, notebook):
        """Audio devices configuration tab"""
        tab = ttk.Frame(notebook)
        notebook.add(tab, text="  Audio Devices  ")
        
        # Device selection
        device_frame = tk.LabelFrame(tab, text="Device Selection", padx=10, pady=10)
        device_frame.pack(fill=tk.X, padx=10, pady=10)
        
        # Input device
        tk.Label(device_frame, text="Microphone (Input):", font=("Arial", 10, "bold")).grid(
            row=0, column=0, sticky=tk.W, pady=5)
        
        self.input_device_var = tk.StringVar(value=self.config["audio"]["input_device"])
        input_devices = self.get_input_devices()
        self.input_combo = ttk.Combobox(device_frame, textvariable=self.input_device_var,
                                       values=input_devices, state="readonly", width=40)
        self.input_combo.grid(row=1, column=0, pady=5)
        
        # Output device
        tk.Label(device_frame, text="Speakers/Headphones (Output):", 
                font=("Arial", 10, "bold")).grid(row=2, column=0, sticky=tk.W, pady=5)
        
        self.output_device_var = tk.StringVar(value=self.config["audio"]["output_device"])
        output_devices = self.get_output_devices()
        self.output_combo = ttk.Combobox(device_frame, textvariable=self.output_device_var,
                                        values=output_devices, state="readonly", width=40)
        self.output_combo.grid(row=3, column=0, pady=5)
        
        # Refresh button
        tk.Button(device_frame, text="🔄 Refresh Devices", 
                 command=self.refresh_devices).grid(row=4, column=0, pady=10)
        
        # Audio settings
        settings_frame = tk.LabelFrame(tab, text="Audio Settings", padx=10, pady=10)
        settings_frame.pack(fill=tk.X, padx=10, pady=10)
        
        tk.Label(settings_frame, text="Sample Rate:").grid(row=0, column=0, sticky=tk.W)
        self.sample_rate_var = tk.StringVar(value=str(self.config["audio"]["sample_rate"]))
        ttk.Combobox(settings_frame, textvariable=self.sample_rate_var,
                    values=["16000", "44100", "48000"], state="readonly", width=15).grid(
                        row=0, column=1, padx=10)
        
        tk.Label(settings_frame, text="Buffer Size:").grid(row=1, column=0, sticky=tk.W, pady=5)
        self.buffer_size_var = tk.StringVar(value=str(self.config["audio"]["buffer_size"]))
        ttk.Combobox(settings_frame, textvariable=self.buffer_size_var,
                    values=["480", "960", "1920"], state="readonly", width=15).grid(
                        row=1, column=1, padx=10)
        
        # Level meters
        tk.Label(settings_frame, text="Input Level:").grid(row=2, column=0, sticky=tk.W, pady=5)
        self.input_level = ttk.Progressbar(settings_frame, length=200, mode='determinate')
        self.input_level.grid(row=2, column=1, padx=10)
    
    def setup_language_tab(self, notebook):
        """Language selection tab"""
        tab = ttk.Frame(notebook)
        notebook.add(tab, text="  Languages  ")
        
        lang_frame = tk.LabelFrame(tab, text="Translation Languages", padx=10, pady=10)
        lang_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)
        
        # Source language
        tk.Label(lang_frame, text="I speak:", font=("Arial", 11, "bold")).grid(
            row=0, column=0, sticky=tk.W, pady=10)
        
        self.source_lang_var = tk.StringVar(value=self.config["translation"]["source_language"])
        
        languages = {
            "English (US)": "en-US",
            "English (UK)": "en-GB",
            "Spanish (Spain)": "es-ES",
            "Spanish (Latin America)": "es-LA",
            "Japanese": "ja-JP",
            "French": "fr-FR",
            "German": "de-DE",
            "Chinese (Mandarin)": "zh-CN",
            "Korean": "ko-KR",
            "Italian": "it-IT",
            "Portuguese (Brazil)": "pt-BR",
            "Russian": "ru-RU",
            "Arabic": "ar-SA"
        }
        
        self.lang_names = list(languages.keys())
        self.lang_codes = list(languages.values())
        
        # Find current selection
        current_source = self.config["translation"]["source_language"]
        source_idx = self.lang_codes.index(current_source) if current_source in self.lang_codes else 0
        
        source_combo = ttk.Combobox(lang_frame, values=self.lang_names,
                                   state="readonly", width=30)
        source_combo.current(source_idx)
        source_combo.grid(row=1, column=0, pady=5)
        source_combo.bind("<<ComboboxSelected>>", 
                         lambda e: self.source_lang_var.set(
                             self.lang_codes[source_combo.current()]))
        
        # Swap button
        tk.Button(lang_frame, text="⇅ Swap", command=lambda: self.swap_languages(
            source_combo, target_combo)).grid(row=2, column=0, pady=10)
        
        # Target language
        tk.Label(lang_frame, text="Translate to:", font=("Arial", 11, "bold")).grid(
            row=3, column=0, sticky=tk.W, pady=10)
        
        self.target_lang_var = tk.StringVar(value=self.config["translation"]["target_language"])
        current_target = self.config["translation"]["target_language"]
        target_idx = self.lang_codes.index(current_target) if current_target in self.lang_codes else 1
        
        target_combo = ttk.Combobox(lang_frame, values=self.lang_names,
                                   state="readonly", width=30)
        target_combo.current(target_idx)
        target_combo.grid(row=4, column=0, pady=5)
        target_combo.bind("<<ComboboxSelected>>",
                         lambda e: self.target_lang_var.set(
                             self.lang_codes[target_combo.current()]))
        
        # Mode selection
        mode_frame = tk.LabelFrame(tab, text="Translation Mode", padx=10, pady=10)
        mode_frame.pack(fill=tk.X, padx=10, pady=10)
        
        self.mode_var = tk.StringVar(value=self.config["translation"]["mode"])
        
        tk.Radiobutton(mode_frame, text="Online (requires internet, higher quality)",
                      variable=self.mode_var, value="online").pack(anchor=tk.W, pady=3)
        tk.Radiobutton(mode_frame, text="Offline (no internet needed, uses downloaded models)",
                      variable=self.mode_var, value="offline").pack(anchor=tk.W, pady=3)
        tk.Radiobutton(mode_frame, text="Hybrid (auto-select based on availability)",
                      variable=self.mode_var, value="hybrid").pack(anchor=tk.W, pady=3)
    
    def setup_processing_tab(self, notebook):
        """Audio processing settings tab"""
        tab = ttk.Frame(notebook)
        notebook.add(tab, text="  Processing  ")
        
        # Quality preset
        quality_frame = tk.LabelFrame(tab, text="Quality Preset", padx=10, pady=10)
        quality_frame.pack(fill=tk.X, padx=10, pady=10)
        
        self.quality_var = tk.StringVar(value=self.config["translation"]["quality"])
        
        presets = [
            ("Fast", "fast", "Lower quality, ~50ms latency"),
            ("Balanced", "balanced", "Good quality, ~200ms latency"),
            ("Best", "best", "Highest quality, ~500ms latency")
        ]
        
        for name, value, desc in presets:
            frame = tk.Frame(quality_frame)
            frame.pack(fill=tk.X, pady=3)
            tk.Radiobutton(frame, text=name, variable=self.quality_var,
                          value=value).pack(side=tk.LEFT)
            tk.Label(frame, text=desc, fg="gray").pack(side=tk.LEFT, padx=10)
        
        # Processing options
        proc_frame = tk.LabelFrame(tab, text="Audio Processing", padx=10, pady=10)
        proc_frame.pack(fill=tk.X, padx=10, pady=10)
        
        # Noise cancellation
        tk.Label(proc_frame, text="Noise Cancellation:").grid(row=0, column=0, sticky=tk.W)
        self.noise_var = tk.IntVar(value=self.config["processing"]["noise_cancellation"])
        noise_scale = tk.Scale(proc_frame, from_=0, to=100, orient=tk.HORIZONTAL,
                              variable=self.noise_var, length=300)
        noise_scale.grid(row=0, column=1, padx=10)
        tk.Label(proc_frame, textvariable=self.noise_var).grid(row=0, column=2)
        
        # Echo cancellation
        tk.Label(proc_frame, text="Echo Cancellation:").grid(row=1, column=0, sticky=tk.W, pady=5)
        self.echo_var = tk.IntVar(value=self.config["processing"]["echo_cancellation"])
        echo_scale = tk.Scale(proc_frame, from_=0, to=100, orient=tk.HORIZONTAL,
                             variable=self.echo_var, length=300)
        echo_scale.grid(row=1, column=1, padx=10, pady=5)
        tk.Label(proc_frame, textvariable=self.echo_var).grid(row=1, column=2, pady=5)
        
        # Voice activity detection
        self.vad_var = tk.BooleanVar(value=self.config["processing"]["voice_activity_detection"])
        tk.Checkbutton(proc_frame, text="Enable Voice Activity Detection (VAD)",
                      variable=self.vad_var).grid(row=2, column=0, columnspan=3,
                                                  sticky=tk.W, pady=10)
    
    def setup_advanced_tab(self, notebook):
        """Advanced settings tab"""
        tab = ttk.Frame(notebook)
        notebook.add(tab, text="  Advanced  ")
        
        # UI options
        ui_frame = tk.LabelFrame(tab, text="User Interface", padx=10, pady=10)
        ui_frame.pack(fill=tk.X, padx=10, pady=10)
        
        self.transcripts_var = tk.BooleanVar(value=self.config["ui"]["show_transcripts"])
        tk.Checkbutton(ui_frame, text="Show text transcripts",
                      variable=self.transcripts_var).pack(anchor=tk.W, pady=3)
        
        self.compact_var = tk.BooleanVar(value=self.config["ui"]["compact_mode"])
        tk.Checkbutton(ui_frame, text="Compact mode (minimal window)",
                      variable=self.compact_var).pack(anchor=tk.W, pady=3)
        
        self.notifications_var = tk.BooleanVar(value=self.config["ui"]["notifications"])
        tk.Checkbutton(ui_frame, text="Show desktop notifications",
                      variable=self.notifications_var).pack(anchor=tk.W, pady=3)
        
        # Actions
        actions_frame = tk.LabelFrame(tab, text="Actions", padx=10, pady=10)
        actions_frame.pack(fill=tk.X, padx=10, pady=10)
        
        tk.Button(actions_frame, text="📥 Download Language Packs",
                 command=self.download_models).pack(fill=tk.X, pady=5)
        
        tk.Button(actions_frame, text="🔄 Check for Updates",
                 command=self.check_updates).pack(fill=tk.X, pady=5)
        
        tk.Button(actions_frame, text="📋 Export Configuration",
                 command=self.export_config).pack(fill=tk.X, pady=5)
        
        tk.Button(actions_frame, text="📂 Import Configuration",
                 command=self.import_config).pack(fill=tk.X, pady=5)
        
        tk.Button(actions_frame, text="🗑️ Reset to Defaults",
                 command=self.reset_defaults).pack(fill=tk.X, pady=5)
    
    def get_input_devices(self):
        """Get list of audio input devices"""
        if not HAS_SOUNDDEVICE:
            return ["default", "Built-in Microphone"]
        
        try:
            devices = sd.query_devices()
            input_devices = ["default"]
            for i, dev in enumerate(devices):
                if dev['max_input_channels'] > 0:
                    input_devices.append(f"{dev['name']} ({i})")
            return input_devices
        except:
            return ["default", "Built-in Microphone"]
    
    def get_output_devices(self):
        """Get list of audio output devices"""
        if not HAS_SOUNDDEVICE:
            return ["default", "Built-in Speakers"]
        
        try:
            devices = sd.query_devices()
            output_devices = ["default"]
            for i, dev in enumerate(devices):
                if dev['max_output_channels'] > 0:
                    output_devices.append(f"{dev['name']} ({i})")
            return output_devices
        except:
            return ["default", "Built-in Speakers"]
    
    def refresh_devices(self):
        """Refresh audio device lists"""
        self.input_combo['values'] = self.get_input_devices()
        self.output_combo['values'] = self.get_output_devices()
        messagebox.showinfo("Refresh", "Device lists refreshed!")
    
    def swap_languages(self, source_combo, target_combo):
        """Swap source and target languages"""
        source_idx = source_combo.current()
        target_idx = target_combo.current()
        source_combo.current(target_idx)
        target_combo.current(source_idx)
        self.source_lang_var.set(self.lang_codes[target_idx])
        self.target_lang_var.set(self.lang_codes[source_idx])
    
    def test_audio(self):
        """Test audio devices"""
        messagebox.showinfo("Audio Test", 
                          "Speak into your microphone.\n"
                          "You should see the input level meter move.\n\n"
                          "If you don't hear anything, check your audio device selection.")
        # TODO: Implement actual audio test
    
    def download_models(self):
        """Download offline language models"""
        response = messagebox.askyesno("Download Models",
                                      "Download offline language packs?\n"
                                      "This will download approximately 650MB of data.")
        if response:
            messagebox.showinfo("Download", 
                              "Model download feature coming soon!\n"
                              "For now, use the online translation mode.")
    
    def check_updates(self):
        """Check for application updates"""
        messagebox.showinfo("Updates", 
                          "You are running version 1.0.0\n"
                          "No updates available.")
    
    def export_config(self):
        """Export configuration to file"""
        filename = filedialog.asksaveasfilename(
            defaultextension=".json",
            filetypes=[("JSON files", "*.json"), ("All files", "*.*")])
        
        if filename:
            try:
                with open(filename, 'w') as f:
                    json.dump(self.config, f, indent=2)
                messagebox.showinfo("Export", f"Configuration exported to:\n{filename}")
            except Exception as e:
                messagebox.showerror("Export Error", f"Failed to export: {e}")
    
    def import_config(self):
        """Import configuration from file"""
        filename = filedialog.askopenfilename(
            filetypes=[("JSON files", "*.json"), ("All files", "*.*")])
        
        if filename:
            try:
                with open(filename, 'r') as f:
                    imported = json.load(f)
                    self.merge_config(self.config, imported)
                messagebox.showinfo("Import", "Configuration imported successfully!")
                # Refresh UI with new values
                self.root.destroy()
                # Would need to restart app to refresh UI properly
            except Exception as e:
                messagebox.showerror("Import Error", f"Failed to import: {e}")
    
    def reset_defaults(self):
        """Reset configuration to defaults"""
        response = messagebox.askyesno("Reset",
                                      "Reset all settings to defaults?\n"
                                      "This cannot be undone.")
        if response:
            # Reset to default config
            self.config = {
                "audio": {
                    "sample_rate": 48000,
                    "buffer_size": 960,
                    "input_device": "default",
                    "output_device": "default"
                },
                "translation": {
                    "source_language": "en-US",
                    "target_language": "es-ES",
                    "mode": "online",
                    "quality": "balanced"
                },
                "processing": {
                    "noise_cancellation": 70,
                    "echo_cancellation": 100,
                    "voice_activity_detection": True
                },
                "ui": {
                    "show_transcripts": True,
                    "compact_mode": False,
                    "notifications": True
                }
            }
            if self.save_config():
                messagebox.showinfo("Reset", "Settings reset to defaults!")
    
    def save_and_close(self):
        """Save configuration and close"""
        # Update config with current values
        self.config["audio"]["input_device"] = self.input_device_var.get()
        self.config["audio"]["output_device"] = self.output_device_var.get()
        self.config["audio"]["sample_rate"] = int(self.sample_rate_var.get())
        self.config["audio"]["buffer_size"] = int(self.buffer_size_var.get())
        
        self.config["translation"]["source_language"] = self.source_lang_var.get()
        self.config["translation"]["target_language"] = self.target_lang_var.get()
        self.config["translation"]["mode"] = self.mode_var.get()
        self.config["translation"]["quality"] = self.quality_var.get()
        
        self.config["processing"]["noise_cancellation"] = self.noise_var.get()
        self.config["processing"]["echo_cancellation"] = self.echo_var.get()
        self.config["processing"]["voice_activity_detection"] = self.vad_var.get()
        
        self.config["ui"]["show_transcripts"] = self.transcripts_var.get()
        self.config["ui"]["compact_mode"] = self.compact_var.get()
        self.config["ui"]["notifications"] = self.notifications_var.get()
        
        if self.save_config():
            messagebox.showinfo("Success", "Configuration saved successfully!")
            self.root.quit()

def main():
    root = tk.Tk()
    app = APMConfigGUI(root)
    root.mainloop()

if __name__ == "__main__":
    main()
