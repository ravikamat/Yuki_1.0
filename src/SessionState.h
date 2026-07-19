// SessionState.h
#pragma once
#include <vector>
#include <mutex>
#include <atomic>
#include <string>

struct ChatEntry {
    std::string speaker;
    std::string text;
    bool isVoice = false;
};

struct SessionState {
    std::vector<ChatEntry> history;
    std::mutex historyMutex;
    std::atomic<bool> quit{false};
};
