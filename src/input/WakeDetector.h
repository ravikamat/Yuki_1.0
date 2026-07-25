#pragma once
#include <string>
#include <vector>
#include <functional>
#include <atomic>
#include <thread>
#include <mutex>

namespace yuki::input {

class WakeDetector {
public:
    using WakeCallback = std::function<void()>;

    explicit WakeDetector(WakeCallback cb = nullptr);
    ~WakeDetector(); // out-of-line in .cpp

    bool start();
    void stop();
    bool isRunning() const { return running_.load(); }
    bool loadPatternFromFile(const std::string& path);
    const std::vector<float>& pattern() const { return wake_pattern_; }

private:
    WakeCallback callback_;
    std::atomic<bool> running_{false};
    std::atomic<bool> shutdown_requested_{false};
    std::thread worker_;
    std::vector<float> wake_pattern_;
    mutable std::mutex mutex_;

    void workerLoop();
};

} // namespace yuki::input
