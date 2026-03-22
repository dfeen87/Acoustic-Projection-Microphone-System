# 🎧 APM Headphone Translator - Download Package

**Turn your ordinary headphones into a real-time translation device!**
**Real‑time translation for any headphones. Offline or online. Free and open source.**

---

## 📦 What's in This Package?

This download package contains everything you need to transform your headphones into a powerful real-time translation tool:

### Core Files
1. **Installer/Setup Files**
   - `APM-Headphone-Setup.exe` (Windows)
   - `APM-Headphone.dmg` (macOS)
   - `apm-headphone_1.0.0_amd64.deb` (Ubuntu/Debian)
   - `user-setup.sh` (Linux universal installer)

2. **Configuration Tool**
   - `apm-config-gui.py` - Easy-to-use setup wizard
   - Pre-configured for most common scenarios

3. **Documentation**
   - `INSTALL_USER.md` - Complete user guide
   - `QUICK_START.pdf` - Get started in 5 minutes
   - `TROUBLESHOOTING.pdf` - Common issues and solutions

4. **Optional Language Packs** (in `models/` folder)
   - English ↔ Spanish (85MB)
   - English ↔ Japanese (120MB)
   - English ↔ French (95MB)
   - English ↔ German (90MB)
   - English ↔ Chinese (110MB)

---

## 🚀 Quick Installation

### Windows (Easiest)

1. **Download** `APM-Headphone-Setup.exe`
2. **Double-click** to run installer
3. **Follow** the setup wizard
4. **Launch** from Start Menu
5. **Configure** your headphones
6. **Start translating!**

**Download Size**: 150MB  
**Installation Time**: 2-3 minutes  
**Requires**: Windows 10 or newer

### macOS

1. **Download** `APM-Headphone.dmg`
2. **Open** the DMG file
3. **Drag** app to Applications
4. **Right-click** → Open (first time)
5. **Grant** microphone permissions
6. **Start translating!**

**Download Size**: 120MB  
**Installation Time**: 1-2 minutes  
**Requires**: macOS 10.15 or newer

### Linux (Ubuntu/Debian)

```bash
# Quick one-line install
wget -qO- https://get.apm-system.com | bash
```

Or manually:

```bash
# Download package
wget https://github.com/yourrepo/apm/releases/latest/apm-headphone_1.0.0_amd64.deb

# Install
sudo dpkg -i apm-headphone_1.0.0_amd64.deb
sudo apt-get install -f  # Install dependencies

# Run
apm-headphone
```

**Download Size**: 80MB  
**Installation Time**: 2-3 minutes  
**Requires**: Ubuntu 20.04+ or Debian 11+

---

## 🎯 First-Time Setup (All Platforms)

### 1. Connect Your Headphones

- Plug in wired headphones, or
- Pair Bluetooth headphones
- Make sure microphone is working

### 2. Run Configuration Tool

**Windows/macOS:**
- Installer will launch configuration automatically

**Linux:**
```bash
python3 apm-config-gui.py
# Or if installed: apm-headphone --config
```

### 3. Configure Audio

```
┌─────────────────────────────────┐
│  Select Microphone:             │
│  [Your Headset Mic ▼]           │
│                                 │
│  Select Speakers:               │
│  [Your Headphones ▼]            │
│                                 │
│  [Test Audio] [Continue]        │
└─────────────────────────────────┘
```

### 4. Choose Languages

```
┌─────────────────────────────────┐
│  I speak:                       │
│  [English (US) ▼]               │
│                                 │
│  Translate to:                  │
│  [Spanish (Spain) ▼]            │
│                                 │
│  [Start Translation]            │
└─────────────────────────────────┘
```

### 5. Start Using!

Speak naturally into your microphone and hear instant translation in your headphones!

---

## 📥 What to Download?

### For Most Users (Recommended)

**Download the main installer only:**
- Windows: `APM-Headphone-Setup.exe` (150MB)
- macOS: `APM-Headphone.dmg` (120MB)
- Linux: Use one-line install command

This includes:
✅ Core application  
✅ Online translation (requires internet)  
✅ Configuration tool  
✅ Documentation  

### For Offline Use

**Also download language packs:**

Choose the language pairs you need from the `models/` folder:

- `models-en-es.tar.gz` - English ↔ Spanish (85MB)
- `models-en-ja.tar.gz` - English ↔ Japanese (120MB)
- `models-en-fr.tar.gz` - English ↔ French (95MB)
- `models-en-de.tar.gz` - English ↔ German (90MB)
- `models-en-zh.tar.gz` - English ↔ Chinese (110MB)

