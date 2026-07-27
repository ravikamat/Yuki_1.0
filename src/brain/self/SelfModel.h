#pragma once
#include <cstdint>
#include <vector>
#include <array>
#include <atomic>
#include <string>
#include <memory>

namespace yuki {
namespace metacognition { class MetacognitionEngine; }
namespace organism { class MetabolismEngine; class ConfidenceCalibrator; }
namespace emotion { class ValenceArousalModel; }
namespace memory { class CognitiveMemoryFabric; }
}

namespace yuki {
namespace selfmodel {

struct CompetenceGap {
    uint32_t domain = 0;
    float self_assessed_competence = 0.0f;
    float measured_competence = 0.0f;
    float gap = 0.0f;
    uint32_t persistence_turns = 0;
    float severity = 0.0f;
};

class SelfModelDelta {
public:
    SelfModelDelta() = default;

    std::vector<CompetenceGap> computeGaps(
        const std::vector<float>& self_assessed,
        const std::vector<float>& measured) const;

    std::vector<uint32_t> detectDecline(
        const std::vector<std::vector<float>>& competence_history,
        float decline_threshold = 0.05f) const;

    std::vector<uint32_t> detectInstability(
        const std::vector<std::vector<float>>& competence_history,
        float variance_threshold = 0.1f) const;

private:
    static constexpr uint32_t DOMAIN_COUNT = 11;
    static constexpr float OVERCONFIDENCE_PENALTY = 2.0f;
};

} // namespace selfmodel

namespace self {

class SelfModel;
class TheoryOfMind;

struct IdentitySnapshot {
    uint64_t id = 0;
    uint64_t timestamp = 0;
    std::string version;
    std::vector<uint8_t> selfModelBlob;
    std::vector<uint8_t> theoryOfMindBlob;
    std::vector<uint8_t> valenceArousalBlob;
    std::vector<uint8_t> confidenceCalibratorBlob;
    uint64_t previousHash = 0;
    uint64_t currentHash = 0;
    double identityDrift = 0.0;
};

struct IdentityTimelineEntry {
    uint64_t snapshotId = 0;
    uint64_t timestamp = 0;
    std::string metricName;
    double metricValue = 0.0;
};

struct AutobiographicalEntry {
    uint64_t id = 0;
    uint64_t timestamp = 0;
    std::string entryType;
    std::vector<uint8_t> contentBlob;
    uint64_t relatedSnapshotId = 0;
};

class IdentityPersistence {
public:
    explicit IdentityPersistence(const std::string& dbPath);
    ~IdentityPersistence();
    IdentityPersistence(const IdentityPersistence&) = delete;
    IdentityPersistence& operator=(const IdentityPersistence&) = delete;
    IdentityPersistence(IdentityPersistence&&) noexcept;
    IdentityPersistence& operator=(IdentityPersistence&&) noexcept;

    bool initializeSchema();

    bool saveIdentity(const SelfModel& self, const TheoryOfMind& tom,
                      const yuki::emotion::ValenceArousalModel& emotion, const yuki::organism::ConfidenceCalibrator& calibrator,
                      const std::string& version = "1.0.0");

    bool loadLatestIdentity(SelfModel& self, TheoryOfMind& tom,
                            yuki::emotion::ValenceArousalModel& emotion, yuki::organism::ConfidenceCalibrator& calibrator);

    bool loadIdentityById(uint64_t snapshotId, SelfModel& self, TheoryOfMind& tom,
                          yuki::emotion::ValenceArousalModel& emotion, yuki::organism::ConfidenceCalibrator& calibrator);

    std::vector<IdentityTimelineEntry> getTimeline(uint64_t startTime, uint64_t endTime);
    std::vector<IdentitySnapshot> getSnapshotHistory(size_t limit = 100);

    bool addAutobiographicalEntry(const std::string& entryType,
                                  const std::vector<uint8_t>& content,
                                  uint64_t relatedSnapshotId = 0);
    std::vector<AutobiographicalEntry> getAutobiographicalEntries(size_t limit = 100);
    std::string generateNarrativeSummary();

    double computeDriftBetween(uint64_t snapshotIdA, uint64_t snapshotIdB);
    double computeLatestDrift();
    bool verifyHashChain();
    uint64_t getLatestHash() const;

    size_t getSnapshotCount() const;
    size_t getEntryCount() const;
    bool pruneSnapshots(size_t keepLast);

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
    uint64_t computeFNV1a(const std::vector<uint8_t>& data, uint64_t previousHash = 0) const;
};

class SelfModel {
public:
    static constexpr size_t kCapabilityDims = 11; // matches MetacognitionEngine domain count
    static constexpr float kIdentityEmaAlpha = 0.05f;
    static constexpr float kSuccessEmaAlpha = 0.1f;
    static constexpr uint32_t kSerializationMagic = 0x534D4F44; // "SMOD"
    static constexpr uint16_t kSerializationVersion = 1;

    SelfModel();

    // Update from turn outcome and subsystem states.
    // drive_activations: [curiosity, competence, social, homeostasis] in [0,1]
    void update(const std::array<float, kCapabilityDims>& competence_vector,
                float metabolism_viability,
                const std::array<float, 4>& drive_activations,
                bool last_turn_success,
                float last_precision);

    // Identity stability: EMA of how much the capability vector changes per update [0,1]
    float identityStability() const;

    // Drift from last checkpoint (0 = identical, 1 = completely different)
    float identityDrift() const;

    // FNV-1a hash of capability vector for quick identity comparison
    uint64_t identityHash() const;

    // Snapshot current state as drift baseline
    void checkpoint();

    // Sleep consolidation — updates baseline and restores energy
    void consolidate();

    // Summary string
    std::string toString() const;

    // Binary serialization
    std::vector<uint8_t> serialize() const;
    bool deserialize(const std::vector<uint8_t>& data);

    // Accessors
    const std::array<float, kCapabilityDims>& capabilityVector() const { return capability_vector_; }
    float energyLevel() const { return energy_level_; }
    float recentSuccessRate() const { return recent_success_rate_; }
    uint64_t turnCount() const { return turn_count_.load(std::memory_order_acquire); }

    void loadFromCMF(yuki::memory::CognitiveMemoryFabric* cmf);

private:
    std::array<float, kCapabilityDims> capability_vector_;
    std::array<float, kCapabilityDims> checkpoint_vector_;
    float energy_level_;
    float recent_success_rate_;
    std::atomic<uint64_t> turn_count_;
    float identity_stability_;

    float vectorDelta(const std::array<float, kCapabilityDims>& a,
                      const std::array<float, kCapabilityDims>& b) const;
    float clamp01(float v) const;
};

} // namespace self
} // namespace yuki
