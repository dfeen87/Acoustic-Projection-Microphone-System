# Translation Bridge Implementation

## Overview

The APM System translation bridge connects C++ DSP pipeline with Python AI models (Whisper + NLLB) for real-time speech translation.

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    C++ APM System                           │
│  ┌───────────────────────────────────────────────────────┐  │
│  │  Audio Input → Beamforming → Echo Cancel → Denoise   │  │
│  └───────────────────────────────────────────────────────┘  │
└──────────────────────────┬──────────────────────────────────┘
                           │
                           ↓ audio samples (float[])
                           │
┌──────────────────────────┴──────────────────────────────────┐
│            LocalTranslationEngine (C++)                     │
│  ┌───────────────────────────────────────────────────────┐  │
│  │  1. Write samples to temp WAV file                    │  │
│  │  2. Execute: python3 translation_bridge.py audio.wav  │  │
│  │  3. Capture JSON output                               │  │
│  │  4. Parse and return result                           │  │
│  │  5. Cleanup temp file                                 │  │
│  └───────────────────────────────────────────────────────┘  │
└──────────────────────────┬──────────────────────────────────┘
                           │
                           ↓ subprocess call
                           │
┌──────────────────────────┴──────────────────────────────────┐
│         translation_bridge.py (Python)                      │
│  ┌───────────────────────────────────────────────────────┐  │
│  │  1. Load audio file                                    │  │
│  │  2. Whisper.transcribe(audio)                         │  │
│  │  3. NLLB.translate(text)                              │  │
│  │  4. Output JSON result                                │  │
│  └───────────────────────────────────────────────────────┘  │
└──────────────────────────┬──────────────────────────────────┘
                           │
                           ↓ JSON output
                           │
┌──────────────────────────┴──────────────────────────────────┐
│             TranslationResult (C++)                         │
│  {                                                          │
│    success: true,                                          │
│    transcribed_text: "Hello world",                        │
│    translated_text: "Hola mundo",                          │
│    confidence: 0.95                                        │
│  }                                                          │
└─────────────────────────────────────────────────────────────┘
```

## Files

### C++ Implementation
- `include/apm/local_translation_engine.hpp` - Header with Config and API
- `src/local_translation_engine.cpp` - Implementation with subprocess bridge
- `examples/translation_example.cpp` - Complete usage example
- `tests/test_translation_integration.cpp` - Integration tests

### Python Bridge
- `scripts/translation_bridge.py` - Whisper + NLLB wrapper with CLI
- `scripts/setup_translation.sh` - One-command setup script

## Quick Start

### 1. Setup (First Time Only)

```bash
# Install translation models
./scripts/setup_translation.sh

# This installs:
# - Python virtual environment
# - Whisper (speech recognition)
# - NLLB (translation)
# - Downloads models (~2GB)
```

### 2. Build APM with Translation Support

```bash
# Build the project
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
```

### 3. Run Example

```bash
# Run the complete example
./build/translation_example

# Or run integration tests
./build/test_translation
```

### 4. Use in Your Code

```cpp
#include "apm/local_translation_engine.hpp"

// Configure
apm::LocalTranslationEngine::Config config;
config.source_language = "en";
config.target_language = "es";
config.script_path = "scripts/translation_bridge.py";

// Create engine
apm::LocalTranslationEngine engine(config);

// Translate audio
std::vector<float> audio_samples = ...; // Your audio
auto result = engine.translate(audio_samples, 16000);

