#include "ProceduralStore.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <filesystem>

namespace yuki::memory {

std::string ProceduralStore::typeToString(ProceduralStore::BlobType t) {
    switch (t) {
        case ProceduralStore::BlobType::DMC_WEIGHTS: return "dmc_weights";
        case ProceduralStore::BlobType::SKILL_COMPILED: return "skill_compiled";
        case ProceduralStore::BlobType::SESSION_CHECKPOINT: return "session_checkpoint";
        case ProceduralStore::BlobType::GENERATIVE_MODEL: return "generative_model";
    }
    return "unknown";
}

ProceduralStore::BlobType ProceduralStore::stringToType(const std::string& s) {
    if (s == "dmc_weights") return ProceduralStore::BlobType::DMC_WEIGHTS;
    if (s == "skill_compiled") return ProceduralStore::BlobType::SKILL_COMPILED;
    if (s == "session_checkpoint") return ProceduralStore::BlobType::SESSION_CHECKPOINT;
    if (s == "generative_model") return ProceduralStore::BlobType::GENERATIVE_MODEL;
    return ProceduralStore::BlobType::DMC_WEIGHTS;
}

ProceduralStore::ProceduralStore() = default;

ProceduralStore::~ProceduralStore() {
    close();
}

bool ProceduralStore::init(const std::string& dbPath, const std::string& fsPath) {
    std::lock_guard<std::mutex> lock(mu_);
    db_path_ = dbPath;
    fs_path_ = fsPath;

    try {
        std::filesystem::create_directories(fs_path_);
    } catch (const std::exception& e) {
        std::cerr << "[ProceduralStore] FS mkdir failed: " << e.what() << "\n";
    }

    int rc = sqlite3_open(dbPath.c_str(), &db_);
    if (rc != SQLITE_OK) {
        std::cerr << "[ProceduralStore] DB open failed: " << sqlite3_errmsg(db_) << "\n";
        db_ = nullptr;
        return false;
    }

    // WAL mode for concurrency
    sqlite3_exec(db_, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
    sqlite3_exec(db_, "PRAGMA synchronous=NORMAL;", nullptr, nullptr, nullptr);

    return ensureSchema();
}

void ProceduralStore::close() {
    std::lock_guard<std::mutex> lock(mu_);
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

bool ProceduralStore::ensureSchema() {
    const char* sql = R"SQL(
        CREATE TABLE IF NOT EXISTS procedural_blobs (
            id TEXT PRIMARY KEY,
            type TEXT NOT NULL,
            blob BLOB,
            hash TEXT NOT NULL,
            created_ts INTEGER NOT NULL,
            access_count INTEGER NOT NULL DEFAULT 0,
            size_bytes INTEGER NOT NULL,
            storage_mode TEXT NOT NULL DEFAULT 'DB'
        );
        CREATE INDEX IF NOT EXISTS idx_type ON procedural_blobs(type);
        CREATE INDEX IF NOT EXISTS idx_created ON procedural_blobs(created_ts);
    )SQL";

    char* err = nullptr;
    int rc = sqlite3_exec(db_, sql, nullptr, nullptr, &err);
    if (rc != SQLITE_OK) {
        std::cerr << "[ProceduralStore] Schema: " << (err ? err : "unknown") << "\n";
        sqlite3_free(err);
        return false;
    }
    return true;
}

std::string ProceduralStore::computeHash(const std::vector<uint8_t>& data) const {
    // 128-bit FNV-1a variant — robust, no external deps
    uint64_t h1 = 0xCBF29CE484222325ULL;
    uint64_t h2 = 0x84222325CBF29CE4ULL;
    for (size_t i = 0; i < data.size(); i += 8) {
        uint64_t chunk = 0;
        for (size_t j = 0; j < 8 && (i + j) < data.size(); ++j) {
            chunk |= static_cast<uint64_t>(data[i + j]) << (j * 8);
        }
        h1 ^= chunk; h1 *= 0x100000001B3ULL;
        h2 ^= chunk; h2 *= 0x100000001B3ULL;
        h1 ^= h2 >> 32; h2 ^= h1 << 32;
    }
    std::ostringstream oss;
    oss << std::hex << std::setfill('0') << std::setw(16) << h1 << std::setw(16) << h2;
    return oss.str();
}

bool ProceduralStore::store(const std::string& id, ProceduralStore::BlobType type,
                            const std::vector<uint8_t>& blob) {
    std::lock_guard<std::mutex> lock(mu_);
    if (!db_) return false;

    std::string hash = computeHash(blob);
    int64_t now = std::chrono::system_clock::now().time_since_epoch().count();
    size_t sz = blob.size();

    std::string storage_mode = "DB";
    if (sz > 65536) {
        storage_mode = "FS";
        if (!writeFsBlob(id, blob)) {
            std::cerr << "[ProceduralStore] FS write failed for " << id << "\n";
            return false;
        }
    }

    const char* sql = R"SQL(
        INSERT INTO procedural_blobs(id,type,blob,hash,created_ts,access_count,size_bytes,storage_mode)
        VALUES(?,?,?,?,?,?,?,?)
        ON CONFLICT(id) DO UPDATE SET
            type=excluded.type, blob=excluded.blob, hash=excluded.hash,
            created_ts=excluded.created_ts, access_count=0,
            size_bytes=excluded.size_bytes, storage_mode=excluded.storage_mode;
    )SQL";

    sqlite3_stmt* stmt = nullptr;
    for (int retry = 0; retry < MAX_RETRIES; ++retry) {
        int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            std::this_thread::sleep_for(std::chrono::milliseconds(RETRY_MS));
            continue;
        }

        sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, typeToString(type).c_str(), -1, SQLITE_STATIC);
        if (storage_mode == "DB") {
            sqlite3_bind_blob(stmt, 3, blob.data(), static_cast<int>(sz), SQLITE_STATIC);
        } else {
            sqlite3_bind_null(stmt, 3);
        }
        sqlite3_bind_text(stmt, 4, hash.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int64(stmt, 5, now);
        sqlite3_bind_int(stmt, 6, 0);
        sqlite3_bind_int64(stmt, 7, static_cast<sqlite3_int64>(sz));
        sqlite3_bind_text(stmt, 8, storage_mode.c_str(), -1, SQLITE_STATIC);

        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        if (rc == SQLITE_DONE) return true;

        std::cerr << "[ProceduralStore] Store retry " << retry << ": " << sqlite3_errmsg(db_) << "\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(RETRY_MS));
    }
    return false;
}

