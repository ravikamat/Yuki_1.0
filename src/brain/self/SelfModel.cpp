#include "brain/self/SelfModel.h"
#include "brain/self/TheoryOfMind.h"
#include "brain/emotion/EmotionSystem.h"
#include "brain/organism/ConfidenceCalibrator.h"
#include "brain/core/Logger.h"

#include <sqlite3.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <sstream>
#include <fstream>
#include <chrono>

namespace yuki { namespace memory { class CognitiveMemoryFabric; } }

// ============================================================================
// SelfModelDelta Implementation
// ============================================================================

namespace yuki::selfmodel {

std::vector<CompetenceGap> SelfModelDelta::computeGaps(
    const std::vector<float>& self_assessed,
    const std::vector<float>& measured) const {

    std::vector<CompetenceGap> gaps;
    size_t count = std::min({self_assessed.size(), measured.size(),
                             static_cast<size_t>(DOMAIN_COUNT)});

    for (size_t i = 0; i < count; ++i) {
        CompetenceGap g;
        g.domain = static_cast<uint32_t>(i);
        g.self_assessed_competence = self_assessed[i];
        g.measured_competence = measured[i];
        g.gap = self_assessed[i] - measured[i];

        float abs_gap = std::abs(g.gap);
        if (g.gap > 0.0f) {
            g.severity = std::min(1.0f, abs_gap * OVERCONFIDENCE_PENALTY);
        } else {
            g.severity = std::min(1.0f, abs_gap);
        }

        gaps.push_back(g);
    }
    return gaps;
}

std::vector<uint32_t> SelfModelDelta::detectDecline(
    const std::vector<std::vector<float>>& competence_history,
    float decline_threshold) const {

    std::vector<uint32_t> declining;
    if (competence_history.size() < 2) return declining;

    size_t domain_count = std::min(competence_history[0].size(),
                                    static_cast<size_t>(DOMAIN_COUNT));

    for (size_t d = 0; d < domain_count; ++d) {
        float first = competence_history.front()[d];
        float window_avg = 0.0f;
        size_t window_size = std::min(size_t{3}, competence_history.size());
        for (size_t i = competence_history.size() - window_size;
             i < competence_history.size(); ++i) {
            window_avg += competence_history[i][d];
        }
        window_avg /= static_cast<float>(window_size);

        if (first - window_avg > decline_threshold) {
            declining.push_back(static_cast<uint32_t>(d));
        }
    }
    return declining;
}

std::vector<uint32_t> SelfModelDelta::detectInstability(
    const std::vector<std::vector<float>>& competence_history,
    float variance_threshold) const {

    std::vector<uint32_t> unstable;
    if (competence_history.size() < 2) return unstable;

    size_t domain_count = std::min(competence_history[0].size(),
                                    static_cast<size_t>(DOMAIN_COUNT));

    for (size_t d = 0; d < domain_count; ++d) {
        float mean = 0.0f;
        for (const auto& h : competence_history) {
            mean += h[d];
        }
        mean /= static_cast<float>(competence_history.size());

        float variance = 0.0f;
        for (const auto& h : competence_history) {
            float diff = h[d] - mean;
            variance += diff * diff;
        }
        variance /= static_cast<float>(competence_history.size());

        if (variance > variance_threshold * variance_threshold) {
            unstable.push_back(static_cast<uint32_t>(d));
        }
    }
    return unstable;
}

} // namespace yuki::selfmodel

// ============================================================================
// IdentityPersistence Implementation
// ============================================================================

namespace yuki::self {

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

    std::string sql;
    std::ifstream schemaFile("data/sql/identity_schema.sql");
    if (schemaFile.is_open()) {
        std::stringstream ss;
        ss << schemaFile.rdbuf();
        sql = ss.str();
    }
    if (sql.empty()) {
        sql =
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
            "  timestamp INTEGER NOT NULL"
            ");"
            "CREATE TABLE IF NOT EXISTS autobiographical_memory ("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  timestamp INTEGER NOT NULL,"
            "  entry_type TEXT NOT NULL,"
            "  content_blob BLOB NOT NULL,"
            "  related_snapshot_id INTEGER REFERENCES identity_snapshots(id)"
            ");";
    }
    return pImpl->execSQL(sql);
}

