#include "brain/memory/UserProfile.h"
#include "brain/database/DatabaseManager.h"
#include <cassert>
#include <cstdio>

int main() {
    auto& db = DatabaseManager::instance();
    db.init("test_user_profile.db");

    yuki::memory::UserProfile p1;
    p1.id = 42;
    p1.name = "Rahul";
    p1.preferred_browser = "Edge";
    p1.preferred_editor = "VSCode";
    p1.interaction_count = 10;
    p1.frequent_intents["BUILD_APP"] = 5;
    p1.facts["theme"] = "dark";

    // 1. save() creates row in SQLite
    assert(p1.save(&db));

    // 2. load() retrieves same data
    yuki::memory::UserProfile p2;
    assert(p2.load(&db, 42));
    assert(p2.name == "Rahul");
    assert(p2.preferred_browser == "Edge");
    assert(p2.preferred_editor == "VSCode");

    // 3. interaction_count increments correctly
    p2.interaction_count++;
    assert(p2.save(&db));

    yuki::memory::UserProfile p3;
    assert(p3.load(&db, 42));
    assert(p3.interaction_count == 11);

    // 4. in-memory fallback without DB
    yuki::memory::UserProfile p_fallback;
    assert(p_fallback.save(nullptr));
    assert(p_fallback.load(nullptr, 1));

    db.close();
    std::remove("test_user_profile.db");
    return 0;
}
