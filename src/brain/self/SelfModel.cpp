// SelfModel.cpp — implementation of yuki::self::SelfModel and
// yuki::selfmodel::SelfModelDelta (declared in SelfModel.h). The class was
// previously header-only-declared with no TU; gate_9's TinyOracle odr-uses it.
// Heuristic-memory and YNC integrations are wired through opaque PIMPLs and
// left as inert stubs until those subsystems come back online.

#include "brain/self/SelfModel.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <sstream>
#include <stdexcept>

namespace {
constexpr size_t kCapabilityDims = 11;
} // namespace

// The header forward-declares SelfHeuristicImpl at GLOBAL scope, so the
// definition must live at global scope too (yuki::self::SelfHeuristicImpl
// would be a distinct type and never complete the declaration).
struct SelfHeuristicImpl {
    // Filled when HeuristicMemory integration returns.
};

namespace yuki {
namespace self {

// ── Lifetime ────────────────────────────────────────────────────────────────

SelfModel::SelfModel()
    : capability_vector_{},
      checkpoint_vector_{},
      synaptic_density_(0.0f),
      firing_rate_(0.0f),
      plasticity_rate_(0.0f),
      competence_history_(),
      energy_level_(1.0f),
      recent_success_rate_(0.5f),
      turn_count_(0),
      identity_stability_(1.0f),
      identityEmaAlpha_(kDefaultIdentityEmaAlpha),
      successEmaAlpha_(kDefaultSuccessEmaAlpha),
      smHeuristic_(std::make_unique<SelfHeuristicImpl>()) {
    // Seed capability vector at a neutral 0.5 so identity comparisons are
    // meaningful from the first turn.
    capability_vector_.fill(0.5f);
    checkpoint_vector_ = capability_vector_;
}

SelfModel::~SelfModel() = default;

// ── Configuration ───────────────────────────────────────────────────────────

void SelfModel::configure(yuki::swarm::IIdentityStore& /*store*/) {
    // Dynamic EMA-alpha loading returns when IIdentityStore is linked.
}

// ── Turn updates ────────────────────────────────────────────────────────────

void SelfModel::onSelfModelOutcome(bool lastTurnSucceeded) {
    const float target = lastTurnSucceeded ? 1.0f : 0.0f;
    recent_success_rate_ +=
        successEmaAlpha_ * (target - recent_success_rate_);
    // Success raises the alpha slightly (adapt faster), failure lowers it.
    successEmaAlpha_ = std::clamp(
        successEmaAlpha_ + (lastTurnSucceeded ? 0.005f : -0.005f),
        0.01f, 0.3f);
}

void SelfModel::update(const std::array<float, kCapabilityDims>& competence_vector,
                       float metabolism_viability,
                       const std::array<float, 4>& drive_activations,
                       bool last_turn_success,
                       float last_precision) {
    capability_vector_ = competence_vector;
    competence_history_.push_back(competence_vector);
    if (competence_history_.size() > 64) competence_history_.erase(competence_history_.begin());

    synaptic_density_ = std::clamp(synaptic_density_, 0.0f, 1.0f);
    firing_rate_ = std::clamp(firing_rate_, 0.0f, 1.0f);
    plasticity_rate_ = std::clamp(plasticity_rate_, 0.0f, 1.0f);

    // Energy: mean of drive activations scaled by metabolic viability.
    float drive_mean = 0.0f;
    for (float d : drive_activations) drive_mean += d;
    drive_mean /= 4.0f;
    energy_level_ = std::clamp(0.5f * drive_mean + 0.5f * metabolism_viability, 0.0f, 1.0f);

    // Identity stability: EMA of (1 - capability delta) per update.
    float delta = vectorDelta(capability_vector_, checkpoint_vector_);
    identity_stability_ += identityEmaAlpha_ * ((1.0f - delta) - identity_stability_);
    identity_stability_ = std::clamp(identity_stability_, 0.0f, 1.0f);

    (void)last_precision;
    onSelfModelOutcome(last_turn_success);
    turn_count_.fetch_add(1, std::memory_order_acq_rel);
}

// ── Identity metrics ────────────────────────────────────────────────────────

float SelfModel::identityStability() const { return identity_stability_; }

float SelfModel::identityDrift() const {
    return vectorDelta(capability_vector_, checkpoint_vector_);
}

uint64_t SelfModel::identityHash() const {
    uint64_t hash = 1469598103934665603ull;  // FNV-1a 64 offset basis
    for (float v : capability_vector_) {
        uint32_t bits = 0;
        static_assert(sizeof(bits) == sizeof(v), "float must be 32-bit");
        std::memcpy(&bits, &v, sizeof(bits));
        for (int b = 0; b < 4; ++b) {
            hash ^= static_cast<uint64_t>((bits >> (8 * b)) & 0xFFu);
            hash *= 1099511628211ull;  // FNV-1a 64 prime
        }
    }
    return hash;
}

void SelfModel::checkpoint() { checkpoint_vector_ = capability_vector_; }

void SelfModel::consolidate() {
    checkpoint();
    energy_level_ = std::min(1.0f, energy_level_ + 0.1f);  // sleep restores energy
}

// ── Presentation & persistence ──────────────────────────────────────────────

std::string SelfModel::toString() const {
    std::ostringstream oss;
    oss << "SelfModel(turns=" << turn_count_.load(std::memory_order_acquire)
        << " stability=" << identity_stability_
        << " success=" << recent_success_rate_
        << " energy=" << energy_level_ << ")";
    return oss.str();
}

std::vector<uint8_t> SelfModel::serialize() const {
    std::vector<uint8_t> out;
    out.reserve(4 + 2 + kCapabilityDims * 4 + 5 * 4 + 8);

    auto append_u32 = [&out](uint32_t v) {
        for (int b = 0; b < 4; ++b) out.push_back(static_cast<uint8_t>((v >> (8 * b)) & 0xFFu));
    };
    auto append_u16 = [&out](uint16_t v) {
        out.push_back(static_cast<uint8_t>(v & 0xFFu));
        out.push_back(static_cast<uint8_t>((v >> 8) & 0xFFu));
    };
    auto append_f32 = [&append_u32](float v) {
        uint32_t bits = 0;
        std::memcpy(&bits, &v, sizeof(bits));
        append_u32(bits);
    };

    append_u32(kSerializationMagic);
    append_u16(kSerializationVersion);
    for (float v : capability_vector_) append_f32(v);
    append_f32(synaptic_density_);
    append_f32(firing_rate_);
    append_f32(plasticity_rate_);
    append_f32(energy_level_);
    append_f32(recent_success_rate_);
    append_f32(identity_stability_);
    append_u32(static_cast<uint32_t>(turn_count_.load(std::memory_order_acquire)));
    return out;
}

bool SelfModel::deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < 4 + 2) return false;

