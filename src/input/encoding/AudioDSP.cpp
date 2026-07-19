// AudioDSP.cpp
// Yuki_1.0 — Production Audio DSP Pipeline Implementation
//
// Pipeline (per 512-sample frame at 16 kHz):
//   1. Pre-emphasis filter:       y[n] = x[n] - 0.97 * x[n-1]
//   2. Hanning window:            w[n] = 0.5*(1 - cos(2*pi*n/(N-1)))
//   3. FFT (Cooley-Tukey radix-2, iterative, in-place)
//   4. Magnitude spectrum:        |X[k]|  for k = 0..N/2
//   5. Mel filterbank (26 triangular filters, 20-8000 Hz, mel-spaced)
//   6. Log mel energy:            log(energy + 1e-8)
//   7. DCT-II → 13 MFCCs
//   8. YIN pitch detection (difference func + CMNDF + parabolic interp)
//   9. Spectral centroid, spectral rolloff (85%), spectral flux
//  10. RMS energy, zero-crossing rate
//
// All values are normalised to [0, 1] for the 8D output vector.

#include "AudioDSP.h"
#include <cmath>
#include <cassert>
#include <stdexcept>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace yuki::dsp {

// ─────────────────────────────────────────────────────────────────────────────
// Construction
// ─────────────────────────────────────────────────────────────────────────────

AudioDSPEngine::AudioDSPEngine(int sample_rate, int frame_size)
    : sample_rate_(sample_rate), frame_size_(frame_size)
{
    // frame_size must be a power of 2
    assert((frame_size & (frame_size - 1)) == 0 && "frame_size must be power of 2");
    prev_magnitude_.assign(frame_size / 2 + 1, 0.0f);
    buildHanningWindow();
    buildMelFilterbank();
    buildDCTMatrix();
}

// ─────────────────────────────────────────────────────────────────────────────
// Public interface
// ─────────────────────────────────────────────────────────────────────────────

std::vector<float> AudioDSPEngine::encode(const std::vector<int16_t>& pcm_frame) {
    auto f = extract(pcm_frame);
    return { f.rms_energy, f.zero_crossing_rate, f.spectral_centroid,
             f.spectral_rolloff, f.spectral_flux, f.mfcc_1, f.mfcc_2, f.pitch_yin };
}