if (result.success) {
    std::cout << "Original: " << result.transcribed_text << "\n";
    std::cout << "Translated: " << result.translated_text << "\n";
}
```

## How It Works

### C++ Side (local_translation_engine.cpp)

1. **WAV File Generation**
   ```cpp
   bool write_wav_file(const string& filename, 
                      const vector<float>& samples, 
                      int sample_rate);
   ```
   - Converts float samples to 16-bit PCM
   - Writes proper WAV header
   - Stores in /tmp with unique filename

2. **Python Subprocess Execution**
   ```cpp
   string cmd = "python3 scripts/translation_bridge.py audio.wav "
                "--source en --target es --json 2>/dev/null";
   string output = exec_command(cmd);
   ```
   - Spawns Python subprocess
   - Captures stdout (JSON result)
   - Suppresses stderr (model loading messages)

3. **JSON Parsing**
   ```cpp
   result.transcribed_text = simple_json::extract_field(json, "transcribed_text");
   result.translated_text = simple_json::extract_field(json, "translated_text");
   result.success = simple_json::extract_bool(json, "success");
   ```
   - Simple hand-rolled JSON parser (no external deps)
   - Extracts key fields from response

4. **Cleanup**
   ```cpp
   std::remove(temp_wav_path.c_str());
   ```
   - Deletes temporary WAV file

### Python Side (translation_bridge.py)

1. **Model Loading**
   ```python
   self.whisper_model = whisper.load_model("small")
   self.nllb_model = AutoModelForSeq2SeqLM.from_pretrained(
       "facebook/nllb-200-distilled-600M"
   )
   ```

2. **Transcription**
   ```python
   result = whisper_model.transcribe(audio_path, language="en")
   transcribed = result["text"]
   ```

3. **Translation**
   ```python
   inputs = nllb_tokenizer(text, return_tensors="pt")
   outputs = nllb_model.generate(**inputs, 
       forced_bos_token_id=tokenizer.lang_code_to_id[target_lang])
   translated = nllb_tokenizer.decode(outputs[0])
   ```

4. **JSON Output**
   ```python
   print(json.dumps({
       "transcribed_text": transcribed,
       "translated_text": translated,
       "success": True
   }))
   ```

## Performance

### Latency Breakdown

| Component | Time (CPU) | Time (GPU) |
|-----------|-----------|-----------|
| WAV file write | ~5ms | ~5ms |
| Python startup | ~500ms | ~500ms |
| Whisper transcribe | 4-6 sec | 1-2 sec |
| NLLB translate | 1-2 sec | 0.5-1 sec |
| JSON parsing | ~1ms | ~1ms |
| **Total** | **5-8 sec** | **2-4 sec** |

### Memory Usage

- Whisper (small): ~244 MB
- NLLB (distilled): ~1.2 GB
- Audio buffer: negligible
- **Total**: ~1.5 GB RAM

### Optimization Tips

1. **Use GPU** - 2-3x faster than CPU
   ```cpp
   config.use_gpu = true;
   ```

2. **Smaller Whisper Model** - Faster but less accurate
   ```cpp
   config.whisper_model_path = "tiny";  // 39MB, fastest
   ```

3. **Keep Python Process Running** - Future optimization
   - Current: spawn new process each time
   - Future: persistent Python process via pipe/socket

## Troubleshooting

### "Python script not found"
```bash
# Verify script exists
ls -la scripts/translation_bridge.py

# If missing, check you're in project root
pwd
```

### "Whisper not installed"
```bash
# Run setup script
./scripts/setup_translation.sh

# Or manually install
source venv/bin/activate
pip install openai-whisper transformers torch
```

### "CUDA out of memory"
```cpp
// Use CPU instead
config.use_gpu = false;

// Or use smaller model
config.whisper_model_path = "tiny";
```

### "Translation timeout"
```cpp
// Increase timeout (future feature)
config.timeout_ms = 30000;  // 30 seconds
```

### Slow performance on CPU
- Expected: 5-8 seconds per sentence
- Consider GPU upgrade for real-time use
- Or use cloud API version (coming in v2.5)

## Advanced Usage

### Async Translation (Non-Blocking)

```cpp
// Start translation in background
auto future = engine.translate_async(audio, 16000);

// Do other work...
process_next_audio_frame();

// Get result when ready
auto result = future.get();
```

### Batch Processing

```cpp
std::vector<std::vector<float>> audio_batch = ...;

for (const auto& audio : audio_batch) {
    auto result = engine.translate(audio, 16000);
    save_result(result);
}
```

### Custom Models

```cpp
config.whisper_model_path = "/path/to/custom/whisper";
config.nllb_model_path = "/path/to/custom/nllb";
```

### Language Detection (Auto-detect source)

```python
# In translation_bridge.py, omit language parameter
result = whisper_model.transcribe(audio_path)
detected_lang = result["language"]
```

## Future Improvements (v2.5+)

1. **Persistent Python Process**
   - Keep Python running, communicate via pipes
   - Eliminates 500ms startup overhead
   - 10x faster for repeated translations

2. **Direct Memory Sharing**
   - Use pybind11 for zero-copy audio transfer
   - No WAV file I/O overhead
   - ~100x faster data transfer

3. **Streaming Translation**
   - Translate audio chunks as they arrive
   - Lower latency for real-time conversations

4. **GPU Optimization**
   - Batch multiple translations
   - Mixed-precision inference (FP16)
   - Model quantization (INT8)

5. **Cloud API Fallback**
   - Optional Google/Azure backup
   - Automatic failover if local fails
   - Cost-aware routing

## License

MIT License - see LICENSE file

## Credits

- OpenAI Whisper: https://github.com/openai/whisper
- Meta NLLB: https://github.com/facebookresearch/fairseq/tree/nllb
- APM System: Don Michael Feeney Jr.
