#pragma once
#include <string>
#include <mutex>

namespace yuki::input {

class VoiceEngine {
public:
    VoiceEngine();
    ~VoiceEngine();

    bool initialize();
    bool speak(const std::string& text);
    bool setRate(int rate);     // [-10, 10]
    bool setVolume(int volume); // [0, 100]
    void shutdown();

private:
    bool initialized_{false};
    int rate_{0};
    int volume_{100};
    mutable std::mutex mutex_;
};

} // namespace yuki::input