AudioFeatures AudioDSPEngine::extract(const std::vector<int16_t>& pcm_frame) {
    AudioFeatures out{};

    // Pad or truncate to exactly frame_size_
    std::vector<int16_t> pcm(frame_size_, 0);
    size_t copy_len = std::min(static_cast<size_t>(frame_size_), pcm_frame.size());
    std::copy(pcm_frame.begin(), pcm_frame.begin() + static_cast<ptrdiff_t>(copy_len), pcm.begin());

    // ── Time-domain features ─────────────────────────────────────────────────

    // RMS energy (normalised: full-scale int16 sine has RMS = 32767/sqrt(2) ≈ 23170)
    double sum_sq = 0.0;
    for (auto s : pcm) sum_sq += static_cast<double>(s) * static_cast<double>(s);
    float rms_raw = static_cast<float>(std::sqrt(sum_sq / frame_size_));
    out.rms_energy = clamp01(rms_raw / 32768.0f);

    // Zero-crossing rate
    int zcr_count = 0;
    for (int i = 1; i < frame_size_; ++i) {
        if ((pcm[i] >= 0) != (pcm[i - 1] >= 0)) ++zcr_count;
    }
    // Theoretical max ZCR for an alternating signal = (N-1) sign changes
    out.zero_crossing_rate = clamp01(static_cast<float>(zcr_count) / static_cast<float>(frame_size_ - 1));

    // ── Frequency-domain features ─────────────────────────────────────────────

    std::vector<float> mag = computeMagnitudeSpectrum(pcm);
    int num_bins = static_cast<int>(mag.size()); // = frame_size_/2 + 1

    // Total energy in spectrum
    float total_energy = 0.0f;
    for (float v : mag) total_energy += v;
    if (total_energy < 1e-10f) total_energy = 1e-10f;

    // Spectral centroid (Hz) — normalised to [0,1] by Nyquist
    float nyquist = static_cast<float>(sample_rate_) / 2.0f;
    float cent_num = 0.0f;
    for (int k = 0; k < num_bins; ++k) {
        float freq = static_cast<float>(k) * nyquist / static_cast<float>(num_bins - 1);
        cent_num += freq * mag[k];
    }
    float centroid_hz = cent_num / total_energy;
    out.spectral_centroid = clamp01(centroid_hz / nyquist);

    // Spectral rolloff (85% cumulative energy) — normalised to [0,1]
    float target_energy = 0.85f * total_energy;
    float cumulative = 0.0f;
    float rolloff_hz = nyquist;
    for (int k = 0; k < num_bins; ++k) {
        cumulative += mag[k];
        if (cumulative >= target_energy) {
            rolloff_hz = static_cast<float>(k) * nyquist / static_cast<float>(num_bins - 1);
            break;
        }
    }
    out.spectral_rolloff = clamp01(rolloff_hz / nyquist);

    // Spectral flux — L1 distance of magnitude from previous frame, normalised
    float flux = 0.0f;
    float max_mag = 0.0f;
    for (int k = 0; k < num_bins; ++k) {
        float diff = mag[k] - prev_magnitude_[k];
        if (diff > 0.0f) flux += diff;  // half-wave rectification (onset detection)
        if (mag[k] > max_mag) max_mag = mag[k];
    }
    prev_magnitude_ = mag;
    // Normalise by number of bins and peak magnitude
    float flux_norm = (max_mag > 1e-10f) ? flux / (static_cast<float>(num_bins) * max_mag) : 0.0f;
    out.spectral_flux = clamp01(flux_norm);

    // ── MFCC ─────────────────────────────────────────────────────────────────
    std::vector<float> mel_log = melLogEnergy(mag);
    std::vector<float> mfccs = computeMFCCs(mel_log);

    // MFCC[0] and MFCC[1] — typical range roughly [-20, +20]; scale to [0,1]
    // We use a soft sigmoid-style normalisation: 0.5 + tanh(c/10) * 0.5
    auto mfccNorm = [](float c) -> float {
        float t = std::tanh(c / 10.0f);
        return clamp01(0.5f + t * 0.5f);
    };
    out.mfcc_1 = mfccs.size() > 0 ? mfccNorm(mfccs[0]) : 0.5f;
    out.mfcc_2 = mfccs.size() > 1 ? mfccNorm(mfccs[1]) : 0.5f;

    // ── YIN Pitch ─────────────────────────────────────────────────────────────
    float f0 = computeYIN(pcm);
    // Normalise: unvoiced → 0, PITCH_MAX_HZ → 1 (log scale for perceptual linearity)
    if (f0 < PITCH_MIN_HZ) {
        out.pitch_yin = 0.0f;
    } else {
        // Map [PITCH_MIN_HZ, PITCH_MAX_HZ] → [0, 1] on log scale
        float log_min = std::log(PITCH_MIN_HZ);
        float log_max = std::log(PITCH_MAX_HZ);
        float log_f0  = std::log(std::min(f0, PITCH_MAX_HZ));
        out.pitch_yin = clamp01((log_f0 - log_min) / (log_max - log_min));
    }

    return out;
}

std::vector<float> AudioDSPEngine::magnitudeSpectrum(const std::vector<int16_t>& pcm_frame) {
    std::vector<int16_t> pcm(frame_size_, 0);
    size_t copy_len = std::min(static_cast<size_t>(frame_size_), pcm_frame.size());
    std::copy(pcm_frame.begin(), pcm_frame.begin() + static_cast<ptrdiff_t>(copy_len), pcm.begin());
    return computeMagnitudeSpectrum(pcm);
}

// ─────────────────────────────────────────────────────────────────────────────
// Init helpers
// ─────────────────────────────────────────────────────────────────────────────

void AudioDSPEngine::buildHanningWindow() {
    window_.resize(frame_size_);
    for (int n = 0; n < frame_size_; ++n) {
        window_[n] = 0.5f * (1.0f - std::cos(
            2.0f * static_cast<float>(M_PI) * static_cast<float>(n) /
            static_cast<float>(frame_size_ - 1)));
    }
}

