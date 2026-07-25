#include "brain/self/IdentityPersistence.h"
#include "brain/self/SelfModel.h"
#include "brain/self/TheoryOfMind.h"
#include "brain/emotion/ValenceArousalModel.h"
#include "brain/organism/ConfidenceCalibrator.h"
#include "brain/core/Logger.h"

#include <sqlite3.h>
#include <chrono>
#include <sstream>
#include <cmath>
#include <cstring>
#include <algorithm>

namespace yuki { namespace self {

class IdentityPersistence::Impl {
public:
    std::string dbPath_;
    sqlite3* db_ = nullptr;

    explicit Impl(const std::string& path) : dbPath_(path) {}

    ~Impl() {
        if (db_) {
            sqlite3_close(db_);
            db_ = nullptr;
        }
    }

    bool openDB() {
        if (db_) return true;
        int rc = sqlite3_open(dbPath_.c_str(), &db_);
        if (rc != SQLITE_OK) {
            yuki::core::Logger::instance().log(yuki::core::LogLevel::ERROR, "Failed to open SQLite database at " + dbPath_);
            return false;
        }
        return true;
    }

    bool execSQL(const std::string& sql) {
        if (!openDB()) return false;
        char* err = nullptr;
        int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err);
        if (rc != SQLITE_OK) {
            std::string msg = err ? err : "unknown error";
            sqlite3_free(err);
            yuki::core::Logger::instance().log(yuki::core::LogLevel::ERROR, "SQL execution failed: " + msg);
            return false;
        }
        return true;
    }
};

IdentityPersistence::IdentityPersistence(const std::string& dbPath)
    : pImpl(std::make_unique<Impl>(dbPath)) {
    initializeSchema();
}

IdentityPersistence::~IdentityPersistence() = default;

IdentityPersistence::IdentityPersistence(IdentityPersistence&&) noexcept = default;
IdentityPersistence& IdentityPersistence::operator=(IdentityPersistence&&) noexcept = default;

uint64_t IdentityPersistence::computeFNV1a(const std::vector<uint8_t>& data, uint64_t previousHash) const {
    uint64_t hash = previousHash == 0 ? 0xcbf29ce484222325ULL : previousHash;
    for (uint8_t byte : data) {
        hash ^= byte;
        hash *= 0x100000001b3ULL;
    }
    return hash;
}

bool IdentityPersistence::initializeSchema() {
    if (!pImpl->openDB()) return false;

    const std::string sql =
        "CREATE TABLE IF NOT EXISTS identity_snapshots ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  timestamp INTEGER NOT NULL,"
        "  version TEXT NOT NULL,"
        "  self_model_blob BLOB NOT NULL,"
        "  theory_of_mind_blob BLOB NOT NULL,"
        "  valence_arousal_blob BLOB NOT NULL,"
        "  confidence_calibrator_blob BLOB NOT NULL,"
        "  previous_hash INTEGER NOT NULL,"
        "  current_hash INTEGER NOT NULL,"
        "  identity_drift REAL NOT NULL"
        ");"
        "CREATE TABLE IF NOT EXISTS identity_evolution ("
        "  snapshot_id INTEGER REFERENCES identity_snapshots(id),"
        "  metric_name TEXT NOT NULL,"
        "  metric_value REAL NOT NULL,"
        "  PRIMARY KEY (snapshot_id, metric_name)"
        ");"
        "CREATE TABLE IF NOT EXISTS autobiographical_entries ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  timestamp INTEGER NOT NULL,"
        "  entry_type TEXT NOT NULL,"
        "  content_blob BLOB NOT NULL,"
        "  related_snapshot_id INTEGER REFERENCES identity_snapshots(id)"
        ");"
        "CREATE TABLE IF NOT EXISTS vae_checkpoints ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  timestamp INTEGER NOT NULL,"
        "  config_blob BLOB NOT NULL,"
        "  weights_blob BLOB NOT NULL"
        ");"
        "CREATE TABLE IF NOT EXISTS creative_concepts ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  timestamp INTEGER NOT NULL,"
        "  source_a_blob BLOB NOT NULL,"
        "  source_b_blob BLOB NOT NULL,"
        "  blend_blob BLOB NOT NULL,"
        "  novelty REAL NOT NULL,"
        "  divergence REAL NOT NULL"
        ");";

    return pImpl->execSQL(sql);
}

bool IdentityPersistence::saveIdentity(const SelfModel& self, const TheoryOfMind& tom,
                                       const ValenceArousalModel& emotion, const ConfidenceCalibrator& calibrator,
                                       const std::string& version) {
    if (!pImpl->openDB()) return false;

    auto bSelf = self.serialize();
    auto bTom = tom.serialize();
    auto bEmo = emotion.serialize();
    auto bCal = calibrator.serialize();

    uint64_t prevHash = getLatestHash();
    std::vector<uint8_t> combined;
    combined.insert(combined.end(), bSelf.begin(), bSelf.end());
    combined.insert(combined.end(), bTom.begin(), bTom.end());
    combined.insert(combined.end(), bEmo.begin(), bEmo.end());
    combined.insert(combined.end(), bCal.begin(), bCal.end());

    uint64_t currHash = computeFNV1a(combined, prevHash);
    double drift = computeLatestDrift();

    uint64_t ts = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    const char* sql = "INSERT INTO identity_snapshots (timestamp, version, self_model_blob, theory_of_mind_blob, valence_arousal_blob, confidence_calibrator_blob, previous_hash, current_hash, identity_drift) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(pImpl->db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_int64(stmt, 1, ts);
    sqlite3_bind_text(stmt, 2, version.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_blob(stmt, 3, bSelf.data(), static_cast<int>(bSelf.size()), SQLITE_TRANSIENT);
    sqlite3_bind_blob(stmt, 4, bTom.data(), static_cast<int>(bTom.size()), SQLITE_TRANSIENT);
    sqlite3_bind_blob(stmt, 5, bEmo.data(), static_cast<int>(bEmo.size()), SQLITE_TRANSIENT);
    sqlite3_bind_blob(stmt, 6, bCal.data(), static_cast<int>(bCal.size()), SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 7, prevHash);
    sqlite3_bind_int64(stmt, 8, currHash);
    sqlite3_bind_double(stmt, 9, drift);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

bool IdentityPersistence::loadLatestIdentity(SelfModel& self, TheoryOfMind& tom,
                                             ValenceArousalModel& emotion, ConfidenceCalibrator& calibrator) {
    if (!pImpl->openDB()) return false;
    const char* sql = "SELECT id FROM identity_snapshots ORDER BY id DESC LIMIT 1;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(pImpl->db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    bool found = (sqlite3_step(stmt) == SQLITE_ROW);
    uint64_t id = 0;
    if (found) id = sqlite3_column_int64(stmt, 0);
    sqlite3_finalize(stmt);

    if (!found) return false;
    return loadIdentityById(id, self, tom, emotion, calibrator);
}

bool IdentityPersistence::loadIdentityById(uint64_t snapshotId, SelfModel& self, TheoryOfMind& tom,
                                           ValenceArousalModel& emotion, ConfidenceCalibrator& calibrator) {
    if (!pImpl->openDB()) return false;
    const char* sql = "SELECT self_model_blob, theory_of_mind_blob, valence_arousal_blob, confidence_calibrator_blob FROM identity_snapshots WHERE id = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(pImpl->db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_int64(stmt, 1, snapshotId);

    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return false;
    }

    auto readBlob = [](sqlite3_stmt* st, int col) {
        const uint8_t* ptr = static_cast<const uint8_t*>(sqlite3_column_blob(st, col));
        int len = sqlite3_column_bytes(st, col);
        return std::vector<uint8_t>(ptr, ptr + len);
    };

    auto bSelf = readBlob(stmt, 0);
    auto bTom  = readBlob(stmt, 1);
    auto bEmo  = readBlob(stmt, 2);
    auto bCal  = readBlob(stmt, 3);
    sqlite3_finalize(stmt);

    bool okSelf = self.deserialize(bSelf);
    bool okTom  = tom.deserialize(bTom);
    bool okEmo  = emotion.deserialize(bEmo);
    bool okCal  = calibrator.deserialize(bCal);

    return okSelf && okTom && okEmo && okCal;
}

std::vector<IdentityTimelineEntry> IdentityPersistence::getTimeline(uint64_t startTime, uint64_t endTime) {
    std::vector<IdentityTimelineEntry> timeline;
    if (!pImpl->openDB()) return timeline;

    const char* sql = "SELECT s.id, s.timestamp, e.metric_name, e.metric_value FROM identity_snapshots s JOIN identity_evolution e ON s.id = e.snapshot_id WHERE s.timestamp >= ? AND s.timestamp <= ? ORDER BY s.timestamp ASC;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(pImpl->db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return timeline;
    sqlite3_bind_int64(stmt, 1, startTime);
    sqlite3_bind_int64(stmt, 2, endTime);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        IdentityTimelineEntry entry;
        entry.snapshotId = sqlite3_column_int64(stmt, 0);
        entry.timestamp = sqlite3_column_int64(stmt, 1);
        const unsigned char* txt = sqlite3_column_text(stmt, 2);
        entry.metricName = txt ? reinterpret_cast<const char*>(txt) : "";
        entry.metricValue = sqlite3_column_double(stmt, 3);
        timeline.push_back(entry);
    }
    sqlite3_finalize(stmt);
    return timeline;
}

std::vector<IdentitySnapshot> IdentityPersistence::getSnapshotHistory(size_t limit) {
    std::vector<IdentitySnapshot> history;
    if (!pImpl->openDB()) return history;

    const char* sql = "SELECT id, timestamp, version, previous_hash, current_hash, identity_drift FROM identity_snapshots ORDER BY id DESC LIMIT ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(pImpl->db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return history;
    sqlite3_bind_int64(stmt, 1, limit);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        IdentitySnapshot snap;
        snap.id = sqlite3_column_int64(stmt, 0);
        snap.timestamp = sqlite3_column_int64(stmt, 1);
        const unsigned char* txt = sqlite3_column_text(stmt, 2);
        snap.version = txt ? reinterpret_cast<const char*>(txt) : "";
        snap.previousHash = sqlite3_column_int64(stmt, 3);
        snap.currentHash = sqlite3_column_int64(stmt, 4);
        snap.identityDrift = sqlite3_column_double(stmt, 5);
        history.push_back(snap);
    }
    sqlite3_finalize(stmt);
    return history;
}

