# Smartglasses A-P-M-S Pseudocode & Algorithm Specification

**Version:** 11.0.0 (V10 architecture preserved)
**Status:** Reference Engineering Specification
**Module Target:** `src/dsp/smartglasses_apm/smartglasses_apm.cpp` / `include/apm/smartglasses_apm.hpp`

---

## 1. Mathematical Model & Geometry

```
                      [ Symmetric Top-View Geometry ]

                              Mouth Source M (0, Y_m, Z_m)
                                         /\
                                        /  \
                           r_L         /    \        r_R
                                      /      \
                                     /        \
       Left Mic L (-D/2, 0, 0)   [ Left ]----[ Right ]   Right Mic R (+D/2, 0, 0)
                                      <-- D -->
```

* **Microphone Baseline ($D$):** $0.14 - 0.16\text{ m}$ ($14 - 16\text{ cm}$ between left/right temples).
* **Target Acoustic Source (Mouth):** $x_m = 0.0\text{ m}$, $y_m \approx 0.11\text{ m}$, $z_m \approx -0.09\text{ m}$.
* **Symmetry Property:** Target mouth speech arrives at both temples with zero path difference ($\Delta \tau = 0$, $\Delta \phi_{\text{target}}(\omega) \approx 0$).
* **Lateral Noise Sources:** Arrive off-axis with $\Delta \phi_{\text{noise}}(\omega) \neq 0$ and lower inter-temple coherence due to head shadow occlusion.

---

## 2. Core Processing Pipeline Pseudocode

