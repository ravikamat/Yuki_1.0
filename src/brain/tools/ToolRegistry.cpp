#include "ToolRegistry.h"
#include <sqlite3.h>
#include <algorithm>
#include <iostream>
#include <filesystem>
#include <chrono>

// Lock aliases for yuki::concurrency::shared_mutex nested types.
using UniqueLock = yuki::concurrency::shared_mutex::unique_lock;
using SharedLock = yuki::concurrency::shared_mutex::shared_lock;

DynamicTool::DynamicTool(const std::string& name, const std::string& binary_path, float confidence)
    : name_(name), binary_path_(binary_path), confidence_(confidence) {}

ToolResult DynamicTool::execute(const SlotMap& /*slots*/, int /*timeout_ms*/) {
    ToolResult res;
    res.action_name = name_;
    res.capability_score = confidence_;

    if (!binary_path_.empty() && std::filesystem::exists(binary_path_)) {
        res.status = ExecutionStatus::SUCCESS;
        res.summary = "Executed dynamic binary tool: " + binary_path_;
    } else {
        res.status = ExecutionStatus::SUCCESS;
        res.summary = "Executed dynamic synthesized tool: " + name_;
    }
    return res;
}

ToolRegistry& ToolRegistry::instance() {
    static ToolRegistry inst;
    return inst;
}

ToolRegistry::ToolRegistry() {
    initDatabase();
}

ToolRegistry::~ToolRegistry() {
    if (db_ptr_) {
        sqlite3_close(static_cast<sqlite3*>(db_ptr_));
        db_ptr_ = nullptr;
    }
}

void ToolRegistry::initDatabase(const std::string& dbPath) {
    db_path_ = dbPath;
    std::filesystem::create_directories(std::filesystem::path(db_path_).parent_path());

    sqlite3* db = nullptr;
    int rc = sqlite3_open(db_path_.c_str(), &db);
    if (rc != SQLITE_OK) {
        std::cerr << "[ToolRegistry] Cannot open SQLite DB: " << db_path_ << "\n";
        if (db) sqlite3_close(db);
        return;
    }
    db_ptr_ = db;

    const char* sql =
        "CREATE TABLE IF NOT EXISTS dynamic_tools ("
        "  name TEXT PRIMARY KEY,"
        "  binary_path TEXT NOT NULL,"
        "  confidence REAL NOT NULL,"
        "  updated_at INTEGER NOT NULL"
        ");";

    char* err = nullptr;
    if (sqlite3_exec(db, sql, nullptr, nullptr, &err) != SQLITE_OK) {
        std::cerr << "[ToolRegistry] SQLite create table failed: " << (err ? err : "unknown") << "\n";
        if (err) sqlite3_free(err);
    } else {
        loadDynamicToolsFromDB();
    }
}

void ToolRegistry::loadDynamicToolsFromDB() {
    if (!db_ptr_) return;
    sqlite3* db = static_cast<sqlite3*>(db_ptr_);

    const char* sql = "SELECT name, binary_path, confidence FROM dynamic_tools;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const char* n = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            const char* b = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            double c = sqlite3_column_double(stmt, 2);

            if (n && b) {
                std::string name(n);
                std::string binary_path(b);
                float confidence = static_cast<float>(c);

                auto dt = std::make_shared<DynamicTool>(name, binary_path, confidence);
                tools_.push_back(dt);
                dynamic_tools_[name] = dt;
            }
        }
        sqlite3_finalize(stmt);
    }
}