bool IdentityPersistence::addAutobiographicalEntry(const std::string& entryType,
                                                   const std::vector<uint8_t>& content,
                                                   uint64_t relatedSnapshotId) {
    if (!pImpl->openDB()) return false;
    uint64_t ts = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    const char* sql = "INSERT INTO autobiographical_entries (timestamp, entry_type, content_blob, related_snapshot_id) VALUES (?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(pImpl->db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_int64(stmt, 1, ts);
    sqlite3_bind_text(stmt, 2, entryType.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_blob(stmt, 3, content.data(), static_cast<int>(content.size()), SQLITE_TRANSIENT);
    if (relatedSnapshotId > 0) {
        sqlite3_bind_int64(stmt, 4, relatedSnapshotId);
    } else {
        sqlite3_bind_null(stmt, 4);
    }

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

std::vector<AutobiographicalEntry> IdentityPersistence::getAutobiographicalEntries(size_t limit) {
    std::vector<AutobiographicalEntry> entries;
    if (!pImpl->openDB()) return entries;

    const char* sql = "SELECT id, timestamp, entry_type, content_blob, related_snapshot_id FROM autobiographical_entries ORDER BY id DESC LIMIT ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(pImpl->db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return entries;
    sqlite3_bind_int64(stmt, 1, limit);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        AutobiographicalEntry entry;
        entry.id = sqlite3_column_int64(stmt, 0);
        entry.timestamp = sqlite3_column_int64(stmt, 1);
        const unsigned char* txt = sqlite3_column_text(stmt, 2);
        entry.entryType = txt ? reinterpret_cast<const char*>(txt) : "";
        const uint8_t* ptr = static_cast<const uint8_t*>(sqlite3_column_blob(stmt, 3));
        int len = sqlite3_column_bytes(stmt, 3);
        entry.contentBlob = std::vector<uint8_t>(ptr, ptr + len);
        entry.relatedSnapshotId = sqlite3_column_int64(stmt, 4);
        entries.push_back(entry);
    }
    sqlite3_finalize(stmt);
    return entries;
}

std::string IdentityPersistence::generateNarrativeSummary() {
    auto entries = getAutobiographicalEntries(50);
    std::ostringstream oss;
    oss << "SESSIONS=" << getSnapshotCount() << ";ENTRIES=" << entries.size() << ";TYPES=";
    for (size_t i = 0; i < entries.size(); ++i) {
        if (i > 0) oss << ",";
        oss << entries[i].entryType << ":" << entries[i].timestamp;
    }
    return oss.str();
}

double IdentityPersistence::computeDriftBetween(uint64_t snapshotIdA, uint64_t snapshotIdB) {
    SelfModel sA, sB;
    TheoryOfMind tA, tB;
    ValenceArousalModel eA, eB;
    ConfidenceCalibrator cA, cB;

    if (!loadIdentityById(snapshotIdA, sA, tA, eA, cA)) return 0.0;
    if (!loadIdentityById(snapshotIdB, sB, tB, eB, cB)) return 0.0;

    auto bA = sA.serialize();
    auto bB = sB.serialize();
    size_t minLen = std::min(bA.size(), bB.size());
    if (minLen == 0) return 0.0;

    double diff = 0.0;
    for (size_t i = 0; i < minLen; ++i) {
        if (bA[i] != bB[i]) diff += 1.0;
    }
    return diff / minLen;
}

double IdentityPersistence::computeLatestDrift() {
    auto history = getSnapshotHistory(2);
    if (history.size() < 2) return 0.0;
    return computeDriftBetween(history[0].id, history[1].id);
}

bool IdentityPersistence::verifyHashChain() {
    auto history = getSnapshotHistory(1000);
    if (history.size() < 2) return true;

    for (size_t i = 0; i + 1 < history.size(); ++i) {
        if (history[i].previousHash != history[i + 1].currentHash) {
            return false;
        }
    }
    return true;
}

uint64_t IdentityPersistence::getLatestHash() const {
    if (!pImpl->openDB()) return 0;
    const char* sql = "SELECT current_hash FROM identity_snapshots ORDER BY id DESC LIMIT 1;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(pImpl->db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return 0;

    uint64_t hash = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        hash = sqlite3_column_int64(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return hash;
}

size_t IdentityPersistence::getSnapshotCount() const {
    if (!pImpl->openDB()) return 0;
    const char* sql = "SELECT COUNT(*) FROM identity_snapshots;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(pImpl->db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return 0;

    size_t count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = static_cast<size_t>(sqlite3_column_int64(stmt, 0));
    }
    sqlite3_finalize(stmt);
    return count;
}

size_t IdentityPersistence::getEntryCount() const {
    if (!pImpl->openDB()) return 0;
    const char* sql = "SELECT COUNT(*) FROM autobiographical_entries;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(pImpl->db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return 0;

    size_t count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = static_cast<size_t>(sqlite3_column_int64(stmt, 0));
    }
    sqlite3_finalize(stmt);
    return count;
}

bool IdentityPersistence::pruneSnapshots(size_t keepLast) {
    if (!pImpl->openDB()) return false;
    const char* sql = "DELETE FROM identity_snapshots WHERE id NOT IN (SELECT id FROM identity_snapshots ORDER BY id DESC LIMIT ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(pImpl->db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_int64(stmt, 1, keepLast);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

}} // namespace yuki::self