**Extract to:**
- Windows: `C:\Program Files\APM\models\`
- macOS: `/Applications/APM Headphone.app/Contents/Resources/models/`
- Linux: `~/.local/share/apm-headphone/models/`

### For Developers

**Download the source code:**
- `apm-source-1.0.0.tar.gz` (5MB)
- See `BUILD.md` for compilation instructions

---

## 💾 System Requirements

### Minimum (Online Mode)
- **OS**: Windows 10, macOS 10.15, Ubuntu 20.04
- **RAM**: 4 GB
- **Disk**: 500 MB
- **CPU**: Dual-core 2.0 GHz
- **Audio**: Any headset with microphone
- **Internet**: Required for online translation

### Recommended (Offline Mode)
- **RAM**: 8 GB
- **Disk**: 2 GB (for language packs)
- **CPU**: Quad-core 2.5 GHz
- **Audio**: Quality headset
- **Internet**: Optional

---

## 🌍 Supported Languages

| From/To | Spanish | Japanese | French | German | Chinese | Korean | Italian | Portuguese | Russian | Arabic |
|---------|---------|----------|--------|--------|---------|--------|---------|------------|---------|--------|
| **English** | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| **Spanish** | - | ⚠️ | ✅ | ⚠️ | ⚠️ | ⚠️ | ✅ | ✅ | ⚠️ | ⚠️ |

✅ Fully supported with offline models  
⚠️ Online only (no offline pack available)

---

## 🎮 Usage Scenarios

### 💼 Business Meetings
```
Mode: Online (for best quality)
Quality: Best (500ms latency acceptable)
Noise Cancellation: High (70-100%)
```

### 🎮 Gaming Voice Chat
```
Mode: Online
Quality: Fast (50ms low latency)
Noise Cancellation: Medium (50%)
Echo Cancellation: High
```

### ✈️ Travel/Tourism
```
Mode: Offline (download packs before trip)
Quality: Balanced
Noise Cancellation: High (outdoor noise)
```

### 📚 Learning Languages
```
Mode: Online
Quality: Best
Enable: Show Transcripts
Enable: Save Translation History
```

---

## 🔧 Configuration Files Location

### Windows
```
%APPDATA%\APMHeadphone\settings.json
```

### macOS
```
~/Library/Application Support/APMHeadphone/settings.json
```

### Linux
```
~/.config/apm-headphone/settings.json
```

You can edit these files directly or use the GUI configuration tool.

---

## 🆘 Need Help?

### Quick Solutions

**No sound?**
1. Check headphone connection
2. Verify device selection in settings
3. Test audio devices in system settings

**Poor translation quality?**
1. Speak clearly at normal pace
2. Increase noise cancellation
3. Switch to "Best" quality mode
4. Try online mode if using offline

**High latency/lag?**
1. Close other applications
2. Switch to "Fast" quality mode
3. Use wired instead of Bluetooth headphones

### Support Resources

- 📖 Full Manual: See `INSTALL_USER.md`
- 🎥 Video Tutorials: [youtube.com/apm-tutorials](https://youtube.com/apm-tutorials)
- 💬 Community Forum: [forum.apm-system.com](https://forum.apm-system.com)
- 📧 Email Support: support@apm-system.com
- 🐛 Report Bugs: [github.com/yourrepo/apm/issues](https://github.com/yourrepo/apm/issues)

---

## 🔐 Privacy & Security

### Your Data is Safe

✅ **Local Processing**: Audio processed on your device  
✅ **No Recording**: Real-time processing, nothing stored  
✅ **Optional Cloud**: Only when using online mode  
✅ **No Tracking**: We don't collect usage data  
✅ **Open Source**: Code available for review  

### Online Mode

When using online translation:
- Audio sent to translation servers
- Encrypted in transit (HTTPS)
- Not stored after translation
- Used only for translation quality

You can use 100% offline mode by downloading language packs.

---

## 📊 Feature Comparison

| Feature | Online Mode | Offline Mode |
|---------|------------|--------------|
| **Internet Required** | ✅ Yes | ❌ No |
| **Translation Quality** | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐ |
| **Speed** | 200-500ms | 100-300ms |
| **Download Size** | 150MB | 800MB+ |
| **Languages** | 10+ pairs | 5 pairs |
| **Privacy** | Moderate | High |
| **Cost** | Free | Free |

---

## 🎁 What's Included

### Free Features (Forever)
✅ Unlimited translation time  
✅ All supported language pairs  
✅ Noise cancellation  
✅ Echo cancellation  
✅ Voice activity detection  
✅ Text transcripts  
✅ Translation history  
✅ Offline mode (with downloaded packs)  

### No Hidden Costs
- No subscriptions
- No ads
- No data collection for profit
- Completely free and open source

---

## 🚀 Getting Started Checklist

Before you start, make sure you have:

- [ ] Downloaded installer for your OS
- [ ] Headphones with working microphone
- [ ] 500MB+ free disk space
- [ ] Internet connection (for initial setup)
- [ ] 5 minutes for setup

Then follow:

1. [ ] Install application
2. [ ] Run configuration tool
3. [ ] Select audio devices
4. [ ] Choose languages
5. [ ] Test audio
6. [ ] Start translating!

---

## 📱 Mobile App Coming Soon

iOS and Android versions in development!

**Sign up for beta**:
- iOS: [testflight.apple.com/apm](https://testflight.apple.com/apm)
- Android: [play.google.com/apps/testing/apm](https://play.google.com/apps/testing/apm)

---

## 💝 Support the Project

APM Headphone Translator is free and open source.

**Ways to help:**
- ⭐ Star on [GitHub](https://github.com/yourrepo/apm)
- 🐛 Report bugs and issues
- 💡 Suggest features
- 📝 Improve documentation
- 🌍 Help translate the app
- ☕ [Buy us a coffee](https://ko-fi.com/apm)

---

## 📄 License

Non-Commercial License - Free to use, modify, and distribute for non-commercial purposes

Copyright © 2025 Don Michael Feeney Jr.

See LICENSE file for full terms.

---

## 🙏 Credits

**Created by**: Don Michael Feeney Jr.  
**Dedicated to**: Marcel Krüger  
**Enhanced with**: Claude (Anthropic)  

**Special Thanks**:
- FFTW developers (Matteo Frigo & Steven G. Johnson)
- TensorFlow Lite team (Google)
- Open source community

---

**Version**: 1.0.0  
**Release Date**: December 2025  
**Last Updated**: December 11, 2025

### 1.0.0
- Initial release
- Offline + online translation
- Noise/echo cancellation
- Multi‑platform installers

---

<div align="center">

### Ready to break language barriers? 🌍

**[Download Now](#-what-to-download) | [View Documentation](INSTALL_USER.md) | [Get Support](#-need-help)**

</div>
