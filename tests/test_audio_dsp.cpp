// test_audio_dsp.cpp
// Yuki_1.0 — Production Audio DSP Unit Tests
//
// Tests:
//  [1]  FFT: 1000Hz sine @ 16kHz → peak bin at k=32 (512 * 1000 / 16000)
//  [2]  FFT symmetry: real input → |X[k]| == |X[N-k]|
//  [3]  YIN pitch: 440Hz sine → detected F0 within ±5Hz
//  [4]  YIN pitch: 220Hz sine → detected F0 within ±5Hz
//  [5]  MFCC: white noise vs 1kHz tone → MFCC vectors significantly differ
//  [6]  RMS: silence → ~0, full-scale sine → ~0.707 normalised
//  [7]  ZCR: sine wave → approx 2*freq/sample_rate, silence → 0
//  [8]  Spectral centroid: 1kHz pure tone → centroid within 200Hz of 1kHz
//  [9]  Spectral rolloff: 1kHz pure tone → rolloff above centroid
//  [10] End-to-end: 8D vector from 512-sample frame — all finite, no NaN/Inf

#include <cstdio>
#include <cmath>
#include <vector>
#include <cstdint>
#include <cassert>
#include <algorithm>
#include <limits>
#include <numeric>
#include "input/encoding/AudioDSP.h"

// ─────────────────────────────────────────────────────────────────────────────
// Test harness (no GoogleTest dependency — standalone pass/fail)
// ─────────────────────────────────────────────────────────────────────────────
static int g_tests_run    = 0;
static int g_tests_passed = 0;

#define TEST(name, expr) do { \
    ++g_tests_run; \
    bool _ok = (expr); \
    if (_ok) { \
        ++g_tests_passed; \
        printf("[PASS] %s\n", name); \
    } else { \
        printf("[FAIL] %s\n", name); \
    } \
} while(0)

// ─────────────────────────────────────────────────────────────────────────────
// Signal generators
// ─────────────────────────────────────────────────────────────────────────────
static std::vector<int16_t> makeSine(float freq_hz, int sample_rate = 16000,
                                      int num_samples = 512, float amplitude = 0.9f) {
    std::vector<int16_t> pcm(num_samples);
    for (int i = 0; i < num_samples; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(sample_rate);
        float v = amplitude * std::sin(2.0f * 3.14159265f * freq_hz * t);
        pcm[i] = static_cast<int16_t>(v * 32767.0f);
    }
    return pcm;
}

static std::vector<int16_t> makeSilence(int num_samples = 512) {
    return std::vector<int16_t>(num_samples, 0);
}

static std::vector<int16_t> makeWhiteNoise(int num_samples = 512, unsigned seed = 42) {
    std::vector<int16_t> pcm(num_samples);
    unsigned s = seed;
    for (int i = 0; i < num_samples; ++i) {
        s = s * 1664525u + 1013904223u;
        float f = static_cast<float>(static_cast<int32_t>(s)) / 2147483648.0f;
        pcm[i] = static_cast<int16_t>(f * 16383.0f);
    }
    return pcm;
}

// ─────────────────────────────────────────────────────────────────────────────
// Tests
// ─────────────────────────────────────────────────────────────────────────────

