#pragma once
#include <cstdint>
#include <vector>
#include <array>
#include <atomic>
#include <string>
#include <memory>
#include "brain/inference/VariationalStateEstimator.h"
#include "brain/inference/BeliefState.h"
// #include "brain/policy/ActiveInferencePolicyEngine.h"  // Moved to not_in_use

// Forward declarations for inference namespace
namespace yuki::brain::inference {
    class BeliefState;
}

// Forward declarations only — full includes are in SelfModel.cpp to break
// the circular include: IIdentityStore.h includes SelfModel.h.
namespace yuki { namespace swarm { class IIdentityStore; } }

// Opaque PIMPL for heuristic memory — defined in SelfModel.cpp.
// Using opaque ptr instead of unique_ptr<IdentityHeuristicAccess> avoids
// including HeuristicMemory.h here (which would cascade the circular include).
struct SelfHeuristicImpl;

namespace yuki {
namespace metacognition { class MetacognitionEngine; }
namespace organism { class MetabolismEngine; class ConfidenceCalibrator; }
namespace emotion { class ValenceArousalModel; }
}
namespace yuki::brain::memory { class CognitiveMemoryFabric; }

namespace ync {
class YncOrchestrator;
class YNCTrainingSupervisor;
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
    SelfModelDelta();
    ~SelfModelDelta(); // out-of-line in .cpp; required for unique_ptr<SelfHeuristicImpl>

    std::vector<CompetenceGap> computeGaps(
        const std::vector<float>& self_assessed,
        const std::vector<float>& measured) const;

    std::vector<uint32_t> detectDecline(
        const std::vector<std::vector<float>>& competence_history,
        float decline_threshold = 0.05f) const;

    std::vector<uint32_t> detectInstability(
        const std::vector<std::vector<float>>& competence_history,
        float variance_threshold = 0.1f) const;

    // First-run seed default
    static constexpr float kDefaultOverconfidencePenalty = 2.0f;
    // Runtime accessor
    float overconfidencePenalty() const { return overconfidencePenalty_; }
    void configure(yuki::swarm::IIdentityStore& store);

private:
    static constexpr uint32_t DOMAIN_COUNT = 11;
    float overconfidencePenalty_{kDefaultOverconfidencePenalty};
    std::unique_ptr<SelfHeuristicImpl> heuristic_;
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

    bool saveSnapshot(const IdentitySnapshot& snap);

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
    // First-run seed defaults
    static constexpr float kDefaultIdentityEmaAlpha = 0.05f;
    static constexpr float kDefaultSuccessEmaAlpha  = 0.1f;
    static constexpr uint32_t kSerializationMagic = 0x534D4F44; // "SMOD"
    static constexpr uint16_t kSerializationVersion = 1;

    SelfModel();
    ~SelfModel();

    /// Bind identity store for dynamic EMA alpha loading.
    void configure(yuki::swarm::IIdentityStore& store);
    /// Called after each turn: adjusts EMA alphas based on outcome.
    void onSelfModelOutcome(bool lastTurnSucceeded);

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

    // ── Task 6.2: YNC Metrics & Self-Model Integration ───────────────────────
    void updateFromYNC(const ync::YncOrchestrator* ync, ync::YNCTrainingSupervisor* trainer = nullptr);
    std::vector<float> getLearningTrajectory() const;

    // Generate metacognitive strategies from current state
    struct MetacognitiveStrategy {
        // yuki::brain::policy::Policy policy;  // Moved to not_in_use
        std::vector<int> actions;
        float efe;
        std::string description;
        float expected_value;
    };
    std::vector<MetacognitiveStrategy> generateStrategies(
        const yuki::brain::inference::BeliefState& belief) const;

    // Accessors
    const std::array<float, kCapabilityDims>& capabilityVector() const { return capability_vector_; }
    float synapticDensity() const { return synaptic_density_; }
    float firingRate() const { return firing_rate_; }
    float plasticityRate() const { return plasticity_rate_; }
    float energyLevel() const { return energy_level_; }
    float recentSuccessRate() const { return recent_success_rate_; }
    uint64_t turnCount() const { return turn_count_.load(std::memory_order_acquire); }

    void loadFromCMF(yuki::brain::memory::CognitiveMemoryFabric* cmf);

private:
    std::array<float, kCapabilityDims> capability_vector_;
    std::array<float, kCapabilityDims> checkpoint_vector_;
    float synaptic_density_{0.0f};
    float firing_rate_{0.0f};
    float plasticity_rate_{0.0f};
    std::vector<std::array<float, kCapabilityDims>> competence_history_;
    float energy_level_;
    float recent_success_rate_;
    std::atomic<uint64_t> turn_count_;
    float identity_stability_;

    // Dynamic EMA alphas — loaded from IIdentityStore, self-adjust per turn
    float identityEmaAlpha_{kDefaultIdentityEmaAlpha};
    float successEmaAlpha_  {kDefaultSuccessEmaAlpha};
    std::unique_ptr<SelfHeuristicImpl> smHeuristic_;

    float vectorDelta(const std::array<float, kCapabilityDims>& a,
                      const std::array<float, kCapabilityDims>& b) const;
    float clamp01(float v) const;
};

} // namespace self
} // namespace yuki
