#include "UniversalCache.h"
#include "DatabaseManager.h"
#include <iostream>
#include <algorithm>

UniversalCache& UniversalCache::instance() {
    static UniversalCache inst;
    return inst;
}

void UniversalCache::preload() {
    std::lock_guard<std::mutex> lock(cacheMutex_);
    responseCache_.clear();
    settingsCache_.clear();

    sqlite3* db = DatabaseManager::instance().rawHandle();
    if (!db) {
        std::cerr << "[Cache] DB not open during preload.\n";
        return;
    }

    // Load response templates
    {
        sqlite3_stmt* stmt = nullptr;
        const char* sql =
            "SELECT response_key, language, response_text, emotion_tag, variation_index "
            "FROM global_response_templates ORDER BY response_key, language, variation_index ASC;";
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                auto col = [&](int i) -> std::string {
                    const char* p = reinterpret_cast<const char*>(sqlite3_column_text(stmt, i));
                    return p ? std::string(p) : "";
                };
                ResponseTemplate t;
                std::string key  = col(0);
                std::string lang = col(1);
                t.text           = col(2);
                t.emotion_tag    = col(3).empty() ? "NEUTRAL" : col(3);
                t.variation_index = sqlite3_column_int(stmt, 4);
                responseCache_[key][lang].push_back(std::move(t));
            }
        } else {
            std::cerr << "[Cache] Template query prepare failed.\n";
        }
        sqlite3_finalize(stmt);
    }

    // Load settings
    {
        sqlite3_stmt* stmt = nullptr;
        const char* sql = "SELECT setting_key, setting_value FROM global_settings;";
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                const char* k = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                const char* v = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
                if (k && v) settingsCache_[k] = v;
            }
        }
        sqlite3_finalize(stmt);
    }

    std::cout << "[Cache] Preloaded " << responseCache_.size()
              << " response keys, " << settingsCache_.size() << " settings.\n";
}

void UniversalCache::reload() {
    preload();
}

std::vector<ResponseTemplate> UniversalCache::getTemplates(
    const std::string& responseKey,
    const std::string& language) const
{
    std::lock_guard<std::mutex> lock(cacheMutex_);
    auto keyIt = responseCache_.find(responseKey);
    if (keyIt == responseCache_.end()) return {};
    auto langIt = keyIt->second.find(language);
    if (langIt == keyIt->second.end()) return {};
    return langIt->second;
}

std::string UniversalCache::getSetting(
    const std::string& key,
    const std::string& defaultVal) const
{
    std::lock_guard<std::mutex> lock(cacheMutex_);
    auto it = settingsCache_.find(key);
    return (it != settingsCache_.end()) ? it->second : defaultVal;
}
