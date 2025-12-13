# Local Translation Quick Start

## 🔒 100% Local, 100% Private

APM System now includes fully local translation using:
- **Whisper** for speech recognition (OpenAI, open-source)
- **NLLB** for translation (Meta, 200+ languages)

**No cloud APIs. No data collection. Your conversations stay on your device.**

---

## Quick Setup (5 minutes)

### 1. Install Translation Models

```bash
# Run the setup script
chmod +x scripts/setup_translation.sh
./scripts/setup_translation.sh
```

This will:
- Create a Python virtual environment
- Install Whisper + NLLB
- Download models (~2GB total)
- Configure GPU if available

### 2. Test Translation

```bash
# Activate environment
source venv/bin/activate

# Test with an audio file
python3 scripts/translation_bridge.py test_audio.wav \
  --source en \
  --target es
```

### 3. Use in APM System

```cpp
#include "apm/local_translation_engine.hpp"

// Initialize
apm::LocalTranslationEngine::Config config;
config.source_language = "en";
config.target_language = "es";
config.use_gpu = true;

apm::LocalTranslationEngine engine(config);

// Translate
std::vector<float> audio_samples = ...; // Your audio
auto result = engine.translate(audio_samples, 48000);

if (result.success) {
    std::cout << "Original: " << result.transcribed_text << "\n";
    std::cout << "Translated: " << result.translated_text << "\n";
}
```

---

## Supported Languages

**200+ languages** including:
- 🇬🇧 English (en)
- 🇪🇸 Spanish (es)
- 🇫🇷 French (fr)
- 🇩🇪 German (de)
- 🇮🇹 Italian (it)
- 🇵🇹 Portuguese (pt)
- 🇷🇺 Russian (ru)
- 🇨🇳 Chinese (zh)
- 🇯🇵 Japanese (ja)
- 🇰🇷 Korean (ko)
- 🇸🇦 Arabic (ar)
- 🇮🇳 Hindi (hi)
- And 180+ more!

---

## Performance

### Hardware Requirements

**Minimum (CPU only):**
- 4GB RAM
- 2GB disk space for models
- Processing: ~5-8 seconds per sentence

**Recommended (with GPU):**
- NVIDIA GPU with 4GB+ VRAM
- CUDA support
- Processing: ~2-4 seconds per sentence

### Model Sizes

| Whisper Model | Size | Speed | Accuracy |
|--------------|------|-------|----------|
| tiny | 39MB | Fastest | Good |
| base | 74MB | Fast | Better |
| **small** | 244MB | **Balanced** | **Best** ⭐ |
| medium | 769MB | Slow | Excellent |
| large | 1.5GB | Slowest | Best |

**Default: small** (best balance for real-time use)

---

## Privacy Guarantee

✅ **All processing happens on your device**
- Speech recognition: Local Whisper model
- Translation: Local NLLB model
- No internet connection required after setup
- No data sent to cloud services
- No API keys needed
- No usage tracking

---

## Troubleshooting

### "CUDA out of memory"
```bash
# Use smaller Whisper model
python3 scripts/translation_bridge.py audio.wav \
  --whisper-model tiny \
  --source en --target es
```

### "Models not found"
```bash
# Re-run setup
./scripts/setup_translation.sh
```

### Slow on CPU
Consider:
- Using `tiny` or `base` Whisper models
- Processing audio in chunks
- Upgrading to GPU

---

## Advanced Usage

### Async Translation (C++)

```cpp
// Non-blocking translation
auto future = engine.translate_async(audio_samples, 48000);

// Do other work...

// Get result when ready
auto result = future.get();
```

### Custom Models

```cpp
config.whisper_model_path = "/path/to/custom/whisper";
config.nllb_model_path = "/path/to/custom/nllb";
```

### Batch Processing

```python
# Process multiple files
for audio_file in audio_files:
    result = engine.transcribe_and_translate(audio_file, "en", "es")
    print(result['translated_text'])
```

---

## Next Steps

1. ✅ Run `./scripts/setup_translation.sh`
2. ✅ Test with sample audio
3. ✅ Integrate into your APM pipeline
4. 🚀 Build real-time multilingual communication!

**Questions?** Open an issue on GitHub!