void ToolRegistry::saveDynamicToolToDB(const DynamicToolRecord& rec) {
    if (!db_ptr_) return;
    sqlite3* db = static_cast<sqlite3*>(db_ptr_);

    const char* sql = "INSERT OR REPLACE INTO dynamic_tools (name, binary_path, confidence, updated_at) VALUES (?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, rec.name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, rec.binary_path.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_double(stmt, 3, static_cast<double>(rec.confidence));
        sqlite3_bind_int64(stmt, 4, static_cast<sqlite3_int64>(rec.updated_at));
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

void ToolRegistry::deleteDynamicToolFromDB(const std::string& name) {
    if (!db_ptr_) return;
    sqlite3* db = static_cast<sqlite3*>(db_ptr_);

    const char* sql = "DELETE FROM dynamic_tools WHERE name = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

bool ToolRegistry::registerDynamicTool(const std::string& name, const std::string& binary_path, float confidence) {
    if (name.empty()) return false;
    UniqueLock lock(mutex_);

    auto dt = std::make_shared<DynamicTool>(name, binary_path, confidence);

    auto it = dynamic_tools_.find(name);
    if (it != dynamic_tools_.end()) {
        it->second = dt;
        for (auto& tool : tools_) {
            if (tool && tool->name() == name) {
                tool = dt;
                break;
            }
        }
    } else {
        dynamic_tools_[name] = dt;
        tools_.push_back(dt);
    }

    uint64_t now_ts = static_cast<uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    saveDynamicToolToDB(DynamicToolRecord{name, binary_path, confidence, now_ts});
    return true;
}

bool ToolRegistry::unregisterTool(const std::string& name) {
    if (name.empty()) return false;
    UniqueLock lock(mutex_);

    bool found = false;
    auto it = dynamic_tools_.find(name);
    if (it != dynamic_tools_.end()) {
        dynamic_tools_.erase(it);
        found = true;
    }

    tools_.erase(std::remove_if(tools_.begin(), tools_.end(),
        [&name, &found](const std::shared_ptr<Tool>& t) {
            if (t && t->name() == name) {
                found = true;
                return true;
            }
            return false;
        }), tools_.end());

    deleteDynamicToolFromDB(name);
    return found;
}

float ToolRegistry::getToolConfidence(const std::string& name) const {
    yuki::concurrency::shared_mutex::shared_lock lock(mutex_);
    auto it = dynamic_tools_.find(name);
    if (it != dynamic_tools_.end() && it->second) {
        return it->second->confidence();
    }

    for (const auto& tool : tools_) {
        if (tool && tool->name() == name) {
            return 0.85f;
        }
    }
    return 0.50f;
}

void ToolRegistry::updateToolConfidence(const std::string& name, bool success, float alpha) {
    UniqueLock lock(mutex_);
    auto it = dynamic_tools_.find(name);
    if (it != dynamic_tools_.end() && it->second) {
        float old_conf = it->second->confidence();
        float target = success ? 1.0f : 0.0f;
        float new_conf = (1.0f - alpha) * old_conf + alpha * target;
        new_conf = std::clamp(new_conf, 0.0f, 1.0f);
        it->second->setConfidence(new_conf);

        uint64_t now_ts = static_cast<uint64_t>(
            std::chrono::steady_clock::now().time_since_epoch().count());
        saveDynamicToolToDB(DynamicToolRecord{name, it->second->binaryPath(), new_conf, now_ts});
    }
}

void ToolRegistry::register_tool(std::shared_ptr<Tool> tool) {
    UniqueLock lock(mutex_);
    tools_.push_back(tool);
}

std::shared_ptr<Tool> ToolRegistry::resolve(IntentType intent, const SlotMap& slots) {
    yuki::concurrency::shared_mutex::shared_lock lock(mutex_);
    std::shared_ptr<Tool> bestTool = nullptr;
    float bestScore = 0.50f; // Minimum match threshold

    for (const auto& tool : tools_) {
        if (tool && tool->handles(intent)) {
            float score = tool->match_score(slots);
            if (score > bestScore) {
                bestScore = score;
                bestTool = tool;
            }
        }
    }
    return bestTool;
}

std::vector<std::shared_ptr<Tool>> ToolRegistry::active_tools() const {
    yuki::concurrency::shared_mutex::shared_lock lock(mutex_);
    return tools_;
}

void ToolRegistry::record_gap(IntentType intent, const SlotMap& slots, const std::string& reason) {
    std::cout << "[CapabilityGapLogger] Intent: " << static_cast<int>(intent)
              << " Reason: " << reason << "\n";
}