std::optional<std::vector<uint8_t>> ProceduralStore::retrieve(const std::string& id) {
    std::lock_guard<std::mutex> lock(mu_);
    auto blob = retrieveInternal(id);
    if (!blob) return std::nullopt;

    // Update access count
    const char* upd = "UPDATE procedural_blobs SET access_count=access_count+1 WHERE id=?";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, upd, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_STATIC);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    return blob;
}

std::optional<std::vector<uint8_t>> ProceduralStore::retrieveInternal(const std::string& id) const {
    if (!db_) return std::nullopt;

    const char* sql = "SELECT blob, hash, storage_mode FROM procedural_blobs WHERE id=?";
    sqlite3_stmt* stmt = nullptr;

    for (int retry = 0; retry < MAX_RETRIES; ++retry) {
        int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            std::this_thread::sleep_for(std::chrono::milliseconds(RETRY_MS));
            continue;
        }

        sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_STATIC);
        rc = sqlite3_step(stmt);

        if (rc == SQLITE_ROW) {
            std::string stored_hash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            std::string mode = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));

            std::vector<uint8_t> blob;
            if (mode == "FS") {
                sqlite3_finalize(stmt);
                auto fs = readFsBlob(id);
                if (!fs) return std::nullopt;
                blob = std::move(*fs);
            } else {
                const void* data = sqlite3_column_blob(stmt, 0);
                int len = sqlite3_column_bytes(stmt, 0);
                blob.resize(len);
                if (len > 0) std::memcpy(blob.data(), data, len);
                sqlite3_finalize(stmt);
            }

            // Hash verification
            if (computeHash(blob) != stored_hash) {
                std::cerr << "[ProceduralStore] Hash mismatch: " << id << "\n";
                return std::nullopt;
            }

            return blob;
        }

        sqlite3_finalize(stmt);
        if (rc == SQLITE_DONE) return std::nullopt;

        std::this_thread::sleep_for(std::chrono::milliseconds(RETRY_MS));
    }
    return std::nullopt;
}

std::vector<std::string> ProceduralStore::list(ProceduralStore::BlobType type) const {
    std::lock_guard<std::mutex> lock(mu_);
    std::vector<std::string> result;
    if (!db_) return result;

    const char* sql = "SELECT id FROM procedural_blobs WHERE type=? ORDER BY created_ts DESC";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, typeToString(type).c_str(), -1, SQLITE_STATIC);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            result.emplace_back(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
        }
        sqlite3_finalize(stmt);
    }
    return result;
}

