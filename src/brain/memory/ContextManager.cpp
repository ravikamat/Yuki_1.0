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
    systemNotes_.clear();
    userFacts_.clear();
    userAliases_.clear();
    flags_.clear();
    cognitiveOrganOutput_.clear();
}

void ContextManager::setSystemNote(const std::string& key, const std::string& note) {
    std::lock_guard<std::mutex> lock(mutex_);
    systemNotes_[key] = note;
}

std::string ContextManager::getSystemNote(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = systemNotes_.find(key);
    return (it != systemNotes_.end()) ? it->second : "";
}

void ContextManager::appendToWorkingMemory(const std::string& entry) {
    appendTurn("system", entry);
}

void ContextManager::setUserFact(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    userFacts_[key] = value;
}

std::string ContextManager::getUserFact(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = userFacts_.find(key);
    return (it != userFacts_.end()) ? it->second : "";
}

std::unordered_map<std::string, std::string> ContextManager::queryRelevantUserFacts(
    const std::vector<std::string>& keywords) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::unordered_map<std::string, std::string> results;
    for (const auto& [k, v] : userFacts_) {
        if (keywords.empty()) {
            results[k] = v;
            continue;
        }
        for (const auto& kw : keywords) {
            if (k.find(kw) != std::string::npos || v.find(kw) != std::string::npos) {
                results[k] = v;
                break;
            }
        }
    }
    return results;
}

void ContextManager::setAlias(const std::string& symbol, const std::string& meaning) {
    std::lock_guard<std::mutex> lock(mutex_);
    userAliases_[symbol] = meaning;
}

std::string ContextManager::getAlias(const std::string& symbol) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = userAliases_.find(symbol);
    return (it != userAliases_.end()) ? it->second : "";
}

std::unordered_map<std::string, std::string> ContextManager::getAllAliases() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return userAliases_;
}

void ContextManager::setFlag(const std::string& flagName, const std::string& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    flags_[flagName] = value;
}

std::string ContextManager::getFlag(const std::string& flagName) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = flags_.find(flagName);
    return (it != flags_.end()) ? it->second : "";
}

bool ContextManager::hasFlag(const std::string& flagName) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return flags_.find(flagName) != flags_.end();
}

void ContextManager::setCognitiveOrganOutput(const std::string& output) {
    std::lock_guard<std::mutex> lock(mutex_);
    cognitiveOrganOutput_ = output;
}

std::string ContextManager::getCognitiveOrganOutput() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return cognitiveOrganOutput_;
}

bool ContextManager::hasCognitiveOrganOutput() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return !cognitiveOrganOutput_.empty();
}

} // namespace yuki::memory