void AudioDSPEngine::buildMelFilterbank() {
    // Triangular Mel filters: NUM_MEL filters linearly spaced in mel scale
    // from MEL_LOW_HZ to MEL_HIGH_HZ. Each filter spans 3 consecutive mel
    // points and has a triangular response.
    int num_bins = frame_size_ / 2 + 1;

    float mel_low  = hzToMel(MEL_LOW_HZ);
    float mel_high = hzToMel(std::min(MEL_HIGH_HZ, static_cast<float>(sample_rate_) / 2.0f));

    // NUM_MEL + 2 mel points (including lower and upper edge)
    std::vector<float> mel_points(NUM_MEL + 2);
    for (int m = 0; m < NUM_MEL + 2; ++m) {
        mel_points[m] = mel_low + static_cast<float>(m) * (mel_high - mel_low) /
                        static_cast<float>(NUM_MEL + 1);
    }

    // Convert mel points to FFT bin indices
    std::vector<int> bin_points(NUM_MEL + 2);
    for (int m = 0; m < NUM_MEL + 2; ++m) {
        float hz = melToHz(mel_points[m]);
        bin_points[m] = static_cast<int>(
            std::round(hz * static_cast<float>(frame_size_) / static_cast<float>(sample_rate_)));
        bin_points[m] = std::max(0, std::min(num_bins - 1, bin_points[m]));
    }

    mel_filterbank_.assign(NUM_MEL, std::vector<float>(num_bins, 0.0f));
    for (int m = 0; m < NUM_MEL; ++m) {
        int lo  = bin_points[m];
        int mid = bin_points[m + 1];
        int hi  = bin_points[m + 2];
        // Rising slope
        for (int k = lo; k <= mid; ++k) {
            if (mid > lo)
                mel_filterbank_[m][k] = static_cast<float>(k - lo) / static_cast<float>(mid - lo);
        }
        // Falling slope
        for (int k = mid; k <= hi; ++k) {
            if (hi > mid)
                mel_filterbank_[m][k] = static_cast<float>(hi - k) / static_cast<float>(hi - mid);
        }
    }
}

