#!/usr/bin/env python3
"""
Local Translation Bridge for APM System
Uses Whisper for speech recognition and NLLB for translation
100% local processing - no cloud APIs
"""

import sys
import json
import numpy as np
import torch
from pathlib import Path

# Check if models are available
try:
    import whisper
    WHISPER_AVAILABLE = True
except ImportError:
    WHISPER_AVAILABLE = False
    print("WARNING: Whisper not installed. Run: pip install openai-whisper", file=sys.stderr)

try:
    from transformers import AutoTokenizer, AutoModelForSeq2SeqLM
    NLLB_AVAILABLE = True
except ImportError:
    NLLB_AVAILABLE = False
    print("WARNING: Transformers not installed. Run: pip install transformers", file=sys.stderr)


class LocalTranslationEngine:
    """Local translation engine using Whisper + NLLB"""
    
    def __init__(self, whisper_model="small", nllb_model="facebook/nllb-200-distilled-600M", device=None):
        """
        Initialize translation engine
        
        Args:
            whisper_model: Whisper model size (tiny, base, small, medium, large)
            nllb_model: NLLB model from HuggingFace
            device: "cuda" or "cpu" (auto-detect if None)
        """
        self.device = device or ("cuda" if torch.cuda.is_available() else "cpu")
        print(f"Using device: {self.device}", file=sys.stderr)
        
        # Load Whisper
        if WHISPER_AVAILABLE:
            print(f"Loading Whisper model: {whisper_model}...", file=sys.stderr)
            self.whisper_model = whisper.load_model(whisper_model, device=self.device)
            print("Whisper loaded successfully", file=sys.stderr)
        else:
            self.whisper_model = None
        
        # Load NLLB
        if NLLB_AVAILABLE:
            print(f"Loading NLLB model: {nllb_model}...", file=sys.stderr)
            self.nllb_tokenizer = AutoTokenizer.from_pretrained(nllb_model)
            self.nllb_model = AutoModelForSeq2SeqLM.from_pretrained(nllb_model).to(self.device)
            print("NLLB loaded successfully", file=sys.stderr)
        else:
            self.nllb_tokenizer = None
            self.nllb_model = None
    
    def transcribe(self, audio_path, source_language="en"):
        """
        Transcribe audio using Whisper
        
        Args:
            audio_path: Path to audio file
            source_language: Source language code
            
        Returns:
            Transcribed text
        """
        if not self.whisper_model:
            raise RuntimeError("Whisper not available")
        
        result = self.whisper_model.transcribe(
            audio_path,
            language=source_language,
            fp16=(self.device == "cuda")
        )
        
        return result["text"].strip()
    
    def translate(self, text, source_lang="eng_Latn", target_lang="spa_Latn"):
        """
        Translate text using NLLB
        
        Args:
            text: Text to translate
            source_lang: Source language code (NLLB format: eng_Latn)
            target_lang: Target language code (NLLB format: spa_Latn)
            
        Returns:
            Translated text
        """
        if not self.nllb_model or not self.nllb_tokenizer:
            raise RuntimeError("NLLB not available")
        
        # Tokenize
        self.nllb_tokenizer.src_lang = source_lang
        inputs = self.nllb_tokenizer(text, return_tensors="pt").to(self.device)
        
        # Translate
        translated_tokens = self.nllb_model.generate(
            **inputs,
            forced_bos_token_id=self.nllb_tokenizer.lang_code_to_id[target_lang],
            max_length=512
        )
        
        # Decode
        translated_text = self.nllb_tokenizer.batch_decode(
            translated_tokens, skip_special_tokens=True
        )[0]
        
        return translated_text
    
    def transcribe_and_translate(self, audio_path, source_lang="en", target_lang="es"):
        """
        Complete pipeline: audio -> transcription -> translation
        
        Args:
            audio_path: Path to audio file
            source_lang: Source language (ISO 639-1)
            target_lang: Target language (ISO 639-1)
            
        Returns:
            dict with transcribed_text and translated_text
        """
        # Convert language codes to NLLB format
        lang_map = {
            "en": "eng_Latn", "es": "spa_Latn", "fr": "fra_Latn",
            "de": "deu_Latn", "it": "ita_Latn", "pt": "por_Latn",
            "ru": "rus_Cyrl", "zh": "zho_Hans", "ja": "jpn_Jpan",
            "ko": "kor_Hang", "ar": "arb_Arab", "hi": "hin_Deva"
        }
        
        nllb_source = lang_map.get(source_lang, "eng_Latn")
        nllb_target = lang_map.get(target_lang, "spa_Latn")
        
        # Transcribe
        transcribed = self.transcribe(audio_path, source_lang)
        
        # Translate
        translated = self.translate(transcribed, nllb_source, nllb_target)
        
        return {
            "transcribed_text": transcribed,
            "translated_text": translated,
            "source_language": source_lang,
            "target_language": target_lang,
            "success": True
        }


def main():
    """Command-line interface for translation"""
    import argparse
    
    parser = argparse.ArgumentParser(description="Local Translation Engine")
    parser.add_argument("audio_file", help="Audio file to transcribe/translate")
    parser.add_argument("--source", default="en", help="Source language code")
    parser.add_argument("--target", default="es", help="Target language code")
    parser.add_argument("--whisper-model", default="small", help="Whisper model size")
    parser.add_argument("--device", choices=["cuda", "cpu"], help="Device to use")
    parser.add_argument("--json", action="store_true", help="Output as JSON")
    
    args = parser.parse_args()
    
    # Initialize engine
    engine = LocalTranslationEngine(
        whisper_model=args.whisper_model,
        device=args.device
    )
    
    # Process
    result = engine.transcribe_and_translate(
        args.audio_file,
        source_lang=args.source,
        target_lang=args.target
    )
    
    # Output
    if args.json:
        print(json.dumps(result, indent=2))
    else:
        print(f"\nTranscribed ({args.source}): {result['transcribed_text']}")
        print(f"Translated ({args.target}): {result['translated_text']}")


if __name__ == "__main__":
    main()