bool IdentityPersistence::saveIdentity(const SelfModel& self, const TheoryOfMind& tom,
                                        const yuki::emotion::ValenceArousalModel& emotion,
                                        const yuki::organism::ConfidenceCalibrator& calibrator,
                                        const std::string& version) {
    if (!pImpl->openDB()) return false;

    auto smBlob = self.serialize();
    auto tomBlob = tom.serialize();
    auto vaBlob = emotion.serialize();
    auto ccBlob = calibrator.serialize();

    uint64_t prevHash = getLatestHash();
    uint64_t curHash = computeFNV1a(smBlob, prevHash);
    curHash = computeFNV1a(tomBlob, curHash);
    curHash = computeFNV1a(vaBlob, curHash);
    curHash = computeFNV1a(ccBlob, curHash);

    double drift = self.identityDrift();
    uint64_t now = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());

    const char* sql = "INSERT INTO identity_snapshots (timestamp, version, self_model_blob, theory_of_mind_blob, "
                      "valence_arousal_blob, confidence_calibrator_blob, previous_hash, current_hash, identity_drift) "
                      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?);";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(pImpl->db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_int64(stmt, 1, now);
    sqlite3_bind_text(stmt, 2, version.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_blob(stmt, 3, smBlob.data(), static_cast<int>(smBlob.size()), SQLITE_TRANSIENT);
    sqlite3_bind_blob(stmt, 4, tomBlob.data(), static_cast<int>(tomBlob.size()), SQLITE_TRANSIENT);
    sqlite3_bind_blob(stmt, 5, vaBlob.data(), static_cast<int>(vaBlob.size()), SQLITE_TRANSIENT);
    sqlite3_bind_blob(stmt, 6, ccBlob.data(), static_cast<int>(ccBlob.size()), SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 7, prevHash);
    sqlite3_bind_int64(stmt, 8, curHash);
    sqlite3_bind_double(stmt, 9, drift);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

bool IdentityPersistence::loadLatestIdentity(SelfModel& self, TheoryOfMind& tom,
                                              yuki::emotion::ValenceArousalModel& emotion,
                                              yuki::organism::ConfidenceCalibrator& calibrator) {
    if (!pImpl->openDB()) return false;
    const char* sql = "SELECT id FROM identity_snapshots ORDER BY id DESC LIMIT 1;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(pImpl->db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    uint64_t latestId = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        latestId = static_cast<uint64_t>(sqlite3_column_int64(stmt, 0));
    }
    sqlite3_finalize(stmt);

    if (latestId == 0) return false;
    return loadIdentityById(latestId, self, tom, emotion, calibrator);
}

bool IdentityPersistence::loadIdentityById(uint64_t snapshotId, SelfModel& self, TheoryOfMind& tom,
                                            yuki::emotion::ValenceArousalModel& emotion,
                                            yuki::organism::ConfidenceCalibrator& calibrator) {
    if (!pImpl->openDB()) return false;
    const char* sql = "SELECT self_model_blob, theory_of_mind_blob, valence_arousal_blob, confidence_calibrator_blob "
                      "FROM identity_snapshots WHERE id = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(pImpl->db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_int64(stmt, 1, snapshotId);

    bool success = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const uint8_t* smData = static_cast<const uint8_t*>(sqlite3_column_blob(stmt, 0));
        int smLen = sqlite3_column_bytes(stmt, 0);
        const uint8_t* tomData = static_cast<const uint8_t*>(sqlite3_column_blob(stmt, 1));
        int tomLen = sqlite3_column_bytes(stmt, 1);
        const uint8_t* vaData = static_cast<const uint8_t*>(sqlite3_column_blob(stmt, 2));
        int vaLen = sqlite3_column_bytes(stmt, 2);
        const uint8_t* ccData = static_cast<const uint8_t*>(sqlite3_column_blob(stmt, 3));
        int ccLen = sqlite3_column_bytes(stmt, 3);

        std::vector<uint8_t> smBlob(smData, smData + smLen);
        std::vector<uint8_t> tomBlob(tomData, tomData + tomLen);
        std::vector<uint8_t> vaBlob(vaData, vaData + vaLen);
        std::vector<uint8_t> ccBlob(ccData, ccData + ccLen);

        bool b1 = self.deserialize(smBlob);
        bool b2 = tom.deserialize(tomBlob);
        bool b3 = emotion.deserialize(vaBlob);
        bool b4 = calibrator.deserialize(ccBlob);

        success = b1 && b2 && b3 && b4;
    }
    sqlite3_finalize(stmt);
    return success;
}