void AudioDSPEngine::buildDCTMatrix() {
    // DCT-II: C[c][m] = cos(pi*(m+0.5)*c / NUM_MEL), normalised
    dct_matrix_.assign(NUM_MFCC, std::vector<float>(NUM_MEL, 0.0f));
    for (int c = 0; c < NUM_MFCC; ++c) {
        for (int m = 0; m < NUM_MEL; ++m) {
            dct_matrix_[c][m] = std::cos(
                static_cast<float>(M_PI) * static_cast<float>(c) *
                (static_cast<float>(m) + 0.5f) / static_cast<float>(NUM_MEL));
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// DSP Primitives
// ─────────────────────────────────────────────────────────────────────────────

void AudioDSPEngine::bitReversalPermutation(std::vector<std::complex<float>>& buf) {
    int n = static_cast<int>(buf.size());
    int j = 0;
    for (int i = 1; i < n; ++i) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(buf[i], buf[j]);
    }
}

void AudioDSPEngine::fft(std::vector<std::complex<float>>& buf) const {
    // Iterative Cooley-Tukey radix-2 DIT FFT
    int n = static_cast<int>(buf.size());
    // Bit-reversal permutation (non-const call via local copy would be needed;
    // but since we own the buffer here, cast away is safe: the caller owns it)
    // We call the static helper on the mutable buffer
    AudioDSPEngine::bitReversalPermutation(buf);

    for (int len = 2; len <= n; len <<= 1) {
        float ang = -2.0f * static_cast<float>(M_PI) / static_cast<float>(len);
        std::complex<float> wlen(std::cos(ang), std::sin(ang));
        for (int i = 0; i < n; i += len) {
            std::complex<float> w(1.0f, 0.0f);
            for (int j = 0; j < len / 2; ++j) {
                std::complex<float> u = buf[i + j];
                std::complex<float> v = buf[i + j + len / 2] * w;
                buf[i + j]           = u + v;
                buf[i + j + len / 2] = u - v;
                w *= wlen;
            }
        }
    }
}

std::vector<float> AudioDSPEngine::computeMagnitudeSpectrum(const std::vector<int16_t>& pcm) {
    // 1. Pre-emphasis filter: y[n] = x[n] - 0.97 * x[n-1]
    std::vector<float> frame(frame_size_);
    frame[0] = static_cast<float>(pcm[0]);
    for (int i = 1; i < frame_size_; ++i) {
        frame[i] = static_cast<float>(pcm[i]) - 0.97f * static_cast<float>(pcm[i - 1]);
    }

    // 2. Hanning window
    for (int i = 0; i < frame_size_; ++i) {
        frame[i] *= window_[i];
    }

    // 3. Pack into complex buffer
    std::vector<std::complex<float>> fft_buf(frame_size_);
    for (int i = 0; i < frame_size_; ++i) {
        fft_buf[i] = std::complex<float>(frame[i], 0.0f);
    }

    // 4. FFT
    fft(fft_buf);

    // 5. Magnitude spectrum (one-sided: bins 0..N/2)
    int num_bins = frame_size_ / 2 + 1;
    std::vector<float> mag(num_bins);
    for (int k = 0; k < num_bins; ++k) {
        mag[k] = std::abs(fft_buf[k]);
    }
    return mag;
}

std::vector<float> AudioDSPEngine::melLogEnergy(const std::vector<float>& magnitude) const {
    std::vector<float> mel_log(NUM_MEL);
    int num_bins = static_cast<int>(magnitude.size());
    for (int m = 0; m < NUM_MEL; ++m) {
        float energy = 0.0f;
        for (int k = 0; k < num_bins; ++k) {
            energy += mel_filterbank_[m][k] * magnitude[k];
        }
        mel_log[m] = std::log(energy + 1e-8f);
    }
    return mel_log;
}

std::vector<float> AudioDSPEngine::computeMFCCs(const std::vector<float>& mel_log) const {
    std::vector<float> mfccs(NUM_MFCC, 0.0f);
    for (int c = 0; c < NUM_MFCC; ++c) {
        float val = 0.0f;
        for (int m = 0; m < NUM_MEL; ++m) {
            val += dct_matrix_[c][m] * mel_log[m];
        }
        // Standard DCT-II normalisation factor
        float norm = (c == 0) ? std::sqrt(1.0f / static_cast<float>(NUM_MEL))
                               : std::sqrt(2.0f / static_cast<float>(NUM_MEL));
        mfccs[c] = val * norm;
    }
    return mfccs;
}

float AudioDSPEngine::computeYIN(const std::vector<int16_t>& pcm) const {
    // YIN algorithm (de Cheveigné & Kawahara 2002)
    // Step 1: difference function
    //   d[tau] = sum_{i=0}^{N-1-tau} (x[i] - x[i+tau])^2
    // Equivalent to: x[i]^2 + x[i+tau]^2 - 2*x[i]*x[i+tau]
    int N = frame_size_;
    int max_tau = N / 2;

    // Convert to float for precision
    std::vector<float> x(N);
    for (int i = 0; i < N; ++i) x[i] = static_cast<float>(pcm[i]);

    std::vector<float> d(max_tau + 1, 0.0f);
    // d[0] = 0 by definition
    for (int tau = 1; tau <= max_tau; ++tau) {
        float s = 0.0f;
        for (int i = 0; i < N - tau; ++i) {
            float diff = x[i] - x[i + tau];
            s += diff * diff;
        }
        d[tau] = s;
    }

    // Step 2: Cumulative mean normalised difference function (CMNDF)
    std::vector<float> cmndf(max_tau + 1, 0.0f);
    cmndf[0] = 1.0f;
    float running_sum = 0.0f;
    for (int tau = 1; tau <= max_tau; ++tau) {
        running_sum += d[tau];
        if (running_sum > 0.0f)
            cmndf[tau] = d[tau] * static_cast<float>(tau) / running_sum;
        else
            cmndf[tau] = 1.0f;
    }

    // Step 3: Absolute threshold — find first tau < YIN_THRESHOLD
    int min_tau = static_cast<int>(std::ceil(static_cast<float>(sample_rate_) / PITCH_MAX_HZ));
    int max_tau_pitch = static_cast<int>(std::floor(static_cast<float>(sample_rate_) / PITCH_MIN_HZ));
    max_tau_pitch = std::min(max_tau_pitch, max_tau);

    int tau_est = -1;
    float min_val = 1.0f;

    for (int tau = min_tau; tau <= max_tau_pitch; ++tau) {
        if (cmndf[tau] < YIN_THRESHOLD) {
            // Find local minimum from this tau
            int local_min = tau;
            while (local_min + 1 <= max_tau_pitch && cmndf[local_min + 1] < cmndf[local_min]) {
                ++local_min;
            }
            tau_est = local_min;
            break;
        }
        if (cmndf[tau] < min_val) {
            min_val = cmndf[tau];
            tau_est = tau;
        }
    }

    if (tau_est <= 0) return 0.0f;  // unvoiced
    if (min_val >= YIN_THRESHOLD * 3.0f) return 0.0f;  // confidence too low

    // Step 4: Parabolic interpolation for sub-sample accuracy
    float interpolated_tau = parabolicInterpolation(cmndf, tau_est);
    if (interpolated_tau <= 0.0f) return 0.0f;

    float f0 = static_cast<float>(sample_rate_) / interpolated_tau;
    if (f0 < PITCH_MIN_HZ || f0 > PITCH_MAX_HZ) return 0.0f;
    return f0;
}

float AudioDSPEngine::parabolicInterpolation(const std::vector<float>& buf, int tau) const {
    int n = static_cast<int>(buf.size());
    if (tau <= 0 || tau >= n - 1) return static_cast<float>(tau);

    float s0 = buf[tau - 1];
    float s1 = buf[tau];
    float s2 = buf[tau + 1];

    float denom = s0 - 2.0f * s1 + s2;
    if (std::abs(denom) < 1e-10f) return static_cast<float>(tau);

    float delta = 0.5f * (s0 - s2) / denom;
    // Clamp delta to [-1, 1] for stability
    delta = std::max(-1.0f, std::min(1.0f, delta));
    return static_cast<float>(tau) + delta;
}

} // namespace yuki::dsp