    auto read_u32 = [&data](size_t off, uint32_t& v) {
        if (off + 4 > data.size()) return false;
        v = static_cast<uint32_t>(data[off]) |
            (static_cast<uint32_t>(data[off + 1]) << 8) |
            (static_cast<uint32_t>(data[off + 2]) << 16) |
            (static_cast<uint32_t>(data[off + 3]) << 24);
        return true;
    };
    auto read_u16 = [&data](size_t off, uint16_t& v) {
        if (off + 2 > data.size()) return false;
        v = static_cast<uint16_t>(data[off]) | (static_cast<uint16_t>(data[off + 1]) << 8);
        return true;
    };
    auto read_f32 = [&read_u32](size_t off, float& v) {
        uint32_t bits = 0;
        if (!read_u32(off, bits)) return false;
        std::memcpy(&v, &bits, sizeof(v));
        return true;
    };

    uint32_t magic = 0;
    uint16_t version = 0;
    size_t off = 0;
    if (!read_u32(off, magic) || magic != kSerializationMagic) return false;
    off += 4;
    if (!read_u16(off, version) || version != kSerializationVersion) return false;
    off += 2;

    for (float& v : capability_vector_) {
        if (!read_f32(off, v)) return false;
        off += 4;
    }
    if (!read_f32(off, synaptic_density_)) return false; off += 4;
    if (!read_f32(off, firing_rate_)) return false;      off += 4;
    if (!read_f32(off, plasticity_rate_)) return false;  off += 4;
    if (!read_f32(off, energy_level_)) return false;     off += 4;
    if (!read_f32(off, recent_success_rate_)) return false; off += 4;
    if (!read_f32(off, identity_stability_)) return false;  off += 4;

    uint32_t turns = 0;
    if (!read_u32(off, turns)) return false;
    turn_count_.store(turns, std::memory_order_release);

    checkpoint_vector_ = capability_vector_;
    return true;
}

// ── Subsystem integrations (inert until those systems are rebuilt) ─────────

void SelfModel::updateFromYNC(const ync::YncOrchestrator* /*ync*/,
                              ync::YNCTrainingSupervisor* /*trainer*/) {
}

