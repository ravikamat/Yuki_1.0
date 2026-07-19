// SensorCalibrationProfile.cpp
// Yuki_1.0 — Signal Conditioning Layer

#include "SensorCalibrationProfile.h"
#include "brain/database/DatabaseManager.h"
#include "../../vendor/sqlite/sqlite3.h"
#include <cmath>
#include <sstream>
#include <iostream>
#include <map>

namespace yuki::conditioning {

static bool db_execute(sqlite3* db, const std::string& sql) {
    if (!db) return false;
    char* errMsg = nullptr;
    int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        if (errMsg) {
            std::cerr << "[SCL DB ERROR] " << errMsg << "\n";
            sqlite3_free(errMsg);
        }
        return false;
    }
    return true;
}

static std::vector<std::map<std::string, std::string>> db_query(sqlite3* db, const std::string& sql) {
    std::vector<std::map<std::string, std::string>> results;
    if (!db) return results;
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        int cols = sqlite3_column_count(stmt);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            std::map<std::string, std::string> row;
            for (int i = 0; i < cols; ++i) {
                const char* name = sqlite3_column_name(stmt, i);
                const unsigned char* val = sqlite3_column_text(stmt, i);
                std::string colName = name ? name : "";
                std::string colVal = val ? reinterpret_cast<const char*>(val) : "";
                row[colName] = colVal;
            }
            results.push_back(row);
        }
        sqlite3_finalize(stmt);
    } else {
        std::cerr << "[SCL DB ERROR] Prepare failed: " << sqlite3_errmsg(db) << "\n";
    }
    return results;
}

// ── CalibrationCurve ─────────────────────────────────────────────────────────

double CalibrationCurve::apply(double raw) const {
    double v = (raw - offset) * gain;
    if (v < floor) return floor;
    if (v > ceil) return ceil;
    return v;
}

double CalibrationCurve::invert(double normalized) const {
    return (normalized / gain) + offset;
}

// ── SensorCalibrationProfile ─────────────────────────────────────────────────

void SensorCalibrationProfile::updateBaseline(double normalized_value) {
    recent_values.push_back(normalized_value);
    if (recent_values.size() > MAX_HISTORY) {
        recent_values.erase(recent_values.begin());
    }

    // Online mean/variance update (Welford's algorithm)
    baseline_samples++;
    double delta = normalized_value - baseline_mean;
    baseline_mean += delta / baseline_samples;
    double delta2 = normalized_value - baseline_mean;
    if (baseline_samples > 1) {
        double var = ((baseline_samples - 2) * baseline_std * baseline_std + delta * delta2)
                     / (baseline_samples - 1);
        baseline_std = std::sqrt(std::max(0.001, var));
    }
}

double SensorCalibrationProfile::zScore(double normalized_value) const {
    if (baseline_std < 1e-6) return 0.0;
    return (normalized_value - baseline_mean) / baseline_std;
}

// ── CalibrationStore ─────────────────────────────────────────────────────────

CalibrationStore& CalibrationStore::instance() {
    static CalibrationStore inst;
    return inst;
}

bool CalibrationStore::init() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (tableExists_) return true;

    auto& db = DatabaseManager::instance();
    const char* sql = R"SQL(
        CREATE TABLE IF NOT EXISTS sensor_calibration (
            sensor_id TEXT PRIMARY KEY,
            sensor_type TEXT NOT NULL,
            hardware_signature TEXT,
            created_at INTEGER,
            last_calibrated_at INTEGER,
            curve_offset REAL DEFAULT 0.0,
            curve_gain REAL DEFAULT 1.0,
            curve_floor REAL DEFAULT 0.0,
            curve_ceil REAL DEFAULT 1.0,
            baseline_mean REAL DEFAULT 0.0,
            baseline_std REAL DEFAULT 1.0,
            baseline_samples INTEGER DEFAULT 0,
            change_threshold REAL DEFAULT 0.05,
            artifact_threshold REAL DEFAULT 0.30
        );
    )SQL";

    tableExists_ = db_execute(db.rawHandle(), sql);
    return tableExists_;
}