```python
class SmartglassesAPM:
    def __init__(self, sample_rate=16000, frame_size=160, fft_size=256, baseline_m=0.15):
        self.sample_rate = sample_rate      # 16000 Hz
        self.frame_size = frame_size        # 10 ms = 160 samples
        self.fft_size = fft_size            # 256 points (zero-padded STFT)
        self.num_bins = fft_size // 2 + 1   # 129 real frequency bins
        self.g_min = 0.10                   # -20 dB attenuation gain floor
        self.alpha_smooth = 0.85            # Temporal IIR gain smoothing factor
        self.spatial_exp = 1.5              # Spatial projection aggressiveness

        # Pre-allocated working buffers (Zero dynamic allocation during runtime)
        self.window = hanning_window(fft_size)
        self.prev_gain = array_of_ones(self.num_bins)
        self.cos_table, self.sin_table = precompute_dft_trig_tables(fft_size, self.num_bins)

    def process_frame(self, mic_left_samples, mic_right_samples):
        """
        Processes 10 ms dual-channel temple microphone input.
        Inputs: mic_left_samples (160 floats), mic_right_samples (160 floats)
        Returns: out_mono (160 floats), metadata (VAD confidence, SNR dB, is_speech_active)
        """
        # --- Stage 0: Numerical Safety & Guard Checks ---
        if mic_left_samples is None or mic_right_samples is None:
            return array_of_zeros(160), Metadata(0.0, -100.0, False)

        if len(mic_left_samples) < 160 or len(mic_right_samples) < 160:
            return array_of_zeros(160), Metadata(0.0, -100.0, False)

        # Sanitize NaNs/Infs and extreme values
        mic_L_clean = sanitize_and_clamp(mic_left_samples[:160], min_val=-1.0, max_val=1.0)
        mic_R_clean = sanitize_and_clamp(mic_right_samples[:160], min_val=-1.0, max_val=1.0)

        # --- Stage 1: Windowed STFT Framing (Zero-padded to 256) ---
        win_L = array_of_zeros(256)
        win_R = array_of_zeros(256)
        for i in range(160):
            win_L[i] = mic_L_clean[i] * self.window[i]
            win_R[i] = mic_R_clean[i] * self.window[i]

        # --- Stage 2: Low-Complexity Real DFT ---
        L_fft = array_of_complex(129)
        R_fft = array_of_complex(129)
        for k in range(129):
            re_L, im_L = 0.0, 0.0
            re_R, im_R = 0.0, 0.0
            tbl_base = k * 256
            for n in range(256):
                c = self.cos_table[tbl_base + n]
                s = self.sin_table[tbl_base + n]
                re_L += win_L[n] * c
                im_L -= win_L[n] * s
                re_R += win_R[n] * c
                im_R -= win_R[n] * s
            L_fft[k] = Complex(re_L, im_L)
            R_fft[k] = Complex(re_R, im_R)

        # --- Stage 3: Geometric Projection & Phase Masking ---
        S_enhanced = array_of_complex(129)
        total_signal_power = 0.0
        total_noise_power = 0.0

        for k in range(129):
            # Cross-spectral density P_LR = L_fft[k] * conj(R_fft[k])
            P_lr = L_fft[k] * complex_conjugate(R_fft[k])
            phase_diff = angle(P_lr)

            mag_L = abs(L_fft[k]) + 1e-6
            mag_R = abs(R_fft[k]) + 1e-6

            # Phase Coherence Index gamma
            coherence = abs(P_lr) / (mag_L * mag_R)
            coherence = clamp(coherence, 0.0, 1.0)

            # Phase Mask centered on symmetrical mouth vector (phase_diff = 0)
            cos_phase = cos(phase_diff / 2.0)
            phase_mask = pow(abs(cos_phase), self.spatial_exp)

            inst_gain = phase_mask * coherence
            inst_gain = max(self.g_min, inst_gain)

            # Recursive Temporal IIR Gain Smoothing
            smooth_gain = self.alpha_smooth * self.prev_gain[k] + (1.0 - self.alpha_smooth) * inst_gain
            self.prev_gain[k] = smooth_gain

            # Mouth Vector Projection & Mask Application
            S_avg = 0.5 * (L_fft[k] + R_fft[k])
            S_enhanced[k] = S_avg * smooth_gain

            # Power metrics for metadata VAD/SNR
            raw_input_power = 0.5 * (squared_magnitude(L_fft[k]) + squared_magnitude(R_fft[k]))
            enhanced_power = squared_magnitude(S_enhanced[k])

            total_signal_power += enhanced_power
            total_noise_power += max(0.0, raw_input_power - enhanced_power)

        # --- Stage 4: Inverse DFT & Output Framing (160 samples) ---
        out_mono = array_of_zeros(160)
        for n in range(160):
            sample_acc = S_enhanced[0].real + S_enhanced[128].real * cos(PI * n)
            for k in range(1, 128):
                tbl_base = k * 256
                c = self.cos_table[tbl_base + n]
                s = self.sin_table[tbl_base + n]
                sample_acc += 2.0 * (S_enhanced[k].real * c - S_enhanced[k].imag * s)
            out_mono[n] = sample_acc / 256.0

        # --- Stage 5: Downstream AI Metadata Generation ---
        snr_ratio = (total_signal_power / total_noise_power) if total_noise_power > 1e-6 else 100.0
        estimated_snr_db = 10.0 * log10(max(0.001, snr_ratio))
        speech_confidence = clamp((estimated_snr_db - 3.0) / 15.0, 0.0, 1.0)
        is_speech_active = (speech_confidence > 0.35)

        metadata = Metadata(
            speech_confidence=speech_confidence,
            estimated_snr_db=estimated_snr_db,
            is_speech_active=is_speech_active
        )

        return out_mono, metadata
```

---

## 3. Downstream AI Assistant Ingestion Pseudocode

```python
def ai_assistant_ingestion_pipeline(apm_engine, pcm_interleaved_lr):
    """
    Consumes dual-channel PCM16 frames, processes them through A-P-M-S,
    and forwards clean 16 kHz mono frames to wake-word and streaming ASR.
    """
    pcm_out_mono = array_of_int16(160)
    metadata = Metadata()

    # Process 10 ms frame via PCM16 interface
    success = apm_engine.process_frame_pcm16(
        pcm_interleaved_lr,
        pcm_out_mono,
        metadata
    )

    if not success:
        return

    # 1. Forward to Wake-Word Engine (e.g. Sensory / Porcupine / PvRecorder)
    if metadata.is_speech_active:
        wakeword_engine.process_frame(pcm_out_mono)

    # 2. Forward to Streaming Speech Recognizer (e.g. Whisper / Kaldi / ONNX Runtime)
    asr_ring_buffer.write(pcm_out_mono)
    if asr_ring_buffer.available_ms() >= 100: # 100 ms chunk
        asr_engine.feed_audio_chunk(asr_ring_buffer.read_chunk(100))
```
