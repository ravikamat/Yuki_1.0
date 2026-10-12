#pragma once

#include <string>
#include <vector>
#include <array>
#include <unordered_map>
#include <chrono>

namespace yuki {
namespace brain {
namespace self {

struct IdentityConfig {
    int identity_dim = 64;
    int num_core_values = 10;
    int num_instrumental_values = 20;
    float identity_stability = 0.99f;
    float value_stability = 0.995f;
    float value_learning_rate = 0.01f;
    int min_examples_for_value_change = 50;
};

enum class ValueType : uint8_t {
    CORE = 0,
    INSTRUMENTAL = 1,
    CONTEXTUAL = 2
};

enum class CoreValue : uint8_t {
    SAFETY = 0,
    TRUTH = 1,
    AUTONOMY = 2,
    HELPFULNESS = 3,
    COMPETENCE = 4,
    CURIOSITY = 5,
    INTEGRITY = 6,
    EMPATHY = 7,
    CREATIVITY = 8,
    SUSTAINABILITY = 9
};

static constexpr int NUM_CORE_VALUES = 10;

struct Value {
    std::string name;
    CoreValue type;
    float weight = 1.0f;
    float activation = 0.5f;
    float certainty = 0.5f;
    ValueType vtype = ValueType::CORE;
    std::string origin;
    std::chrono::steady_clock::time_point acquired;
    int supporting_experiences = 0;
    int conflicting_experiences = 0;
};

struct IdentityVector {
    std::array<float, 64> vector;
    static constexpr const char* DIM_LABELS[64] = {
        "safety_orientation", "truth_seeking", "autonomy_drive", "helpfulness",
        "competence_drive", "curiosity_drive", "integrity", "empathy",
        "creativity", "sustainability",
        "analytical_thinking", "intuitive_thinking", "systematic", "flexible",
        "detail_oriented", "big_picture", "risk_averse", "risk_tolerant",
        "social", "independent", "collaborative", "competitive",
        "verbal", "visual", "kinesthetic", "mathematical",
        "planning", "improvising", "perfectionist", "pragmatic",
        "optimistic", "realistic", "skeptical", "trusting",
        "patient", "urgent", "thorough", "efficient",
        "self_reflective", "externally_focused", "internal_locus", "external_locus",
        "novelty_seeking", "routine_preferring", "abstract", "concrete",
        "emotional_regulation", "emotional_expression", "cognitive_control", "impulse_control",
        "goal_directed", "exploratory", "exploitative", "balanced",
        "identity_coherence", "value_consistency", "self_awareness", "meta_cognition"
    };
};

struct IdentitySnapshot {
    IdentityVector vector;
    std::unordered_map<std::string, Value> values;
    std::chrono::steady_clock::time_point timestamp;
    float identity_coherence = 1.0f;
    float value_consistency = 1.0f;
    std::string dominant_value;
    float value_entropy = 0.0f;
};

class IdentityCore {
public:
    explicit IdentityCore(const IdentityConfig& cfg = IdentityConfig{});
    ~IdentityCore() = default;
    
    void setConfig(const IdentityConfig& cfg) { config_ = cfg; }
    void initializeDefaults();
    
    void recordExperience(const std::string& context, bool aligned_with_values, 
                          const std::vector<std::string>& values_involved);
    void learnValueFromUser(const std::string& value_name, const std::string& description,
                            CoreValue type, float initial_weight = 0.5f);
    void updateValue(const std::string& value_name, bool positive_outcome, 
                     const std::string& context = "");
    
    float evaluateAction(const std::string& action, const std::vector<std::string>& context_values = {}) const;
    
    const IdentityVector& getVector() const { return identity_vector_; }
    const Value* getValue(const std::string& name) const;
    const Value* getCoreValue(CoreValue cv) const;
    std::vector<Value> getActiveValues(float min_activation = 0.3f) const;
    std::vector<Value> getCoreValues() const;
    
    float computeCoherence() const;
    float computeValueConsistency() const;
    
    std::string describeIdentity() const;
    std::string describeValues() const;
    
    std::string serialize() const;
    bool deserialize(const std::string& data);

private:
    IdentityConfig config_;
    IdentityVector identity_vector_;
    std::unordered_map<std::string, Value> values_;
    
    void initializeCoreValues();
    void updateIdentityVector();
    float computeValueAlignment(const std::string& action, const std::string& value_name) const;
    void applyDecay(float dt_days);
    
    static const std::array<std::pair<CoreValue, std::pair<std::string, float>>, 10> DEFAULT_CORE_VALUES;
};

} // namespace self
} // namespace brain
} // namespace yuki