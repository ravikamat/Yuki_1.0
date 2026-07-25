#include "brain/memory/UserProfile.h"
#include "brain/database/DatabaseManager.h"
#include <chrono>

namespace yuki::memory {

bool UserProfile::save(yuki::database::DatabaseManager* db) {
    auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    if (created_at == 0) created_at = now;
    updated_at = now;

    if (!db) return true; // in-memory fallback succeeded

    // SQLite persistence query via DatabaseManager
    std::string sql = "INSERT OR REPLACE INTO user_profiles (id, name, preferred_browser, preferred_editor, interaction_count, created_at, updated_at) VALUES ("
        + std::to_string(id) + ", '" + name + "', '" + preferred_browser + "', '" + preferred_editor + "', "
        + std::to_string(interaction_count) + ", " + std::to_string(created_at) + ", " + std::to_string(updated_at) + ");";
    return db->execute(sql);
}

bool UserProfile::load(yuki::database::DatabaseManager* db, int64_t profile_id) {
    id = profile_id;
    if (!db) return true; // in-memory fallback

    std::string sql = "SELECT name, preferred_browser, preferred_editor, interaction_count, created_at, updated_at FROM user_profiles WHERE id = "
        + std::to_string(id) + ";";
    auto rows = db->query(sql);
    if (!rows.empty() && rows[0].size() >= 6) {
        name = rows[0][0];
        preferred_browser = rows[0][1];
        preferred_editor = rows[0][2];
        interaction_count = std::stoi(rows[0][3]);
        created_at = std::stoll(rows[0][4]);
        updated_at = std::stoll(rows[0][5]);
        return true;
    }
    return false;
}

} // namespace yuki::memory
