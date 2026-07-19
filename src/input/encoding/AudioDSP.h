#pragma once
// AudioDSP.h
// Yuki_1.0 — Production Audio DSP Pipeline
//
// Self-contained DSP engine: FFT (Cooley-Tukey radix-2) + Mel filterbank +
// MFCC (DCT-II) + YIN pitch detection + spectral/time-domain features.
// Requires only: <complex>, <cmath>, <vector>, <cstdint>, <algorithm>
// NO external DSP/audio libraries.
//
// Frame contract: 512 samples @ 16kHz (32ms window, power-of-2 required for FFT)

#include <vector>
#include <complex>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <limits>

namespace yuki::dsp {

// ── AudioFeatures: 8-dimensional physically-meaningful output ────────────────
struct AudioFeatures {
    float rms_energy;         // [0] Root-mean-square energy, normalised to [0,1]
    float zero_crossing_rate; // [1] ZCR — voiced/unvoiced discrimination [0,1]
    float spectral_centroid;  // [2] Centre-of-mass of spectrum, Hz, norm [0,1]
    float spectral_rolloff;   // [3] Freq below which 85% energy lies, norm [0,1]
    float spectral_flux;      // [4] Frame-to-frame spectral change [0,1]
    float mfcc_1;             // [5] 1st MFCC coefficient, norm [0,1]
    float mfcc_2;             // [6] 2nd MFCC coefficient, norm [0,1]
    float pitch_yin;          // [7] F0 via YIN, norm [0,1] (0=unvoiced, 1=max)
};

// ── AudioDSPEngine ───────────────────────────────────────────────────────────
class AudioDSPEngine {
public:
    // sample_rate: e.g. 16000 Hz.  frame_size MUST be a power of 2 (default 512)
    explicit AudioDSPEngine(int sample_rate = 16000, int frame_size = 512);

    // Main entry: returns 8D feature vector as float[8]
    AudioFeatures extract(const std::vector<int16_t>& pcm_frame);

    // Encode returns normalised std::vector<float> ready for ObservationEncoder
    std::vector<float> encode(const std::vector<int16_t>& pcm_frame);

    // Raw magnitude spectrum (frame_size/2 + 1 bins) — exposed for tests
    std::vector<float> magnitudeSpectrum(const std::vector<int16_t>& pcm_frame);

    // Sample rate accessor (for tests)
    int sampleRate() const { return sample_rate_; }
    int frameSize()  const { return frame_size_;  }

private:
    int sample_rate_;
    int frame_size_;

    // Hanning window coefficients
    std::vector<float> window_;

    // Previous magnitude for spectral flux
    std::vector<float> prev_magnitude_;

    // Mel filterbank: mel_filterbank_[m][k] = weight of bin k in filter m
    static constexpr int NUM_MEL = 26;
    static constexpr float MEL_LOW_HZ  =   20.0f;
    static constexpr float MEL_HIGH_HZ = 8000.0f;
    std::vector<std::vector<float>> mel_filterbank_;

    // DCT-II matrix for MFCC: dct_[c][m] for cepstral coeff c, mel band m
    static constexpr int NUM_MFCC = 13;
    std::vector<std::vector<float>> dct_matrix_;

    // YIN parameters
    static constexpr float YIN_THRESHOLD = 0.1f;
    static constexpr float PITCH_MIN_HZ  =  50.0f;  // below this = unvoiced
    static constexpr float PITCH_MAX_HZ  = 600.0f;  // above this = unvoiced

    // ── Init helpers ─────────────────────────────────────────────────────────
    void buildHanningWindow();
    void buildMelFilterbank();
    void buildDCTMatrix();

    // ── DSP primitives ───────────────────────────────────────────────────────
    // Iterative bit-reversal Cooley-Tukey FFT (in-place, size must be pow2)
    void fft(std::vector<std::complex<float>>& buf) const;

    // Pre-emphasis + window + FFT → magnitude spectrum (N/2+1 bins)
    std::vector<float> computeMagnitudeSpectrum(const std::vector<int16_t>& pcm);

    // Apply Mel filterbank → log energies per band
    std::vector<float> melLogEnergy(const std::vector<float>& magnitude) const;

    // DCT-II of mel log-energy → MFCCs (returns NUM_MFCC coefficients)
    std::vector<float> computeMFCCs(const std::vector<float>& mel_log) const;

    // YIN pitch detection — returns F0 in Hz (0 = unvoiced)
    float computeYIN(const std::vector<int16_t>& pcm) const;

    // Parabolic interpolation around peak index tau in buf
    float parabolicInterpolation(const std::vector<float>& buf, int tau) const;

    // Mel scale conversions
    static float hzToMel(float hz) { return 2595.0f * std::log10(1.0f + hz / 700.0f); }
    static float melToHz(float mel) { return 700.0f * (std::pow(10.0f, mel / 2595.0f) - 1.0f); }

    // Safe clamp to [0,1]
    static float clamp01(float v) {
        return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
    }

    // Bit-reversal permutation for FFT
    static void bitReversalPermutation(std::vector<std::complex<float>>& buf);
};

} // namespace yuki::dsp
