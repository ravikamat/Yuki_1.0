#pragma once
#include <string>
#include <deque>
#include <vector>
#include <mutex>
#include <utility>
#include <unordered_map>

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

    // --- NEW: WP1/WP2/WP3 Memory & Routing Support ---
    void setSystemNote(const std::string& key, const std::string& note);
    std::string getSystemNote(const std::string& key) const;

    void appendToWorkingMemory(const std::string& entry);
    void setUserFact(const std::string& key, const std::string& value);
    std::string getUserFact(const std::string& key) const;
    std::unordered_map<std::string, std::string> queryRelevantUserFacts(const std::vector<std::string>& keywords) const;

    void setAlias(const std::string& symbol, const std::string& meaning);
    std::string getAlias(const std::string& symbol) const;
    std::unordered_map<std::string, std::string> getAllAliases() const;

    void setFlag(const std::string& flagName, const std::string& value);
    std::string getFlag(const std::string& flagName) const;
    bool hasFlag(const std::string& flagName) const;

    void setCognitiveOrganOutput(const std::string& output);
    std::string getCognitiveOrganOutput() const;
    bool hasCognitiveOrganOutput() const;

private:
    yuki::memory::CognitiveMemoryFabric* cmf_;
    ContextWindow window_;
    mutable std::mutex mutex_;

    std::unordered_map<std::string, std::string> systemNotes_;
    std::unordered_map<std::string, std::string> userFacts_;
    std::unordered_map<std::string, std::string> userAliases_;
    std::unordered_map<std::string, std::string> flags_;
    std::string cognitiveOrganOutput_;

    uint64_t fnv1a(const std::string& s) const;
};

} // namespace yuki::memory