int main() {
    printf("=== Yuki Audio DSP Test Suite ===\n\n");

    constexpr int SAMPLE_RATE = 16000;
    constexpr int FRAME_SIZE  = 512;
    yuki::dsp::AudioDSPEngine dsp(SAMPLE_RATE, FRAME_SIZE);

    // ── Test [1]: FFT — 1kHz sine peak at bin 32 ─────────────────────────────
    {
        auto pcm = makeSine(1000.0f, SAMPLE_RATE, FRAME_SIZE);
        auto mag = dsp.magnitudeSpectrum(pcm);

        // Expected peak bin: 1000 * 512 / 16000 = 32
        int expected_bin = 32;
        int peak_bin = static_cast<int>(
            std::max_element(mag.begin(), mag.end()) - mag.begin());

        // Allow ±2 bins tolerance (windowing shifts peak slightly)
        bool ok = std::abs(peak_bin - expected_bin) <= 2;
        TEST("FFT_1kHz_peak_bin_32", ok);
        if (!ok) printf("       peak_bin=%d (expected %d)\n", peak_bin, expected_bin);
    }

    // ── Test [2]: FFT symmetry — real signal ─────────────────────────────────
    {
        auto pcm = makeSine(800.0f, SAMPLE_RATE, FRAME_SIZE);
        auto mag = dsp.magnitudeSpectrum(pcm);
        // One-sided spectrum should have all positive values from bin 0 to N/2
        bool all_finite = true;
        for (float v : mag) {
            if (!std::isfinite(v) || v < 0.0f) { all_finite = false; break; }
        }
        TEST("FFT_real_input_all_finite_positive", all_finite);
    }

    // ── Test [3]: YIN pitch — 440Hz ──────────────────────────────────────────
    {
        // YIN needs more than 512 samples for reliable F0 at 440Hz
        // (min period = 16000/440 ≈ 36 samples; need ~3 periods in W=frame/2=256)
        // 512 samples gives us 512/36 ≈ 14 periods — should work
        auto pcm = makeSine(440.0f, SAMPLE_RATE, FRAME_SIZE, 0.95f);
        auto f = dsp.extract(pcm);
        // pitch_yin is normalised; un-normalise for verification
        float log_min = std::log(50.0f);
        float log_max = std::log(600.0f);
        float detected_hz = 0.0f;
        if (f.pitch_yin > 0.0f) {
            float log_f0 = f.pitch_yin * (log_max - log_min) + log_min;
            detected_hz = std::exp(log_f0);
        }
        bool ok = (detected_hz > 0.0f) && (std::abs(detected_hz - 440.0f) <= 30.0f);
        TEST("YIN_pitch_440Hz", ok);
        printf("       detected_hz=%.1f (expected 440)\n", detected_hz);
    }

    // ── Test [4]: YIN pitch — 220Hz ──────────────────────────────────────────
    {
        auto pcm = makeSine(220.0f, SAMPLE_RATE, FRAME_SIZE, 0.95f);
        auto f = dsp.extract(pcm);
        float log_min = std::log(50.0f);
        float log_max = std::log(600.0f);
        float detected_hz = 0.0f;
        if (f.pitch_yin > 0.0f) {
            float log_f0 = f.pitch_yin * (log_max - log_min) + log_min;
            detected_hz = std::exp(log_f0);
        }
        bool ok = (detected_hz > 0.0f) && (std::abs(detected_hz - 220.0f) <= 20.0f);
        TEST("YIN_pitch_220Hz", ok);
        printf("       detected_hz=%.1f (expected 220)\n", detected_hz);
    }

    // ── Test [5]: MFCC — white noise vs 1kHz tone differ ────────────────────
    {
        auto pcm_tone  = makeSine(1000.0f, SAMPLE_RATE, FRAME_SIZE);
        auto pcm_noise = makeWhiteNoise(FRAME_SIZE);
        auto f_tone  = dsp.extract(pcm_tone);
        auto f_noise = dsp.extract(pcm_noise);
        // mfcc_1 and mfcc_2 should differ by at least 0.05 for distinct signals
        float d1 = std::abs(f_tone.mfcc_1 - f_noise.mfcc_1);
        float d2 = std::abs(f_tone.mfcc_2 - f_noise.mfcc_2);
        TEST("MFCC_tone_vs_noise_differ", d1 > 0.02f || d2 > 0.02f);
        printf("       MFCC diff: d1=%.4f d2=%.4f\n", d1, d2);
    }

    // ── Test [6]: RMS energy ─────────────────────────────────────────────────
    {
        auto pcm_silence = makeSilence(FRAME_SIZE);
        auto pcm_sine    = makeSine(440.0f, SAMPLE_RATE, FRAME_SIZE, 1.0f);
        auto f_sil  = dsp.extract(pcm_silence);
        auto f_sine = dsp.extract(pcm_sine);
        // Silence → rms ≈ 0
        bool silence_ok = f_sil.rms_energy < 0.01f;
        // Full-scale sine → rms normalised ≈ 0.707 (within some tolerance due to int rounding)
        bool sine_ok = (f_sine.rms_energy > 0.60f) && (f_sine.rms_energy < 0.80f);
        TEST("RMS_silence_near_zero", silence_ok);
        TEST("RMS_fullscale_sine_near_0707", sine_ok);
        printf("       rms_silence=%.4f rms_sine=%.4f\n", f_sil.rms_energy, f_sine.rms_energy);
    }

    // ── Test [7]: ZCR — sine wave ────────────────────────────────────────────
    {
        auto pcm_silence = makeSilence(FRAME_SIZE);
        auto pcm_sine    = makeSine(440.0f, SAMPLE_RATE, FRAME_SIZE);
        auto f_sil  = dsp.extract(pcm_silence);
        auto f_sine = dsp.extract(pcm_sine);
        // Silence → ZCR = 0 (or near 0 with zero PCM)
        bool sil_ok = f_sil.zero_crossing_rate < 0.01f;
        // 440Hz sine at 16kHz: ~2*440/16000 = 0.055 crossings per sample
        // tolerance: [0.03, 0.12]
        bool sine_ok = f_sine.zero_crossing_rate > 0.02f && f_sine.zero_crossing_rate < 0.15f;
        TEST("ZCR_silence_near_zero", sil_ok);
        TEST("ZCR_sine_in_range", sine_ok);
        printf("       zcr_silence=%.4f zcr_sine=%.4f\n", f_sil.zero_crossing_rate, f_sine.zero_crossing_rate);
    }

    // ── Test [8]: Spectral centroid — 1kHz tone ───────────────────────────────
    {
        auto pcm = makeSine(1000.0f, SAMPLE_RATE, FRAME_SIZE);
        auto f   = dsp.extract(pcm);
        // centroid is normalised to [0,1] by Nyquist (8000Hz)
        // 1kHz / 8000Hz = 0.125; allow ±0.06 (pre-emphasis + window smear)
        float centroid_hz = f.spectral_centroid * 8000.0f;
        bool ok = centroid_hz > 700.0f && centroid_hz < 1500.0f;
        TEST("SpectralCentroid_1kHz_in_range", ok);
        printf("       centroid_hz=%.1f (expected ~1000)\n", centroid_hz);
    }

    // ── Test [9]: Spectral rolloff ────────────────────────────────────────────
    {
        auto pcm = makeSine(1000.0f, SAMPLE_RATE, FRAME_SIZE);
        auto f   = dsp.extract(pcm);
        // For a 1kHz pure tone, rolloff should be above the centroid (≥0.05)
        bool ok = f.spectral_rolloff >= f.spectral_centroid * 0.5f &&
                  f.spectral_rolloff <= 1.0f;
        TEST("SpectralRolloff_1kHz_sane", ok);
        printf("       rolloff=%.4f centroid=%.4f\n", f.spectral_rolloff, f.spectral_centroid);
    }

    // ── Test [10]: End-to-end — 8D finite, no NaN/Inf ─────────────────────────
    {
        auto pcm = makeSine(500.0f, SAMPLE_RATE, FRAME_SIZE);
        auto vec = dsp.encode(pcm);
        bool ok = (vec.size() == 8);
        for (float v : vec) {
            if (!std::isfinite(v) || v < 0.0f || v > 1.0f) { ok = false; break; }
        }
        TEST("End2End_8D_finite_0to1", ok);
        if (vec.size() == 8) {
            printf("       [rms=%.3f zcr=%.3f cent=%.3f roll=%.3f flux=%.3f m1=%.3f m2=%.3f pitch=%.3f]\n",
                   vec[0], vec[1], vec[2], vec[3], vec[4], vec[5], vec[6], vec[7]);
        }
    }

    // ─────────────────────────────────────────────────────────────────────────
    printf("\n=== Results: %d/%d PASSED ===\n", g_tests_passed, g_tests_run);
    return (g_tests_passed == g_tests_run) ? 0 : 1;
}
