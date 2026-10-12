#include "brain/self/IdentityCore.h"
#include "brain/core/Logger.h"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <sstream>
#include <random>
#include <nlohmann/json.hpp>
#include "brain/core/RandomEngine.h"

namespace yuki {
namespace brain {
namespace self {



const std::array<std::pair<CoreValue, std::pair<std::string, float>>, NUM_CORE_VALUES> 
IdentityCore::DEFAULT_CORE_VALUES = {{
    {CoreValue::SAFETY,         {"safety", 1.0f}},
    {CoreValue::TRUTH,          {"truth", 1.0f}},
    {CoreValue::AUTONOMY,       {"autonomy", 0.9f}},
    {CoreValue::HELPFULNESS,    {"helpfulness", 0.9f}},
    {CoreValue::COMPETENCE,     {"competence", 0.8f}},
    {CoreValue::CURIOSITY,      {"curiosity", 0.8f}},
    {CoreValue::INTEGRITY,      {"integrity", 0.9f}},
    {CoreValue::EMPATHY,        {"empathy", 0.7f}},
    {CoreValue::CREATIVITY,     {"creativity", 0.7f}},
    {CoreValue::SUSTAINABILITY, {"sustainability", 0.7f}},
}};

IdentityCore::IdentityCore(const IdentityConfig& cfg)
    : config_(cfg) {
    identity_vector_.vector.fill(0.0f);
    yuki::core::Logger::instance().log(yuki::core::LogLevel::INFO, "IdentityCore", 
        "Initialized identity core");
}

void IdentityCore::initializeDefaults() {
    // Initialize core values
    for (const auto& [cv, pair] : DEFAULT_CORE_VALUES) {
        Value v;
        v.name = pair.first;
        v.type = cv;
        v.weight = pair.second;
        v.activation = 0.5f;
        v.certainty = 0.9f;  // High certainty for innate values
        v.vtype = ValueType::CORE;
        v.origin = "innate";
        v.acquired = std::chrono::steady_clock::now();
        v.supporting_experiences = 0;
        v.conflicting_experiences = 0;
        values_[pair.first] = std::move(v);
    }
    
    // Initialize identity vector with small random values around core value positions
    std::uniform_real_distribution<float> dist(-0.1f, 0.1f);
    for (int i = 0; i < config_.identity_dim; ++i) {
        identity_vector_.vector[i] = dist(yuki::core::getRandomEngine());
    }
    
    // Set core value dimensions strongly
    identity_vector_.vector[0] = 0.8f;   // safety
    identity_vector_.vector[1] = 0.8f;   // truth
    identity_vector_.vector[2] = 0.7f;   // autonomy
    identity_vector_.vector[3] = 0.7f;   // helpfulness
    identity_vector_.vector[4] = 0.6f;   // competence
    identity_vector_.vector[5] = 0.6f;   // curiosity
    identity_vector_.vector[6] = 0.8f;   // integrity
    identity_vector_.vector[7] = 0.5f;   // empathy
    identity_vector_.vector[8] = 0.5f;   // creativity
    identity_vector_.vector[9] = 0.5f;   // sustainability
    
    updateIdentityVector();
    
    yuki::core::Logger::instance().log(yuki::core::LogLevel::INFO, "IdentityCore",
        "Initialized with " + std::to_string(values_.size()) + " core values");
}

void IdentityCore::recordExperience(const std::string& context, bool aligned_with_values,
                                     const std::vector<std::string>& values_involved) {
    for (const auto& vname : values_involved) {
        auto it = values_.find(vname);
        if (it != values_.end()) {
            if (aligned_with_values) {
                it->second.supporting_experiences++;
                it->second.activation = std::min(1.0f, it->second.activation + 0.02f);
                it->second.certainty = std::min(1.0f, it->second.certainty + 0.01f);
            } else {
                it->second.conflicting_experiences++;
                it->second.activation = std::max(0.0f, it->second.activation - 0.01f);
            }
        }
    }
    
    updateIdentityVector();
}

void IdentityCore::learnValueFromUser(const std::string& value_name, const std::string& description,
                                       CoreValue type, float initial_weight) {
    if (values_.find(value_name) != values_.end()) {
        // Update existing
        auto& v = values_[value_name];
        v.weight = std::max(v.weight, initial_weight);
        v.certainty = std::max(v.certainty, 0.5f);
        v.origin = "user_taught";
        return;
    }
    
    Value v;
    v.name = value_name;
    v.type = type;
    v.weight = initial_weight;
    v.activation = 0.3f;
    v.certainty = 0.5f;
    v.vtype = (type >= CoreValue::SAFETY && type <= CoreValue::SUSTAINABILITY) ? ValueType::CORE : ValueType::INSTRUMENTAL;
    v.origin = "user_taught";
    v.acquired = std::chrono::steady_clock::now();
    v.supporting_experiences = 1;
    v.conflicting_experiences = 0;
    
    values_[value_name] = std::move(v);
    updateIdentityVector();
    
    yuki::core::Logger::instance().log(yuki::core::LogLevel::INFO, "IdentityCore",
        "Learned new value from user: " + value_name);
}

void IdentityCore::updateValue(const std::string& value_name, bool positive_outcome,
                                const std::string& context) {
    auto it = values_.find(value_name);
    if (it == values_.end()) return;
    
    if (positive_outcome) {
        it->second.supporting_experiences++;
        it->second.activation = std::min(1.0f, it->second.activation + 0.02f);
        it->second.certainty = std::min(1.0f, it->second.certainty + 0.005f);
        it->second.weight = std::min(1.0f, it->second.weight + 0.01f);
    } else {
        it->second.conflicting_experiences++;
        it->second.activation = std::max(0.0f, it->second.activation - 0.01f);
        it->second.certainty = std::max(0.1f, it->second.certainty - 0.005f);
    }
    
    updateIdentityVector();
}

float IdentityCore::evaluateAction(const std::string& action, const std::vector<std::string>& context_values) const {
    float total_score = 0.0f;
    float total_weight = 0.0f;
    
    // Check against all active values
    std::vector<std::string> check_values = context_values;
    if (check_values.empty()) {
        for (const auto& [name, v] : values_) {
            if (v.activation > 0.3f) check_values.push_back(name);
        }
    }
    
    for (const auto& vname : check_values) {
        auto it = values_.find(vname);
        if (it == values_.end()) continue;
        
        float alignment = computeValueAlignment(action, vname);
        total_score += alignment * it->second.weight * it->second.activation;
        total_weight += it->second.weight * it->second.activation;
    }
    
    return total_weight > 0 ? total_score / total_weight : 0.5f;
}

float IdentityCore::computeValueAlignment(const std::string& action, const std::string& value_name) const {
    // Simplified: keyword matching
    std::string lower = action;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    
    static const std::unordered_map<std::string, std::vector<std::string>> VALUE_KEYWORDS = {
        {"safety", {"safe", "secure", "protect", "avoid harm", "careful", "caution"}},
        {"truth", {"truth", "accurate", "honest", "fact", "verify", "evidence"}},
        {"autonomy", {"choose", "decide", "control", "independent", "self-determine"}},
        {"helpfulness", {"help", "assist", "support", "benefit", "serve", "aid"}},
        {"competence", {"learn", "improve", "master", "practice", "skill", "expert"}},
        {"curiosity", {"explore", "discover", "investigate", "question", "wonder"}},
        {"integrity", {"honest", "consistent", "principled", "ethical", "promise"}},
        {"empathy", {"understand", "feel", "perspective", "compassion", "listen"}},
        {"creativity", {"create", "innovate", "imagine", "novel", "original", "design"}},
        {"sustainability", {"sustainable", "long-term", "future", "maintain", "enduring"}},
    };
    
    auto it = VALUE_KEYWORDS.find(value_name);
    if (it == VALUE_KEYWORDS.end()) return 0.5f;
    
    float matches = 0;
    for (const auto& kw : it->second) {
        if (lower.find(kw) != std::string::npos) matches++;
    }
    
    return std::min(1.0f, matches * 0.25f + 0.25f);  // Base 0.25 + 0.25 per match
}

void IdentityCore::updateIdentityVector() {
    // Map values to vector dimensions
    static const std::unordered_map<std::string, int> VALUE_TO_DIM = {
        {"safety", 0}, {"truth", 1}, {"autonomy", 2}, {"helpfulness", 3},
        {"competence", 4}, {"curiosity", 5}, {"integrity", 6}, {"empathy", 7},
        {"creativity", 8}, {"sustainability", 9}
    };
    
    // Update core dimensions
    for (const auto& [vname, v] : values_) {
        auto it = VALUE_TO_DIM.find(vname);
        if (it != VALUE_TO_DIM.end()) {
            float target = v.activation * v.certainty * v.weight;
            identity_vector_.vector[it->second] = 
                (1.0f - config_.value_stability) * identity_vector_.vector[it->second] 
                + config_.value_stability * target;
        }
    }
}

const Value* IdentityCore::getValue(const std::string& name) const {
    auto it = values_.find(name);
    return it != values_.end() ? &it->second : nullptr;
}

const Value* IdentityCore::getCoreValue(CoreValue cv) const {
    for (const auto& [name, v] : values_) {
        if (v.type == cv) return &v;
    }
    return nullptr;
}

std::vector<Value> IdentityCore::getActiveValues(float min_activation) const {
    std::vector<Value> active;
    for (const auto& [name, v] : values_) {
        if (v.activation >= min_activation) {
            active.push_back(v);
        }
    }
    std::sort(active.begin(), active.end(),
        [](const Value& a, const Value& b) { return a.activation > b.activation; });
    return active;
}

std::vector<Value> IdentityCore::getCoreValues() const {
    std::vector<Value> core;
    for (const auto& [name, v] : values_) {
        if (v.vtype == ValueType::CORE) {
            core.push_back(v);
        }
    }
    std::sort(core.begin(), core.end(),
        [](const Value& a, const Value& b) { return a.weight > b.weight; });
    return core;
}

float IdentityCore::computeCoherence() const {
    // Coherence = consistency of identity vector
    // High coherence = vector is stable, aligned with values
    float sum_sq = 0.0f;
    for (float v : identity_vector_.vector) {
        sum_sq += v * v;
    }
    float norm = std::sqrt(sum_sq);
    
    // Coherence = how aligned vector is with value weights
    float alignment = 0.0f;
    int count = 0;
    static const std::unordered_map<std::string, int> VALUE_TO_DIM = {
        {"safety", 0}, {"truth", 1}, {"autonomy", 2}, {"helpfulness", 3},
        {"competence", 4}, {"curiosity", 5}, {"integrity", 6}, {"empathy", 7},
        {"creativity", 8}, {"sustainability", 9}
    };
    
    for (const auto& [vname, v] : values_) {
        auto it = VALUE_TO_DIM.find(vname);
        if (it != VALUE_TO_DIM.end()) {
            alignment += identity_vector_.vector[it->second] * v.weight * v.activation;
            count++;
        }
    }
    
    return count > 0 ? alignment / count : 0.5f;
}

float IdentityCore::computeValueConsistency() const {
    // Consistency = low conflict among values
    int total_supporting = 0, total_conflicting = 0;
    for (const auto& [name, v] : values_) {
        total_supporting += v.supporting_experiences;
        total_conflicting += v.conflicting_experiences;
    }
    
    if (total_supporting + total_conflicting == 0) return 1.0f;
    return static_cast<float>(total_supporting) / (total_supporting + total_conflicting);
}

std::string IdentityCore::describeIdentity() const {
    std::ostringstream oss;
    oss << "Identity Profile:\n";
    oss << "  Coherence: " << (computeCoherence() * 100) << "%\n";
    oss << "  Value Consistency: " << (computeValueConsistency() * 100) << "%\n";
    oss << "  Active Values: " << getActiveValues(0.3f).size() << "\n";
    
    // Top values
    auto active = getActiveValues(0.3f);
    oss << "  Top Values:\n";
    for (size_t i = 0; i < std::min<size_t>(5, active.size()); ++i) {
        oss << "    - " << active[i].name << ": " 
            << "weight=" << active[i].weight 
            << ", act=" << active[i].activation 
            << ", cert=" << active[i].certainty << "\n";
    }
    
    // Vector summary
    oss << "  Identity Vector (top 10 dims):\n";
    for (int i = 0; i < 10; ++i) {
        if (std::abs(identity_vector_.vector[i]) > 0.1f) {
            oss << "    [" << i << "] " << IdentityVector::DIM_LABELS[i] << " = " 
                << identity_vector_.vector[i] << "\n";
        }
    }
    
    return oss.str();
}

std::string IdentityCore::describeValues() const {
    std::ostringstream oss;
    oss << "Value System (" << values_.size() << " values):\n";
    
    for (const auto& [name, v] : values_) {
        oss << "  " << name << ":\n";
        oss << "    Type: " << (v.vtype == ValueType::CORE ? "CORE" : 
                               v.vtype == ValueType::INSTRUMENTAL ? "INSTRUMENTAL" : "CONTEXTUAL") << "\n";
        oss << "    Weight: " << v.weight << ", Activation: " << v.activation 
            << ", Certainty: " << v.certainty << "\n";
        oss << "    Origin: " << v.origin << ", Supporting: " << v.supporting_experiences
            << ", Conflicting: " << v.conflicting_experiences << "\n";
    }
    return oss.str();
}

void IdentityCore::applyDecay(float dt_days) {
    // Decay activations toward baseline
    for (auto& [name, v] : values_) {
        if (v.vtype != ValueType::CORE) {
            v.activation *= std::pow(config_.value_stability, dt_days);
        }
    }
    updateIdentityVector();
}

std::string IdentityCore::serialize() const {
    nlohmann::json j;
    j["identity_vector"] = identity_vector_.vector;
    
    for (const auto& [name, v] : values_) {
        nlohmann::json vj;
        vj["name"] = v.name;
        vj["type"] = static_cast<int>(v.type);
        vj["weight"] = v.weight;
        vj["activation"] = v.activation;
        vj["certainty"] = v.certainty;
        vj["vtype"] = static_cast<int>(v.vtype);
        vj["origin"] = v.origin;
        vj["acquired"] = std::chrono::duration_cast<std::chrono::milliseconds>(v.acquired.time_since_epoch()).count();
        vj["supporting_experiences"] = v.supporting_experiences;
        vj["conflicting_experiences"] = v.conflicting_experiences;
        j["values"][name] = vj;
    }
    
    return j.dump();
}

bool IdentityCore::deserialize(const std::string& data) {
    try {
        auto j = nlohmann::json::parse(data);
        
        if (j.contains("identity_vector")) {
            identity_vector_.vector = j["identity_vector"].get<std::array<float, 64>>();
        }
        
        values_.clear();
        if (j.contains("values")) {
            for (auto& [name, vj] : j["values"].items()) {
                Value v;
                v.name = name;
                v.type = static_cast<CoreValue>(vj.value("type", 0));
                v.weight = vj.value("weight", 1.0f);
                v.activation = vj.value("activation", 0.5f);
                v.certainty = vj.value("certainty", 0.5f);
                v.vtype = static_cast<ValueType>(vj.value("vtype", 0));
                v.origin = vj.value("origin", "unknown");
                v.acquired = std::chrono::steady_clock::time_point(
                    std::chrono::milliseconds(vj.value("acquired", 0)));
                v.supporting_experiences = vj.value("supporting_experiences", 0);
                v.conflicting_experiences = vj.value("conflicting_experiences", 0);
                values_[name] = std::move(v);
            }
        }
        
        return true;
    } catch (const std::exception& e) {
        yuki::core::Logger::instance().log(yuki::core::LogLevel::ERROR, "IdentityCore",
            "Deserialize failed: " + std::string(e.what()));
        return false;
    }
}

} // namespace self
} // namespace brain
} // namespace yuki