#include "input/WakeDetector.h"
#include <fstream>
#include <chrono>

namespace yuki::input {

WakeDetector::WakeDetector(WakeCallback cb)
    : callback_(cb) {}

WakeDetector::~WakeDetector() {
    stop();
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

void WakeDetector::workerLoop() {
    while (!shutdown_requested_.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        // Energy threshold & pattern match mock
    }
}

} // namespace yuki::input
