#pragma once
// EmotionSystem.h — Emotion state + empathy detection + ValenceArousalModel
#include <string>
#include <vector>
#include <atomic>
#include <mutex>
#include <cstdint>
#include <memory>

// Forward declarations
namespace yuki { namespace swarm { class IIdentityStore; } }
namespace yuki { namespace brain { namespace memory { class IdentityHeuristicAccess; } } }

namespace yuki::emotion {

class ValenceArousalModel {
public:
    static constexpr float kDecayRate = 0.95f;
    static constexpr float kValenceMin = -1.0f;
    static constexpr float kValenceMax = 1.0f;
    static constexpr float kArousalMin = 0.0f;
    static constexpr float kArousalMax = 1.0f;
    static constexpr uint32_t kSerializationMagic = 0x56414D4F; // "VAMO"
    static constexpr uint16_t kSerializationVersion = 1;

    /// Default constructor — identical to previous behaviour.
    ValenceArousalModel();

    /// Explicit destructor declared here, defined in EmotionSystem.cpp where IdentityHeuristicAccess is complete.
    ~ValenceArousalModel();

    /// Memory-backed constructor: loads EMA alpha and modulation weights from IIdentityStore.
    explicit ValenceArousalModel(yuki::swarm::IIdentityStore& store);

    // Update from turn outcome signals.
    // outcome_reward: +1 success, -1 failure, 0 neutral
    // user_feedback_signal: +1 approval, -1 rejection, 0 none
    // task_difficulty: [0,1]
    // surprise: [0,1] from FreeEnergy / VSE
    void update(float outcome_reward,
                float user_feedback_signal,
                float task_difficulty,
                float surprise);

    // Modulate a decision threshold.
    float modulateThreshold(float base_threshold) const;

    /// Call after emotional update to persist/adjust learned weights.
    void onEmotionOutcome(bool wasPositive);

    // Accessors
    float valence() const { return valence_.load(std::memory_order_acquire); }
    float arousal() const { return arousal_.load(std::memory_order_acquire); }
    float valenceEma() const { return valence_ema_; }
    float arousalEma() const { return arousal_ema_; }

    // Serialization
    std::vector<uint8_t> serialize() const;
    bool deserialize(const std::vector<uint8_t>& data);

    void reset();
    void decay(); // apply kDecayRate to both dimensions

private:
    std::atomic<float> valence_;
    std::atomic<float> arousal_;
    float valence_ema_;
    float arousal_ema_;

    // Learnable weights (defaults match original constexpr values)
    float emaAlpha_{0.1f};          // was static constexpr kEmaAlpha = 0.1f
    float wOutcomeReward_{0.4f};    // outcome_reward weight in valence update
    float wFeedback_{0.4f};         // user_feedback weight in valence update
    float wArousalFactor_{0.15f};   // modulation factor
    float wValenceFactor_{0.1f};    // valence penalization factor

    std::unique_ptr<yuki::brain::memory::IdentityHeuristicAccess> heuristic_;
    void loadHeuristicValues();

    float clamp(float v, float lo, float hi) const;
};

} // namespace yuki::emotion

// ── §EmotionState ─────────────────────────────────────────────────────────────

struct EmotionSnapshot {
    float valence   = 0.0f;
    float arousal   = 0.0f;
    float curiosity = 0.5f;
    float fatigue   = 0.0f;
    int   turnCount = 0;
};

class EmotionState {
public:
    EmotionState();
    void update(bool gotGoodAnswer, bool userFrustrated,
                bool taskCompleted, bool newTopicLearned, float answerConfidence);
    std::string toneResponse(const std::string& raw) const;
    std::string describeMood() const;
    void save() const;
    void load();
    EmotionSnapshot snapshot() const;
    // GlobalWorkspace integration
    void subscribeToBus();
    void onPerceptionFrame(const std::string& json_payload);
    void extractAndPublish(const std::string& text_json,
                           const std::string& audio_features_json);
private:
    mutable std::mutex mu_;
    EmotionSnapshot    state_;
    void clamp();
};

// ── §EmpathyLayer ─────────────────────────────────────────────────────────────

enum class UserMood {
    UNKNOWN, UNWELL, TIRED, STRESSED, SAD,
    FRUSTRATED, HAPPY, PROUD, NEUTRAL
};

struct EmpathyResult {
    bool        triggered  = false;
    UserMood    mood       = UserMood::UNKNOWN;
    float       intensity  = 0.0f;
    std::string moodLabel;
};

class EmpathyLayer {
public:
    EmpathyLayer() = default;
    EmpathyResult evaluate(const std::string& rawInput, const std::string& userName) const;
    UserMood currentMood()   const { return currentMood_; }
    float    moodIntensity() const { return moodIntensity_; }
    void     resetSession();
private:
    UserMood    detectMood(const std::string& lower, float& intensity) const;
    static std::string toLower(const std::string& s);
    static bool        has(const std::string& h, const std::string& n);
    mutable UserMood currentMood_   = UserMood::UNKNOWN;
    mutable float    moodIntensity_ = 0.0f;
};
