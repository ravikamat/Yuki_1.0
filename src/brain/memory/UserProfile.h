#pragma once
#include <string>
#include <unordered_map>
#include <cstdint>

#include "brain/database/DatabaseManager.h"

namespace yuki::memory {


struct UserProfile {
    int64_t id = 1;
    std::string name = "User";
    std::string preferred_browser = "Chrome";
    std::string preferred_editor = "VSCode";
    int interaction_count = 0;
    int64_t created_at = 0;
    int64_t updated_at = 0;

    std::unordered_map<std::string, int> frequent_intents;
    std::unordered_map<std::string, std::string> facts;

    bool save(yuki::database::DatabaseManager* db);
    bool load(yuki::database::DatabaseManager* db, int64_t profile_id = 1);
};

} // namespace yuki::memory
