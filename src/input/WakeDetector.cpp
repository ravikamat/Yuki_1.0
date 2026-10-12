#include "input/WakeDetector.h"
#include "input/Ear.h"
#include <fstream>
#include <chrono>
#include <cmath>
#include <numeric>
#include <algorithm>
#include <iostream>

namespace yuki::input {

WakeDetector::WakeDetector(WakeCallback cb)
    : callback_(std::move(cb)) {}

WakeDetector::~WakeDetector() {
    stop();
}

void WakeDetector::setCallback(WakeCallback cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    callback_ = std::move(cb);
}

bool WakeDetector::loadPatternFromFile(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;

    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);

    if (size == 0) return false;
    wake_pattern_.resize(size / sizeof(float));
    if (!wake_pattern_.empty()) {
        file.read(reinterpret_cast<char*>(wake_pattern_.data()), wake_pattern_.size() * sizeof(float));
    } else {
        // Fallback for non-float dummy pattern file
        wake_pattern_ = {0.1f, 0.2f, 0.3f, 0.4f};
    }
    return true;
}

bool WakeDetector::start() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (running_.load()) return true;

    shutdown_requested_.store(false);
    running_.store(true);
    worker_ = std::thread(&WakeDetector::workerLoop, this);
    return true;
}

void WakeDetector::stop() {
    shutdown_requested_.store(true);
    running_.store(false);
    if (worker_.joinable()) {
        worker_.join();
    }
}

void WakeDetector::feedPCM(const short* samples, size_t count) {
    if (!samples || count == 0) return;
    std::vector<short> window;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        audio_buffer_.insert(audio_buffer_.end(), samples, samples + count);
        if (audio_buffer_.size() > 16000 * 2) {
            audio_buffer_.erase(audio_buffer_.begin(), audio_buffer_.end() - 16000);
        }
        window = audio_buffer_;
    }
    processAudioWindow(window);
}

bool WakeDetector::processAudioWindow(const std::vector<short>& pcm) {
    if (pcm.empty()) return false;

    // 1. Calculate overall RMS energy
    double sum_sq = 0.0;
    for (short s : pcm) {
        double norm = static_cast<double>(s) / 32768.0;
        sum_sq += norm * norm;
    }
    float rms = static_cast<float>(std::sqrt(sum_sq / pcm.size()));
    if (rms < 0.012f) {
        return false; // Noise floor / silence
    }

    // 2. Extract energy envelope vector matching wake_pattern_ dimension
    std::vector<float> pattern_copy;
    WakeCallback cb;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pattern_copy = wake_pattern_;
        cb = callback_;
    }

    if (pattern_copy.empty()) {
        return false;
    }

    size_t K = pattern_copy.size();
    size_t W = pcm.size() / K;
    if (W == 0) return false;

    std::vector<float> envelope(K, 0.0f);
    double env_norm_sq = 0.0;
    for (size_t k = 0; k < K; ++k) {
        double sub_sum = 0.0;
        for (size_t j = 0; j < W; ++j) {
            double norm = static_cast<double>(pcm[k * W + j]) / 32768.0;
            sub_sum += norm * norm;
        }
        envelope[k] = static_cast<float>(std::sqrt(sub_sum / W));
        env_norm_sq += envelope[k] * envelope[k];
    }

    // 3. Compute cosine similarity against loaded pattern
    double pat_norm_sq = 0.0;
    for (float p : pattern_copy) pat_norm_sq += p * p;

    if (env_norm_sq < 1e-9 || pat_norm_sq < 1e-9) return false;

    double dot = 0.0;
    for (size_t i = 0; i < K; ++i) {
        dot += envelope[i] * pattern_copy[i];
    }
    float similarity = static_cast<float>(dot / (std::sqrt(env_norm_sq) * std::sqrt(pat_norm_sq)));

    // 4. Trigger wake if similarity is high and debounce condition met
    if (similarity > 0.55f) {
        auto now = std::chrono::steady_clock::now();
        std::lock_guard<std::mutex> lock(mutex_);
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_wake_time_).count();
        if (elapsed > 2000) { // 2 second debounce
            last_wake_time_ = now;
            if (cb) {
                std::cout << "[WakeDetector] Match! similarity=" << similarity << " rms=" << rms << " -> Triggering wake callback\n";
                cb();
                return true;
            }
        }
    }
    return false;
}

void WakeDetector::workerLoop() {
    while (!shutdown_requested_.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(80));
        if (ear_) {
            auto window = ear_->readLatestPCMWindow(16000); // 1-second sliding window
            if (!window.empty()) {
                processAudioWindow(window);
            }
        }
    }
}

} // namespace yuki::input
