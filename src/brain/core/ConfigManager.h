#pragma once
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <mutex>
#include <tuple>
#include <utility>

namespace yuki {

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

    std::string getTemplate(const std::string& key) const;
    bool isKeyword(const std::string& word) const;
    float getPatternScore(const std::string& prefix) const;
    std::vector<std::string> getToolPathHints(const std::string& tool) const;

private:
    ConfigManager() = default;
    mutable std::mutex mtx_;
    std::unordered_map<std::string, std::string> templates_;
    std::unordered_set<std::string> keywords_;
    std::unordered_map<std::string, float> pattern_scores_;
    std::unordered_map<std::string, std::vector<std::string>> tool_paths_;
};

} // namespace yuki