bool ProceduralStore::erase(const std::string& id) {
    std::lock_guard<std::mutex> lock(mu_);
    if (!db_) return false;

    // Check if FS blob exists
    const char* sql = "SELECT storage_mode FROM procedural_blobs WHERE id=?";
    sqlite3_stmt* stmt = nullptr;
    std::string mode;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            mode = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        }
        sqlite3_finalize(stmt);
    }

    if (mode == "FS") deleteFsBlob(id);

    const char* del = "DELETE FROM procedural_blobs WHERE id=?";
    if (sqlite3_prepare_v2(db_, del, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_STATIC);
        bool ok = sqlite3_step(stmt) == SQLITE_DONE;
        sqlite3_finalize(stmt);
        return ok;
    }
    return false;
}

bool ProceduralStore::exists(const std::string& id) const {
    std::lock_guard<std::mutex> lock(mu_);
    if (!db_) return false;
    const char* sql = "SELECT 1 FROM procedural_blobs WHERE id=?";
    sqlite3_stmt* stmt = nullptr;
    bool found = false;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_STATIC);
        found = sqlite3_step(stmt) == SQLITE_ROW;
        sqlite3_finalize(stmt);
    }
    return found;
}

std::optional<ProceduralStore::Metadata> ProceduralStore::getMetadata(const std::string& id) const {
    std::lock_guard<std::mutex> lock(mu_);
    if (!db_) return std::nullopt;
    const char* sql = "SELECT type,hash,created_ts,access_count,size_bytes FROM procedural_blobs WHERE id=?";
    sqlite3_stmt* stmt = nullptr;
    Metadata m;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            m.id = id;
            m.type = stringToType(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
            m.hash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            m.created_ts = sqlite3_column_int64(stmt, 2);
            m.access_count = sqlite3_column_int(stmt, 3);
            m.size_bytes = static_cast<size_t>(sqlite3_column_int64(stmt, 4));
            sqlite3_finalize(stmt);
            return m;
        }
        sqlite3_finalize(stmt);
    }
    return std::nullopt;
}

size_t ProceduralStore::totalBlobCount() const {
    std::lock_guard<std::mutex> lock(mu_);
    if (!db_) return 0;
    const char* sql = "SELECT COUNT(*) FROM procedural_blobs";
    sqlite3_stmt* stmt = nullptr;
    size_t n = 0;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            n = static_cast<size_t>(sqlite3_column_int64(stmt, 0));
        }
        sqlite3_finalize(stmt);
    }
    return n;
}

size_t ProceduralStore::totalByteSize() const {
    std::lock_guard<std::mutex> lock(mu_);
    if (!db_) return 0;
    const char* sql = "SELECT COALESCE(SUM(size_bytes),0) FROM procedural_blobs";
    sqlite3_stmt* stmt = nullptr;
    size_t n = 0;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            n = static_cast<size_t>(sqlite3_column_int64(stmt, 0));
        }
        sqlite3_finalize(stmt);
    }
    return n;
}

bool ProceduralStore::verifyIntegrity(const std::string& id) const {
    std::lock_guard<std::mutex> lock(mu_);
    auto blob = retrieveInternal(id);
    if (!blob) return false;
    return true; // retrieveInternal already verifies computed hash against stored hash
}

bool ProceduralStore::writeFsBlob(const std::string& id, const std::vector<uint8_t>& data) {
    std::string path = fs_path_ + "/" + id + ".bin";
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f.write(reinterpret_cast<const char*>(data.data()), data.size());
    return f.good();
}

std::optional<std::vector<uint8_t>> ProceduralStore::readFsBlob(const std::string& id) const {
    std::string path = fs_path_ + "/" + id + ".bin";
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return std::nullopt;
    auto sz = f.tellg();
    f.seekg(0, std::ios::beg);
    std::vector<uint8_t> data(static_cast<size_t>(sz));
    f.read(reinterpret_cast<char*>(data.data()), sz);
    return data;
}

bool ProceduralStore::deleteFsBlob(const std::string& id) {
    try {
        return std::filesystem::remove(fs_path_ + "/" + id + ".bin");
    } catch (...) {
        return false;
    }
}

} // namespace yuki::memory