std::vector<IdentityTimelineEntry> IdentityPersistence::getTimeline(uint64_t startTime, uint64_t endTime) {
    std::vector<IdentityTimelineEntry> entries;
    if (!pImpl->openDB()) return entries;
    const char* sql = "SELECT snapshot_id, timestamp, metric_name, metric_value FROM identity_evolution "
                      "WHERE timestamp >= ? AND timestamp <= ? ORDER BY timestamp ASC;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(pImpl->db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return entries;
    sqlite3_bind_int64(stmt, 1, startTime);
    sqlite3_bind_int64(stmt, 2, endTime);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        IdentityTimelineEntry e;
        e.snapshotId = static_cast<uint64_t>(sqlite3_column_int64(stmt, 0));
        e.timestamp = static_cast<uint64_t>(sqlite3_column_int64(stmt, 1));
        e.metricName = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        e.metricValue = sqlite3_column_double(stmt, 3);
        entries.push_back(e);
    }
    sqlite3_finalize(stmt);
    return entries;
}

std::vector<IdentitySnapshot> IdentityPersistence::getSnapshotHistory(size_t limit) {
    std::vector<IdentitySnapshot> history;
    if (!pImpl->openDB()) return history;
    const char* sql = "SELECT id, timestamp, version, previous_hash, current_hash, identity_drift "
                      "FROM identity_snapshots ORDER BY id DESC LIMIT ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(pImpl->db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return history;
    sqlite3_bind_int64(stmt, 1, limit);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        IdentitySnapshot s;
        s.id = static_cast<uint64_t>(sqlite3_column_int64(stmt, 0));
        s.timestamp = static_cast<uint64_t>(sqlite3_column_int64(stmt, 1));
        s.version = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        s.previousHash = static_cast<uint64_t>(sqlite3_column_int64(stmt, 3));
        s.currentHash = static_cast<uint64_t>(sqlite3_column_int64(stmt, 4));
        s.identityDrift = sqlite3_column_double(stmt, 5);
        history.push_back(s);
    }
    sqlite3_finalize(stmt);
    return history;
}

