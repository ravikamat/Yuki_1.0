#include "ConfigManager.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace yuki {

static std::string toLower(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return result;
}

static std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

ConfigManager& ConfigManager::instance() {
    static ConfigManager instance;
    return instance;
}

bool ConfigManager::loadFromFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return false;

    std::lock_guard<std::mutex> lock(mtx_);
    std::string line;
    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;
        size_t eq = line.find('=');
        if (eq != std::string::npos) {
            std::string key = toLower(trim(line.substr(0, eq)));
            std::string val = trim(line.substr(eq + 1));
            templates_[key] = val;
        }
    }
    return true;
}

bool ConfigManager::loadKeywords(const std::string& path, std::unordered_set<std::string>& out) {
    std::ifstream file(path);
    if (!file.is_open()) return false;

    std::string line;
    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#' || line[0] == '[') continue;
        out.insert(toLower(line));
    }
    {
        std::lock_guard<std::mutex> lock(mtx_);
        for (const auto& k : out) {
            keywords_.insert(k);
        }
    }
    return true;
}

bool ConfigManager::loadTemplates(const std::string& path, std::unordered_map<std::string, std::string>& out) {
    std::ifstream file(path);
    if (!file.is_open()) return false;

    std::string line;
    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;
        size_t sep = line.find('|');
        if (sep == std::string::npos) sep = line.find('=');
        if (sep != std::string::npos) {
            std::string key = trim(line.substr(0, sep));
            std::string val = trim(line.substr(sep + 1));
            out[key] = val;
        }
    }
    {
        std::lock_guard<std::mutex> lock(mtx_);
        for (const auto& kv : out) {
            templates_[toLower(kv.first)] = kv.second;
        }
    }
    return true;
}

bool ConfigManager::loadPatterns(const std::string& path, std::vector<std::pair<std::string, float>>& out) {
    std::ifstream file(path);
    if (!file.is_open()) return false;

    std::string line;
    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;
        size_t pipe = line.find('|');
        if (pipe != std::string::npos) {
            std::string prefix = toLower(trim(line.substr(0, pipe)));
            float score = std::stof(trim(line.substr(pipe + 1)));
            out.emplace_back(prefix, score);
        }
    }
    {
        std::lock_guard<std::mutex> lock(mtx_);
        for (const auto& item : out) {
            pattern_scores_[item.first] = item.second;
        }
    }
    return true;
}

bool ConfigManager::loadFloatConfig(const std::string& path, std::unordered_map<std::string, float>& out) {
    std::ifstream file(path);
    if (!file.is_open()) return false;

    std::string line;
    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;
        size_t eq = line.find('=');
        if (eq != std::string::npos) {
            std::string key = toLower(trim(line.substr(0, eq)));
            try {
                float val = std::stof(trim(line.substr(eq + 1)));
                out[key] = val;
            } catch (...) {
                // Ignore non-float lines in key=value files
            }
        }
    }
    return true;
}

bool ConfigManager::loadToolPaths(const std::string& path, std::unordered_map<std::string, std::vector<std::string>>& out) {
    std::ifstream file(path);
    if (!file.is_open()) return false;

    std::string line;
    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;
        std::stringstream ss(line);
        std::string tool;
        if (std::getline(ss, tool, '|')) {
            tool = toLower(trim(tool));
            std::vector<std::string> paths;
            std::string p;
            while (std::getline(ss, p, '|')) {
                p = trim(p);
                if (!p.empty()) paths.push_back(p);
            }
            out[tool] = paths;
        }
    }
    {
        std::lock_guard<std::mutex> lock(mtx_);
        tool_paths_ = out;
    }
    return true;
}

bool ConfigManager::loadSecurityPaths(const std::string& path, 
                                     std::vector<std::string>& denied_win,
                                     std::vector<std::string>& denied_unix) {
    std::ifstream file(path);
    if (!file.is_open()) return false;

    std::string line;
    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;
        size_t pipe = line.find('|');
        if (pipe != std::string::npos) {
            std::string platform = toLower(trim(line.substr(0, pipe)));
            std::string p = trim(line.substr(pipe + 1));
            if (platform == "win") denied_win.push_back(p);
            else if (platform == "unix") denied_unix.push_back(p);
        }
    }
    return true;
}

bool ConfigManager::loadBootstrapKnowledge(const std::string& path,
                                           std::vector<std::tuple<std::string, std::string, float>>& out) {
    std::ifstream file(path);
    if (!file.is_open()) return false;

    std::string line;
    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;
        std::stringstream ss(line);
        std::string cat, text, confStr;
        if (std::getline(ss, cat, '|') && std::getline(ss, text, '|') && std::getline(ss, confStr, '|')) {
            out.emplace_back(trim(cat), trim(text), std::stof(trim(confStr)));
        }
    }
    return true;
}

bool ConfigManager::loadVseFeatures(const std::string& path,
                                    std::unordered_map<std::string, std::vector<float>>& out) {
    std::ifstream file(path);
    if (!file.is_open()) return false;

    std::string line;
    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;
        size_t pipe = line.find('|');
        if (pipe != std::string::npos) {
            std::string label = trim(line.substr(0, pipe));
            std::string featsStr = trim(line.substr(pipe + 1));
            std::stringstream ss(featsStr);
            std::string valStr;
            std::vector<float> vec;
            while (std::getline(ss, valStr, ',')) {
                vec.push_back(std::stof(trim(valStr)));
            }
            out[label] = vec;
        }
    }
    return true;
}

std::string ConfigManager::getTemplate(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = templates_.find(toLower(key));
    if (it != templates_.end()) return it->second;
    return "";
}

bool ConfigManager::isKeyword(const std::string& word) const {
    std::lock_guard<std::mutex> lock(mtx_);
    return keywords_.find(toLower(word)) != keywords_.end();
}

float ConfigManager::getPatternScore(const std::string& prefix) const {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = pattern_scores_.find(toLower(prefix));
    if (it != pattern_scores_.end()) return it->second;
    return 0.0f;
}

std::vector<std::string> ConfigManager::getToolPathHints(const std::string& tool) const {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = tool_paths_.find(toLower(tool));
    if (it != tool_paths_.end()) return it->second;
    return {};
}

} // namespace yuki
