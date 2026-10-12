// YNCCheckpoint.cpp — static save/load wrapper + directory scan.
#include "YNCCheckpoint.h"
#include <fstream>

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#endif

#include <sqlite3.h>
#include <filesystem>
#include <chrono>
#include <iostream>
#include <cmath>

namespace ync {

std::unordered_map<std::string, float> YNCCheckpoint::last_checkpointed_weights_;
std::mutex YNCCheckpoint::cache_mutex_;

bool YNCCheckpoint::save(const NeuromorphicSimulator& sim, const std::string& path) {
    sim.saveCheckpoint(path);
    std::ifstream verify(path, std::ios::binary);
    return verify.good();
}

bool YNCCheckpoint::load(NeuromorphicSimulator& sim, const std::string& path) {
    return sim.loadCheckpoint(path);
}

std::vector<std::string> YNCCheckpoint::listCheckpoints(const std::string& directory) {
    std::vector<std::string> results;
#ifdef _WIN32
    std::string search_path = directory + "\\*.ynck";
    WIN32_FIND_DATAA find_data;
    HANDLE handle = FindFirstFileA(search_path.c_str(), &find_data);
    if (handle != INVALID_HANDLE_VALUE) {
        do {
            results.push_back(directory + "\\" + find_data.cFileName);
        } while (FindNextFileA(handle, &find_data));
        FindClose(handle);
    }
#else
    DIR* dir = opendir(directory.c_str());
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            std::string name = entry->d_name;
            if (name.size() > 5 && name.substr(name.size() - 5) == ".ynck") {
                results.push_back(directory + "/" + name);
            }
        }
        closedir(dir);
    }
#endif
    return results;
}

bool YNCCheckpoint::checkpointToSQLite(const NeuromorphicSimulator& sim, const std::string& db_path) {
    try { std::filesystem::create_directories(std::filesystem::path(db_path).parent_path()); } catch (...) {}

    sqlite3* db = nullptr;
    if (sqlite3_open(db_path.c_str(), &db) != SQLITE_OK) {
        std::cerr << "[YNCCheckpoint] Cannot open SQLite DB: " << db_path << "\n";
        if (db) sqlite3_close(db);
        return false;
    }

    const char* schema =
        "CREATE TABLE IF NOT EXISTS ync_synaptic_weights ("
        "  synapse_id TEXT PRIMARY KEY,"
        "  source_neuron INTEGER NOT NULL,"
        "  target_neuron INTEGER NOT NULL,"
        "  weight REAL NOT NULL,"
        "  delay INTEGER NOT NULL,"
        "  last_updated INTEGER NOT NULL"
        ");";

    char* err = nullptr;
    if (sqlite3_exec(db, schema, nullptr, nullptr, &err) != SQLITE_OK) {
        std::cerr << "[YNCCheckpoint] Schema error: " << (err ? err : "unknown") << "\n";
        if (err) sqlite3_free(err);
        sqlite3_close(db);
        return false;
    }

    sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);

    const char* sql = "INSERT OR REPLACE INTO ync_synaptic_weights (synapse_id, source_neuron, target_neuron, weight, delay, last_updated) VALUES (?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        sqlite3_close(db);
        return false;
    }

    uint64_t now_ts = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());

    std::lock_guard<std::mutex> lock(cache_mutex_);

    for (const auto& neuron : sim.neurons) {
        for (const auto& ax : neuron.axons) {
            std::string syn_id = "syn_" + std::to_string(neuron.id) + "_" + std::to_string(ax.target_id);

            sqlite3_reset(stmt);
            sqlite3_bind_text(stmt, 1, syn_id.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int64(stmt, 2, neuron.id);
            sqlite3_bind_int64(stmt, 3, ax.target_id);
            sqlite3_bind_double(stmt, 4, static_cast<double>(ax.weight));
            sqlite3_bind_int(stmt, 5, ax.delay);
            sqlite3_bind_int64(stmt, 6, now_ts);
            sqlite3_step(stmt);

            last_checkpointed_weights_[syn_id] = ax.weight;
        }
    }

    sqlite3_finalize(stmt);
    sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);
    sqlite3_close(db);
    return true;
}