SensorCalibrationProfile CalibrationStore::load(const std::string& sensor_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    SensorCalibrationProfile prof;

    auto& db = DatabaseManager::instance();
    std::stringstream sql;
    sql << "SELECT * FROM sensor_calibration WHERE sensor_id = '" << sensor_id << "';";

    auto rows = db_query(db.rawHandle(), sql.str());
    if (rows.empty()) return prof; // Empty = not found

    const auto& r = rows[0];
    prof.sensor_id = sensor_id;
    if (r.count("sensor_type")) prof.sensor_type = r.at("sensor_type");
    if (r.count("hardware_signature")) prof.hardware_signature = r.at("hardware_signature");
    if (r.count("curve_offset")) prof.curve.offset = std::stod(r.at("curve_offset"));
    if (r.count("curve_gain")) prof.curve.gain = std::stod(r.at("curve_gain"));
    if (r.count("curve_floor")) prof.curve.floor = std::stod(r.at("curve_floor"));
    if (r.count("curve_ceil")) prof.curve.ceil = std::stod(r.at("curve_ceil"));
    if (r.count("baseline_mean")) prof.baseline_mean = std::stod(r.at("baseline_mean"));
    if (r.count("baseline_std")) prof.baseline_std = std::stod(r.at("baseline_std"));
    if (r.count("baseline_samples")) prof.baseline_samples = std::stoi(r.at("baseline_samples"));
    if (r.count("change_threshold")) prof.change_threshold = std::stod(r.at("change_threshold"));
    if (r.count("artifact_threshold")) prof.artifact_threshold = std::stod(r.at("artifact_threshold"));

    return prof;
}

void CalibrationStore::save(const SensorCalibrationProfile& profile) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto& db = DatabaseManager::instance();

    std::stringstream sql;
    sql << "INSERT OR REPLACE INTO sensor_calibration ("
        << "sensor_id, sensor_type, hardware_signature, created_at, last_calibrated_at, "
        << "curve_offset, curve_gain, curve_floor, curve_ceil, "
        << "baseline_mean, baseline_std, baseline_samples, "
        << "change_threshold, artifact_threshold) VALUES ("
        << "'" << profile.sensor_id << "', "
        << "'" << profile.sensor_type << "', "
        << "'" << profile.hardware_signature << "', "
        << "strftime('%s','now'), strftime('%s','now'), "
        << profile.curve.offset << ", " << profile.curve.gain << ", "
        << profile.curve.floor << ", " << profile.curve.ceil << ", "
        << profile.baseline_mean << ", " << profile.baseline_std << ", "
        << profile.baseline_samples << ", "
        << profile.change_threshold << ", " << profile.artifact_threshold << ");";

    db_execute(db.rawHandle(), sql.str());
}

void CalibrationStore::remove(const std::string& sensor_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto& db = DatabaseManager::instance();
    std::stringstream sql;
    sql << "DELETE FROM sensor_calibration WHERE sensor_id = '" << sensor_id << "';";
    db_execute(db.rawHandle(), sql.str());
}

std::vector<std::string> CalibrationStore::listSensors() const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto& db = DatabaseManager::instance();
    auto rows = db_query(db.rawHandle(), "SELECT sensor_id FROM sensor_calibration;");
    std::vector<std::string> result;
    for (const auto& r : rows) {
        if (r.count("sensor_id")) result.push_back(r.at("sensor_id"));
    }
    return result;
}

SensorCalibrationProfile CalibrationStore::makeDefault(const std::string& sensor_id,
                                                        const std::string& sensor_type) {
    SensorCalibrationProfile prof;
    prof.sensor_id = sensor_id;
    prof.sensor_type = sensor_type;
    prof.created_at = std::chrono::system_clock::now();
    prof.last_calibrated_at = prof.created_at;

    // Type-specific defaults
    if (sensor_type == "audio") {
        prof.curve.offset = 0.0;
        prof.curve.gain = 1.0;
        prof.change_threshold = 0.08;
        prof.artifact_threshold = 0.40;
    } else if (sensor_type == "camera") {
        prof.curve.offset = 0.0;
        prof.curve.gain = 1.0;
        prof.change_threshold = 0.12;
        prof.artifact_threshold = 0.50;
    } else if (sensor_type == "screen") {
        prof.curve.offset = 0.0;
        prof.curve.gain = 1.0;
        prof.change_threshold = 0.10;
        prof.artifact_threshold = 0.35;
    } else if (sensor_type == "body") {
        prof.curve.offset = 0.0;
        prof.curve.gain = 1.0;
        prof.change_threshold = 0.15;
        prof.artifact_threshold = 0.30;
    }

    return prof;
}

} // namespace yuki::conditioning
