# APM Headphone System - User Installation Guide

**Transform your headphones into a real-time translation device!**

This guide will help you install and set up the Acoustic Projection Microphone (APM) system on your device to use with your headphones for real-time language translation.

---

## 🎧 What You'll Need

### Hardware Requirements
- **Headphones/Headset**: Any headphones with a built-in microphone
  - Bluetooth headphones (recommended)
  - Wired headphones with mic
  - USB gaming headsets
- **Computer**: 
  - Windows 10/11, macOS 10.15+, or Linux (Ubuntu 20.04+)
  - 4GB RAM minimum (8GB recommended)
  - 500MB free disk space

### Optional (For Enhanced Experience)
- Multi-microphone headset or array microphone for better noise cancellation
- High-quality headphones for clearer translated audio

---

## 📥 Quick Installation

### Windows

1. **Download the Windows Installer**
   - [Download APM-Headphone-Setup.exe](https://github.com/dfeen87/acoustic-projection-microphone-system/releases/latest)
   - File size: ~150MB

2. **Run the Installer**
   - Double-click `APM-Headphone-Setup.exe`
   - Click "Yes" if Windows asks for permission
   - Follow the installation wizard
   - Choose installation location (default: `C:\Program Files\APM`)

3. **Launch the Application**
   - Find "APM Headphone Translator" in your Start Menu
   - Or run from desktop shortcut

### macOS

1. **Download the macOS Package**
   - [Download APM-Headphone.dmg](https://github.com/dfeen87/acoustic-projection-microphone-system/releases/latest)
   - File size: ~120MB

2. **Install the Application**
   - Open the `.dmg` file
   - Drag "APM Headphone" to Applications folder
   - Right-click and select "Open" (first time only)
   - Click "Open" when macOS asks for confirmation

3. **Grant Permissions**
   - System Preferences → Security & Privacy → Microphone
   - Enable for "APM Headphone"

### Linux (Ubuntu/Debian)

1. **Download and Install**
   ```bash
   # Download the package
   wget https://github.com/ydfeen87/acoustic-projection-microphone-system/releases/latest/apm-headphone_1.0.0_amd64.deb
   
   # Install
   sudo dpkg -i apm-headphone_1.0.0_amd64.deb
   
   # Install dependencies (if needed)
   sudo apt-get install -f
   ```

2. **Launch from Applications Menu**
   - Search for "APM Headphone Translator"
   - Or run from terminal: `apm-headphone`

---

## 🎯 First Time Setup

### Step 1: Audio Configuration

1. **Connect Your Headphones**
   - Plug in or pair your headphones/headset
   - Make sure the microphone is working (test in system settings)

2. **Launch APM Application**
   - The app will detect your audio devices automatically

3. **Select Your Devices**
   ```
   ┌─────────────────────────────────────┐
   │     Audio Device Selection          │
   ├─────────────────────────────────────┤
   │ Microphone:                         │
   │ [Headset Microphone ▼]              │
   │                                     │
   │ Speakers:                           │
   │ [Headset Speakers ▼]                │
   │                                     │
   │        [Test Audio] [Continue]      │
   └─────────────────────────────────────┘
   ```

4. **Test Your Audio**
   - Click "Test Audio"
   - Speak into your microphone
   - You should hear your voice and see the level meter move

### Step 2: Language Selection

```
┌─────────────────────────────────────┐
│       Language Settings             │
├─────────────────────────────────────┤
│ I speak:                            │
│ [English (US) ▼]                    │
│                                     │
│ Translate to:                       │
│ [Spanish (Spain) ▼]                 │
│                                     │
│ [ ] Enable reverse translation      │
│     (Hear Spanish, output English)  │
│                                     │
│           [Save & Start]            │
└─────────────────────────────────────┘
```

**Supported Languages:**
- English (US, UK, Australia)
- Spanish (Spain, Latin America)
- Japanese
- French (France, Canada)
- German
- Chinese (Mandarin)
- Korean
- Italian
- Portuguese (Brazil, Portugal)
- Russian
- Arabic

### Step 3: Audio Quality Settings

```
┌─────────────────────────────────────┐
│       Performance Settings          │
├─────────────────────────────────────┤
│ Quality:                            │
│ ○ Fast (Lower quality, 50ms delay)  │
│ ● Balanced (Good quality, 200ms)    │
│ ○ Best (High quality, 500ms)        │
│                                     │
│ Noise Cancellation:                 │
│ [■■■■■■■□□□] 70%                    │
│                                     │
│ Echo Cancellation:                  │
│ [■■■■■■■■■■] 100%                   │
│                                     │
│              [Apply]                │
└─────────────────────────────────────┘
```

---

## 🚀 Using the Application

### Main Interface

```
┌───────────────────────────────────────────────────────┐
│  APM Headphone Translator                    [_][□][X] │
├───────────────────────────────────────────────────────┤
│                                                        │
│  Status: ● ACTIVE                                      │
│  English (US) → Spanish (Spain)                        │
│                                                        │
│  ┌──────────────────────────────────────────────┐     │
│  │ Input Volume:  [■■■■■■■■□□] 80%              │     │
│  │ Output Volume: [■■■■■□□□□□] 50%              │     │
│  │                                              │     │
│  │ Translation Quality: ★★★★☆ (Good)           │     │
│  │ Current Latency: 215ms                      │     │
│  └──────────────────────────────────────────────┘     │
│                                                        │
│  Recent Translations:                                  │
│  ┌──────────────────────────────────────────────┐     │
│  │ You: "Hello, how are you?"                   │     │
│  │ Out: "Hola, ¿cómo estás?"                    │     │
│  │ ─────────────────────────────────────────── │     │
│  │ You: "Nice to meet you"                      │     │
│  │ Out: "Encantado de conocerte"               │     │
│  └──────────────────────────────────────────────┘     │
│                                                        │
│  [ ⏸ Pause ]  [⚙ Settings]  [📊 Statistics]          │
│                                                        │
└───────────────────────────────────────────────────────┘
```

### Basic Operations

1. **Start Translation**
   - Click the big "Start" button
   - Speak normally into your microphone
   - Hear the translation in your headphones

2. **Pause/Resume**
   - Click "Pause" to temporarily stop translation
   - Click "Resume" to continue

3. **Switch Languages**
   - Click the language selector
   - Choose new source/target languages
   - Changes apply immediately

4. **Adjust Volume**
   - Use the sliders to control input/output levels
   - System volume controls also work

---

## 🎛️ Advanced Features

### Directional Focus Mode

Focus on voices from specific directions (requires multi-mic headset):

```
Settings → Audio → Enable Directional Focus
Direction: [◄ Front ►]
Angle: 0° (straight ahead)
```

### Background Noise Profiles

Optimize for different environments:

```
Settings → Noise Cancellation → Environment
• Office (keyboard, air conditioning)
• Café (background chatter, dishes)
• Street (traffic, wind)
• Quiet Room (minimal noise)
• Custom (adjust manually)
```

### Translation Memory

Save frequently used phrases:

```
Tools → Phrase Book → Add Entry
English: "Where is the bathroom?"
Spanish: "¿Dónde está el baño?"
[Save]
```

### Offline Mode

Download language packs for offline use:

```
Settings → Languages → Download Offline Pack
English ↔ Spanish: [Download] (85MB)
English ↔ Japanese: [Download] (120MB)
```

---

## 🔧 Troubleshooting

### No Sound / Microphone Not Working

**Windows:**
1. Right-click speaker icon → Sounds
2. Recording tab → Right-click your mic → Set as Default
3. Playback tab → Right-click your headphones → Set as Default
4. Restart APM application

**macOS:**
1. System Preferences → Sound → Input → Select your microphone
2. System Preferences → Sound → Output → Select your headphones
3. System Preferences → Security & Privacy → Microphone → Enable APM

**Linux:**
```bash
# Check audio devices
aplay -l
arecord -l

# Set default devices
pacmd set-default-sink <your-headphone-sink>
pacmd set-default-source <your-mic-source>
```

### Poor Translation Quality

1. **Check Microphone Position**
   - Keep mic 2-3 inches from mouth
   - Reduce background noise
   - Speak clearly and at normal pace

2. **Adjust Quality Settings**
   - Settings → Performance → "Best"
   - Increase noise cancellation level
   - Enable "Enhanced Speech Clarity"

3. **Update Language Models**
   - Help → Check for Updates
   - Download latest language packs

### High Latency / Lag

1. **Reduce Quality**
   - Settings → Performance → "Fast"
   - This reduces delay to ~50ms

2. **Close Other Applications**
   - Free up CPU and RAM
   - Check Task Manager/Activity Monitor

3. **Wired Instead of Bluetooth**
   - Bluetooth adds 100-200ms latency
   - Use wired headphones for lowest delay

### Echo or Feedback

1. **Enable Echo Cancellation**
   - Settings → Audio → Echo Cancellation → 100%

2. **Reduce Output Volume**
   - Lower headphone volume
   - Prevents sound leaking back to microphone

3. **Check Microphone Position**
   - Keep microphone away from speakers

---

## 📱 Mobile App (iOS/Android)

Coming soon! Sign up for beta access:
Email: dfeen87@gmail.com with Subject APMS.

---

## 💡 Tips for Best Results

### For Conversations
- **One speaker at a time**: Translation works best when one person speaks
- **Natural pace**: Don't speak too fast or too slow
- **Clear pronunciation**: Enunciate words clearly
- **Short sentences**: Break up long thoughts into shorter phrases

### For Different Scenarios

**Video Calls:**
```
1. Set APM as virtual microphone input
2. Use "Mixed Mode" to hear both original and translation
3. Enable "Call Quality" preset
```

**Live Events/Lectures:**
```
1. Use "Continuous Mode" for ongoing speech
2. Enable "Context Learning" for technical terms
3. Save transcript for later review
```

**Travel/Tourism:**
```
1. Download offline language packs before trip
2. Enable "Travel Phrases" quick access
3. Use "Slow Playback" to learn pronunciation
```

---

## 🔐 Privacy & Data

### Your Privacy Matters
- **Processing**: All audio processing happens on your device
- **No Cloud**: Without internet, your audio never leaves your computer
- **No Recording**: Audio is processed in real-time, not stored
- **Optional Cloud**: Online mode only for enhanced accuracy (opt-in)

### Data Usage
- **Offline Mode**: 0 bytes (everything local)
- **Online Mode**: ~50KB per minute of speech
- **Language Pack Downloads**: 80-150MB per language pair

---

## 🆘 Getting Help

### Support
- **Email**: dfeen87@gmail.com
- **Community Forum**: This Github Discussion Board

### Report Issues
- GitHub Issues: In this Repo's Issues Tab (Thank you.)

---

## 🔄 Updates

### Automatic Updates (Recommended)
```
Settings → Updates → ☑ Automatically download updates
```

### Manual Updates
```
Help → Check for Updates
```

**Current Version**: 1.0.0  

---

## 📜 System Requirements Details

### Minimum Requirements
| Component | Requirement |
|-----------|-------------|
| **OS** | Windows 10, macOS 10.15, Ubuntu 20.04 |
| **CPU** | Dual-core 2.0 GHz |
| **RAM** | 4 GB |
| **Disk** | 500 MB free space |
| **Audio** | Any headset with microphone |
| **Internet** | Optional (for online mode) |

### Recommended Requirements
| Component | Requirement |
|-----------|-------------|
| **OS** | Windows 11, macOS 13, Ubuntu 22.04 |
| **CPU** | Quad-core 2.5 GHz or better |
| **RAM** | 8 GB or more |
| **Disk** | 2 GB free space (for offline packs) |
| **Audio** | Quality headset, multi-mic preferred |
| **Internet** | Broadband (for updates & online mode) |

---

## 🎁 Quick Start Checklist

- [ ] Download and install APM application
- [ ] Connect headphones/headset
- [ ] Grant microphone permissions
- [ ] Select input/output devices
- [ ] Choose your languages
- [ ] Test audio levels
- [ ] Start translating!

**Ready to break language barriers? Let's get started!** 🌍🎧

---

*Version 1.0.0 | Last Updated: December 2025*  
*Copyright © 2025 Don Michael Feeney Jr. | Non-Commercial License*