std::vector<float> SelfModel::getLearningTrajectory() const {
    std::vector<float> trajectory;
    trajectory.reserve(competence_history_.size());
    for (const auto& h : competence_history_) {
        float mean = 0.0f;
        for (float v : h) mean += v;
        trajectory.push_back(mean / static_cast<float>(h.size()));
    }
    return trajectory;
}

std::vector<std::string> SelfModel::getGaps(float threshold) const {
    static constexpr const char* kDomainNames[11] = {
        "question_answering",
        "command_execution", 
        "emotional_understanding",
        "technical_reasoning",
        "urgency_detection",
        "greeting_social",
        "action_planning",
        "polarity_judgment",
        "precision_calibration",
        "retrieval_quality",
        "synthesis_quality"
    };
    
    std::vector<std::string> gaps;
    for (size_t i = 0; i < kCapabilityDims; ++i) {
        if (capability_vector_[i] < threshold) {
            gaps.push_back(kDomainNames[i]);
        }
    }
    return gaps;
}

std::vector<SelfModel::MetacognitiveStrategy> SelfModel::generateStrategies(
    const yuki::brain::inference::BeliefState& /*belief*/) const {
    // Full strategy generation lived in the policy engine (moved to not_in_use).
    return {};
}

void SelfModel::loadFromCMF(yuki::brain::memory::CognitiveMemoryFabric* /*cmf*/) {
}

// ── Private helpers ─────────────────────────────────────────────────────────

float SelfModel::vectorDelta(const std::array<float, kCapabilityDims>& a,
                             const std::array<float, kCapabilityDims>& b) const {
    float sum = 0.0f;
    for (size_t i = 0; i < kCapabilityDims; ++i) sum += std::abs(a[i] - b[i]);
    return clamp01(sum / static_cast<float>(kCapabilityDims));
}

float SelfModel::clamp01(float v) const { return std::clamp(v, 0.0f, 1.0f); }

} // namespace self

namespace selfmodel {

// ── SelfModelDelta (declared out-of-line ctors + gap detectors) ────────────

SelfModelDelta::SelfModelDelta()
    : overconfidencePenalty_(kDefaultOverconfidencePenalty),
      heuristic_(std::make_unique<::SelfHeuristicImpl>()) {}

SelfModelDelta::~SelfModelDelta() = default;

std::vector<CompetenceGap> SelfModelDelta::computeGaps(
    const std::vector<float>& self_assessed,
    const std::vector<float>& measured) const {
    std::vector<CompetenceGap> gaps;
    const size_t n = std::min(self_assessed.size(), measured.size());
    gaps.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        CompetenceGap g;
        g.domain = static_cast<uint32_t>(i);
        g.self_assessed_competence = self_assessed[i];
        g.measured_competence = measured[i];
        g.gap = self_assessed[i] - measured[i];
        g.persistence_turns = (g.gap > 0.0f) ? 1u : 0u;
        g.severity = (g.gap > 0.0f) ? g.gap * overconfidencePenalty_ : 0.0f;
        gaps.push_back(g);
    }
    return gaps;
}

std::vector<uint32_t> SelfModelDelta::detectDecline(
    const std::vector<std::vector<float>>& competence_history,
    float decline_threshold) const {
    std::vector<uint32_t> declining;
    if (competence_history.size() < 2) return declining;
    const auto& recent = competence_history.back();
    const auto& previous = competence_history[competence_history.size() - 2];
    const size_t n = std::min(recent.size(), previous.size());
    for (size_t i = 0; i < n; ++i) {
        if (previous[i] - recent[i] > decline_threshold) {
            declining.push_back(static_cast<uint32_t>(i));
        }
    }
    return declining;
}

std::vector<uint32_t> SelfModelDelta::detectInstability(
    const std::vector<std::vector<float>>& competence_history,
    float variance_threshold) const {
    std::vector<uint32_t> unstable;
    if (competence_history.size() < 2) return unstable;
    const size_t dims = competence_history.front().size();
    for (size_t d = 0; d < dims; ++d) {
        float mean = 0.0f;
        for (const auto& h : competence_history) mean += h[d];
        mean /= static_cast<float>(competence_history.size());
        float variance = 0.0f;
        for (const auto& h : competence_history) {
            const float diff = h[d] - mean;
            variance += diff * diff;
        }
        variance /= static_cast<float>(competence_history.size());
        if (variance > variance_threshold) {
            unstable.push_back(static_cast<uint32_t>(d));
        }
    }
    return unstable;
}

} // namespace selfmodel
} // namespace yuki
