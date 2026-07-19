#pragma once
// SensorCalibrationProfile.h
// Yuki_1.0 — Signal Conditioning Layer
//
// Per-sensor calibration curves and baselines. Persisted to SQLite via
// DatabaseManager (no new dependencies).

#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <mutex>

namespace yuki::conditioning {

// ── CalibrationCurve ────────────────────────────────────────────────────────
// A piecewise linear mapping from raw sensor units to normalized 0.0–1.0.
// For most sensors, this is just (raw - offset) * gain.
struct CalibrationCurve {
    double offset = 0.0;        // DC offset / ambient baseline
    double gain = 1.0;          // Multiplier to reach 0.0–1.0
    double floor = 0.0;         // Clamped output minimum
    double ceil = 1.0;          // Clamped output maximum

    double apply(double raw) const;
    double invert(double normalized) const; // For debugging
};

// ── SensorCalibrationProfile ────────────────────────────────────────────────
struct SensorCalibrationProfile {
    std::string sensor_id;              // "EAR_DEFAULT", "CAMERA_0", etc.
    std::string sensor_type;            // "audio", "camera", "screen", "body"
    std::string hardware_signature;     // Device name or hash for detection

    // When was this profile created/updated
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point last_calibrated_at;

    // Milliseconds-since-epoch timestamp of last successful calibration run.
    // Persisted alongside the profile so PrecisionEngine can compute sensor age.
    uint64_t last_calibration_timestamp_ms_ = 0;

    void recordCalibration(uint64_t now_ms) {
        last_calibration_timestamp_ms_ = now_ms;
        last_calibrated_at = std::chrono::system_clock::now();
    }
    uint64_t getLastCalibrationMs() const { return last_calibration_timestamp_ms_; }

    // Calibration state
    CalibrationCurve curve;

    // Baselines (rolling averages for z-score computation)
    double baseline_mean = 0.0;
    double baseline_std = 1.0;          // Prevent div-by-zero
    int baseline_samples = 0;

    // Adaptive thresholds (updated by ChangeDetector feedback)
    double change_threshold = 0.05;     // Default 5% deviation triggers forward
    double artifact_threshold = 0.30;   // Spikes >30% delta are rejected

    // History for online baseline updates
    static constexpr int MAX_HISTORY = 1000;
    std::vector<double> recent_values;  // Ring buffer (not persisted, runtime only)

    void updateBaseline(double normalized_value);
    double zScore(double normalized_value) const;
};

// ── CalibrationStore ────────────────────────────────────────────────────────
// SQLite-backed persistence. Wraps DatabaseManager.
class CalibrationStore {
public:
    static CalibrationStore& instance();

    // Initialize the calibration table in SQLite
    bool init();

    // CRUD
    SensorCalibrationProfile load(const std::string& sensor_id) const;
    void save(const SensorCalibrationProfile& profile);
    void remove(const std::string& sensor_id);
    std::vector<std::string> listSensors() const;

    // Factory: create default profile for a sensor type
    static SensorCalibrationProfile makeDefault(const std::string& sensor_id,
                                                 const std::string& sensor_type);

private:
    CalibrationStore() = default;
    bool tableExists_ = false;
    mutable std::mutex mutex_;
};

} // namespace yuki::conditioning
