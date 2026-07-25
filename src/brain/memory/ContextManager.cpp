#include "brain/memory/ContextManager.h"
#include "brain/memory/CognitiveMemoryFabric.h"
#include <sstream>

namespace yuki::memory {

ContextManager::ContextManager(yuki::memory::CognitiveMemoryFabric* cmf)
    : cmf_(cmf) {}

uint64_t ContextManager::fnv1a(const std::string& s) const {
    uint64_t h = 0xcbf29ce484222325ULL;
    for (unsigned char c : s) {
        h ^= c;
        h *= 0x100000001b3ULL;
    }
    return h;
}

void ContextManager::appendTurn(const std::string& role, const std::string& text) {
    std::lock_guard<std::mutex> lock(mutex_);
    window_.local_messages.push_back({role, text});

    if (window_.local_messages.size() > ContextWindow::kLocalMax) {
        compressOldest();
    }
}

void ContextManager::compressOldest() {
    if (window_.local_messages.size() < ContextWindow::kSummaryCompressEvery) return;

    std::ostringstream summary;
    summary << "Summary [hash=";
    std::string text_concat;
    for (size_t i = 0; i < ContextWindow::kSummaryCompressEvery; ++i) {
        text_concat += window_.local_messages.front().second;
        window_.local_messages.pop_front();
    }
    summary << fnv1a(text_concat) << ", turns=" << ContextWindow::kSummaryCompressEvery << "]";

    window_.global_summaries.push_back(summary.str());
}

ContextWindow ContextManager::getContextWindow() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return window_;
}

void ContextManager::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    window_.local_messages.clear();
    window_.global_summaries.clear();
}

} // namespace yuki::memory
