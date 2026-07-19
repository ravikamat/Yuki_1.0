#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>

struct ResponseTemplate {
    std::string text;
    std::string emotion_tag;
    int         variation_index = 0;
};

class UniversalCache {
public:
    static UniversalCache& instance();

    void preload();
    void reload();

    // Returns templates ordered by variation_index. Empty if not found.
    std::vector<ResponseTemplate> getTemplates(
        const std::string& responseKey,
        const std::string& language) const;

    std::string getSetting(
        const std::string& key,
        const std::string& defaultVal = "") const;

private:
    UniversalCache() = default;

    mutable std::mutex cacheMutex_;

    // [response_key][language] -> ordered templates
    std::unordered_map<
        std::string,
        std::unordered_map<std::string, std::vector<ResponseTemplate>>
    > responseCache_;

    std::unordered_map<std::string, std::string> settingsCache_;
};