size_t YNCCheckpoint::checkpointIncrementalToSQLite(const NeuromorphicSimulator& sim, const std::string& db_path) {
    try { std::filesystem::create_directories(std::filesystem::path(db_path).parent_path()); } catch (...) {}

    sqlite3* db = nullptr;
    if (sqlite3_open(db_path.c_str(), &db) != SQLITE_OK) {
        if (db) sqlite3_close(db);
        return 0;
    }

    const char* schema =
        "CREATE TABLE IF NOT EXISTS ync_synaptic_weights ("
        "  synapse_id TEXT PRIMARY KEY,"
        "  source_neuron INTEGER NOT NULL,"
        "  target_neuron INTEGER NOT NULL,"
        "  weight REAL NOT NULL,"
        "  delay INTEGER NOT NULL,"
        "  last_updated INTEGER NOT NULL"
        ");";

    char* err = nullptr;
    sqlite3_exec(db, schema, nullptr, nullptr, &err);
    if (err) sqlite3_free(err);

    sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);

    const char* sql = "INSERT OR REPLACE INTO ync_synaptic_weights (synapse_id, source_neuron, target_neuron, weight, delay, last_updated) VALUES (?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        sqlite3_close(db);
        return 0;
    }

    uint64_t now_ts = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());

    std::lock_guard<std::mutex> lock(cache_mutex_);
    size_t changed_count = 0;

    for (const auto& neuron : sim.neurons) {
        for (const auto& ax : neuron.axons) {
            std::string syn_id = "syn_" + std::to_string(neuron.id) + "_" + std::to_string(ax.target_id);

            auto it = last_checkpointed_weights_.find(syn_id);
            bool is_changed = (it == last_checkpointed_weights_.end()) || (std::abs(it->second - ax.weight) > 1e-4f);

            if (is_changed) {
                sqlite3_reset(stmt);
                sqlite3_bind_text(stmt, 1, syn_id.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_int64(stmt, 2, neuron.id);
                sqlite3_bind_int64(stmt, 3, ax.target_id);
                sqlite3_bind_double(stmt, 4, static_cast<double>(ax.weight));
                sqlite3_bind_int(stmt, 5, ax.delay);
                sqlite3_bind_int64(stmt, 6, now_ts);
                sqlite3_step(stmt);

                last_checkpointed_weights_[syn_id] = ax.weight;
                ++changed_count;
            }
        }
    }

    sqlite3_finalize(stmt);
    sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);
    sqlite3_close(db);
    return changed_count;
}

bool YNCCheckpoint::restoreFromCheckpoint(NeuromorphicSimulator& sim, const std::string& db_path) {
    if (!std::filesystem::exists(db_path)) return false;

    sqlite3* db = nullptr;
    if (sqlite3_open(db_path.c_str(), &db) != SQLITE_OK) {
        if (db) sqlite3_close(db);
        return false;
    }

    std::unordered_map<uint32_t, Neuron*> neuron_map;
    for (auto& n : sim.neurons) {
        neuron_map[n.id] = &n;
    }

    const char* sql = "SELECT source_neuron, target_neuron, weight FROM ync_synaptic_weights;";
    sqlite3_stmt* stmt = nullptr;
    size_t restored = 0;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            uint32_t src = static_cast<uint32_t>(sqlite3_column_int64(stmt, 0));
            uint32_t tgt = static_cast<uint32_t>(sqlite3_column_int64(stmt, 1));
            float w = static_cast<float>(sqlite3_column_double(stmt, 2));

            auto it = neuron_map.find(src);
            if (it != neuron_map.end() && it->second) {
                for (auto& ax : it->second->axons) {
                    if (ax.target_id == tgt) {
                        ax.weight = w;
                        std::string syn_id = "syn_" + std::to_string(src) + "_" + std::to_string(tgt);
                        last_checkpointed_weights_[syn_id] = w;
                        ++restored;
                        break;
                    }
                }
            }
        }
        sqlite3_finalize(stmt);
    }

    sqlite3_close(db);
    return restored > 0;
}

} // namespace ync