bool IdentityPersistence::addAutobiographicalEntry(const std::string& entryType, const std::vector<uint8_t>& content, uint64_t relatedSnapshotId) {
    if (!pImpl->openDB()) return false;
    uint64_t now = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
    const char* sql = "INSERT INTO autobiographical_memory (timestamp, entry_type, content_blob, related_snapshot_id) "
                      "VALUES (?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(pImpl->db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_int64(stmt, 1, now);
    sqlite3_bind_text(stmt, 2, entryType.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_blob(stmt, 3, content.data(), static_cast<int>(content.size()), SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 4, relatedSnapshotId);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

std::vector<AutobiographicalEntry> IdentityPersistence::getAutobiographicalEntries(size_t limit) {
    std::vector<AutobiographicalEntry> entries;
    if (!pImpl->openDB()) return entries;
    const char* sql = "SELECT id, timestamp, entry_type, content_blob, related_snapshot_id "
                      "FROM autobiographical_memory ORDER BY id DESC LIMIT ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(pImpl->db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return entries;
    sqlite3_bind_int64(stmt, 1, limit);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        AutobiographicalEntry e;
        e.id = static_cast<uint64_t>(sqlite3_column_int64(stmt, 0));
        e.timestamp = static_cast<uint64_t>(sqlite3_column_int64(stmt, 1));
        e.entryType = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        const uint8_t* blob = static_cast<const uint8_t*>(sqlite3_column_blob(stmt, 3));
        int len = sqlite3_column_bytes(stmt, 3);
        e.contentBlob.assign(blob, blob + len);
        e.relatedSnapshotId = static_cast<uint64_t>(sqlite3_column_int64(stmt, 4));
        entries.push_back(e);
    }
    sqlite3_finalize(stmt);
    return entries;
}

std::string IdentityPersistence::generateNarrativeSummary() {
    auto history = getSnapshotHistory(5);
    std::ostringstream ss;
    ss << "Identity Continuity Log (" << history.size() << " recent snapshots):\n";
    for (const auto& s : history) {
        ss << " - Snapshot #" << s.id << " [v" << s.version << "] Hash: 0x"
           << std::hex << s.currentHash << std::dec << " Drift: " << s.identityDrift << "\n";
    }
    return ss.str();
}

double IdentityPersistence::computeDriftBetween(uint64_t snapshotIdA, uint64_t snapshotIdB) {
    SelfModel sA, sB;
    TheoryOfMind tom;
    yuki::emotion::ValenceArousalModel va;
    yuki::organism::ConfidenceCalibrator cc;

    if (!loadIdentityById(snapshotIdA, sA, tom, va, cc)) return 1.0;
    if (!loadIdentityById(snapshotIdB, sB, tom, va, cc)) return 1.0;

    const auto& vecA = sA.capabilityVector();
    const auto& vecB = sB.capabilityVector();
    double diff = 0.0;
    for (size_t i = 0; i < vecA.size(); ++i) {
        diff += std::abs(vecA[i] - vecB[i]);
    }
    return diff / static_cast<double>(vecA.size());
}

double IdentityPersistence::computeLatestDrift() {
    auto history = getSnapshotHistory(2);
    if (history.size() < 2) return 0.0;
    return computeDriftBetween(history[0].id, history[1].id);
}

bool IdentityPersistence::verifyHashChain() {
    auto history = getSnapshotHistory(100);
    if (history.size() < 2) return true;

    for (size_t i = 0; i < history.size() - 1; ++i) {
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
        hash = static_cast<uint64_t>(sqlite3_column_int64(stmt, 0));
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
    const char* sql = "SELECT COUNT(*) FROM autobiographical_memory;";
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
    const char* sql = "DELETE FROM identity_snapshots WHERE id NOT IN "
                      "(SELECT id FROM identity_snapshots ORDER BY id DESC LIMIT ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(pImpl->db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_int64(stmt, 1, keepLast);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

// ============================================================================
// SelfModel Implementation
// ============================================================================

SelfModel::SelfModel()
    : capability_vector_{},
      checkpoint_vector_{},
      energy_level_(1.0f),
      recent_success_rate_(1.0f),
      turn_count_(0),
      identity_stability_(0.0f) {
    capability_vector_.fill(0.0f);
    checkpoint_vector_.fill(0.0f);
    checkpoint();
}

float SelfModel::clamp01(float v) const {
    return (v < 0.0f) ? 0.0f : ((v > 1.0f) ? 1.0f : v);
}

float SelfModel::vectorDelta(const std::array<float, kCapabilityDims>& a,
                            const std::array<float, kCapabilityDims>& b) const {
    float diff = 0.0f;
    for (size_t i = 0; i < kCapabilityDims; ++i) {
        diff += std::abs(a[i] - b[i]);
    }
    return diff / static_cast<float>(kCapabilityDims);
}

void SelfModel::update(const std::array<float, kCapabilityDims>& competence_vector,
                       float metabolism_viability,
                       const std::array<float, 4>& /*drive_activations*/,
                       bool last_turn_success,
                       float /*last_precision*/) {
    capability_vector_ = competence_vector;
    energy_level_ = clamp01(metabolism_viability);
    recent_success_rate_ = kSuccessEmaAlpha * (last_turn_success ? 1.0f : 0.0f) +
                           (1.0f - kSuccessEmaAlpha) * recent_success_rate_;
    turn_count_.fetch_add(1, std::memory_order_acq_rel);

    float delta = vectorDelta(capability_vector_, checkpoint_vector_);
    identity_stability_ = kIdentityEmaAlpha * delta + (1.0f - kIdentityEmaAlpha) * identity_stability_;
}

float SelfModel::identityStability() const {
    return identity_stability_;
}

float SelfModel::identityDrift() const {
    return vectorDelta(capability_vector_, checkpoint_vector_);
}

uint64_t SelfModel::identityHash() const {
    uint64_t hash = 14695981039346656037ULL; // FNV offset basis
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(capability_vector_.data());
    size_t size = kCapabilityDims * sizeof(float);
    for (size_t i = 0; i < size; ++i) {
        hash ^= bytes[i];
        hash *= 1099511628211ULL; // FNV prime
    }
    return hash;
}

void SelfModel::checkpoint() {
    checkpoint_vector_ = capability_vector_;
}

void SelfModel::consolidate() {
    checkpoint();
    energy_level_ = std::min(1.0f, energy_level_ + 0.1f);
}

std::string SelfModel::toString() const {
    std::ostringstream oss;
    oss << "SelfModel [turns=" << turn_count_.load()
        << ", energy=" << energy_level_
        << ", success_rate=" << recent_success_rate_
        << ", stability=" << identity_stability_ << "]";
    return oss.str();
}

std::vector<uint8_t> SelfModel::serialize() const {
    std::vector<uint8_t> out;
    out.reserve(70);

    auto append = [&out](const void* ptr, size_t size) {
        const uint8_t* p = reinterpret_cast<const uint8_t*>(ptr);
        out.insert(out.end(), p, p + size);
    };

    uint32_t magic = kSerializationMagic;
    uint16_t ver = kSerializationVersion;
    uint64_t turns = turn_count_.load(std::memory_order_acquire);

    append(&magic, sizeof(magic));
    append(&ver, sizeof(ver));
    append(capability_vector_.data(), sizeof(float) * kCapabilityDims);
    append(&energy_level_, sizeof(energy_level_));
    append(&recent_success_rate_, sizeof(recent_success_rate_));
    append(&turns, sizeof(turns));
    append(&identity_stability_, sizeof(identity_stability_));

    return out;
}

bool SelfModel::deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < 70) return false;

    uint32_t magic = 0;
    uint16_t ver = 0;
    std::memcpy(&magic, data.data(), sizeof(magic));
    std::memcpy(&ver, data.data() + 4, sizeof(ver));

    if (magic != kSerializationMagic || ver != kSerializationVersion) return false;

    size_t offset = 6;
    std::memcpy(capability_vector_.data(), data.data() + offset, sizeof(float) * kCapabilityDims);
    offset += sizeof(float) * kCapabilityDims;

    std::memcpy(&energy_level_, data.data() + offset, sizeof(energy_level_));
    offset += sizeof(energy_level_);

    std::memcpy(&recent_success_rate_, data.data() + offset, sizeof(recent_success_rate_));
    offset += sizeof(recent_success_rate_);

    uint64_t turns = 0;
    std::memcpy(&turns, data.data() + offset, sizeof(turns));
    turn_count_.store(turns, std::memory_order_release);
    offset += sizeof(turns);

    std::memcpy(&identity_stability_, data.data() + offset, sizeof(identity_stability_));

    return true;
}

void SelfModel::loadFromCMF(yuki::memory::CognitiveMemoryFabric* /*cmf*/) {
    // Advisory hook — CMF integration ready for M10 persistence
}

} // namespace yuki::self
