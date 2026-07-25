#pragma once
#include <string>
#include <deque>
#include <vector>
#include <mutex>
#include <utility>

namespace yuki::memory {
class CognitiveMemoryFabric;

struct ContextWindow {
    std::deque<std::pair<std::string, std::string>> local_messages; // {role, content}
    std::deque<std::string> global_summaries;
    static constexpr size_t kLocalMax = 20;
    static constexpr size_t kSummaryCompressEvery = 5;
};

class ContextManager {
public:
    explicit ContextManager(yuki::memory::CognitiveMemoryFabric* cmf = nullptr);

    void appendTurn(const std::string& role, const std::string& text);
    ContextWindow getContextWindow() const;
    void compressOldest();
    void clear();

private:
    yuki::memory::CognitiveMemoryFabric* cmf_;
    ContextWindow window_;
    mutable std::mutex mutex_;

    uint64_t fnv1a(const std::string& s) const;
};

} // namespace yuki::memory
