#pragma once
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <mutex>
#include <tuple>
#include <utility>

namespace yuki {

// K3 refusal contract (Gate 4) — strict config read status.
// The lenient getFloat() falls back to a default for boot-time convenience;
// getFloatChecked() REFUSES found-but-unparseable and out-of-range values.
enum class ConfigReadStatus : int {
    OK           = 0,
    NOT_FOUND    = 1,  // section/key absent — caller decides policy
    UNPARSEABLE  = 2,  // found but not parseable as float — refuse
    OUT_OF_RANGE = 3   // found, parseable, outside [min_valid, max_valid] — refuse
};

class ConfigManager {
public:
    static ConfigManager& instance();

    bool loadFromFile(const std::string& path);
    bool loadKeywords(const std::string& path, std::unordered_set<std::string>& out);
    bool loadTemplates(const std::string& path, std::unordered_map<std::string, std::string>& out);
    bool loadPatterns(const std::string& path, std::vector<std::pair<std::string, float>>& out);
    bool loadFloatConfig(const std::string& path, std::unordered_map<std::string, float>& out);
    bool loadToolPaths(const std::string& path, std::unordered_map<std::string, std::vector<std::string>>& out);
    bool loadSecurityPaths(const std::string& path, 
                           std::vector<std::string>& denied_prefixes_win,
                           std::vector<std::string>& denied_prefixes_unix);
    bool loadBootstrapKnowledge(const std::string& path, 
                                std::vector<std::tuple<std::string, std::string, float>>& out);
    bool loadVseFeatures(const std::string& path,
                         std::unordered_map<std::string, std::vector<float>>& out);

    // Load main yuki config (INI format with sections)
    bool loadYukiConfig(const std::string& path);

    float loadFloatConfig(const std::string& key, float defaultVal) const;
    float getFloat(const std::string& section, const std::string& key, float defaultVal = 0.0f) const;
    float getFloat(const std::string& key, float defaultVal = 0.0f) const;

    // Strict read with explicit refusal status (K3/Gate 4). Returns OK and
    // fills out_value only when the key exists, parses as float, and lies
    // within [min_valid, max_valid]. Never substitutes a default.
    ConfigReadStatus getFloatChecked(const std::string& section, const std::string& key,
                                     float min_valid, float max_valid, float& out_value) const;
    int   getInt(const std::string& section, const std::string& key, int defaultVal = 0) const;
    int   getInt(const std::string& key, int defaultVal = 0) const;
    std::string getString(const std::string& section, const std::string& key, const std::string& defaultVal = "") const;
    std::string getString(const std::string& key, const std::string& defaultVal = "") const;
    std::string loadPromptTemplate(const std::string& key) const;
    std::string getTemplate(const std::string& key) const;
    bool isKeyword(const std::string& word) const;
    float getPatternScore(const std::string& prefix) const;
    std::vector<std::string> getToolPathHints(const std::string& tool) const;

    // Config file path
    void setConfigPath(const std::string& path) { configPath_ = path; }
    const std::string& getConfigPath() const { return configPath_; }


private:
    ConfigManager() = default;
    mutable std::mutex mtx_;
    std::unordered_map<std::string, std::string> templates_;
    std::unordered_set<std::string> keywords_;
    std::unordered_map<std::string, float> pattern_scores_;
    std::unordered_map<std::string, std::vector<std::string>> tool_paths_;
    std::string configPath_;
};

} // namespace yuki
