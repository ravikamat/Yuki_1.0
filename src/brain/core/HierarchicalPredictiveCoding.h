#pragma once
// HierarchicalPredictiveCoding — 5-level HPC for continuous cognition
// L0: VSE/Sensory (10ms) - predicts immediate sensory input (24D)
// L1: Intent (50ms) - predicts intent/classification (64D)
// L2: Situational (100ms) - predicts situations/events (128D)
// L3: Conceptual (500ms) - predicts concepts/beliefs (256D)
// L4: Abstract/World-Self (1s+) - predicts abstract world/self state (512D)

#include <array>
#include <vector>
#include <memory>
#include <chrono>
#include <atomic>
#include <mutex>
#include "brain/field/CognitiveSnapshot.h"
#include "brain/inference/GenerativeModel.h"
#include "brain/predictive/LatentWorldModel.h"

namespace yuki {
namespace brain {
namespace core {

struct HPCConfig {
    // Level intervals (increasing timescales)
    std::chrono::milliseconds l0_interval{10};   // VSE/Sensory
    std::chrono::milliseconds l1_interval{50};   // Intent
    std::chrono::milliseconds l2_interval{100};  // Situational
    std::chrono::milliseconds l3_interval{500};  // Conceptual
    std::chrono::milliseconds l4_interval{1000}; // Abstract/World-Self
    
    // Level dimensions (aligned with LatentWorldModel)
    size_t l0_dim = 24;     // VSE observation dim
    size_t l1_dim = 64;     // Intent embedding
    size_t l2_dim = 128;    // Situational embedding
    size_t l3_dim = 256;    // Conceptual embedding
    size_t l4_dim = 512;    // Abstract/World-Self embedding
    
    // Precision (inverse variance) for each level
    float l0_precision = 10.0f;
    float l1_precision = 5.0f;
    float l2_precision = 2.0f;
    float l3_precision = 1.0f;
    float l4_precision = 0.5f;
    
    // Learning rates (decreasing with level)
    float l0_lr = 0.01f;
    float l1_lr = 0.005f;
    float l2_lr = 0.002f;
    float l3_lr = 0.001f;
    float l4_lr = 0.0005f;
    
    // Hierarchical coupling weights
    float top_down_weight = 0.7f;    // L4→L3, L3→L2, L2→L1, L1→L0
    float bottom_up_weight = 0.3f;   // L0→L1, L1→L2, L2→L3, L3→L4
    
    // Cross-level skip connections
    float skip_weight = 0.1f;        // L4→L2, L3→L1, L2→L0 etc.
    
    // Surprise threshold for level escalation
    float surprise_threshold = 2.0f;
    
    // LatentWorldModel integration
    bool use_latent_world_model = true;
    int lwm_imagination_horizon = 15;
    int lwm_imagination_samples = 10;
    
    // Event-driven computation (compute only on surprise)
    bool event_driven = true;
    float min_surprise_for_compute = 0.1f;
};

// Level state
struct HPCLevelState {
    std::vector<float> prediction;       // Top-down prediction
    std::vector<float> prediction_error; // Bottom-up error
    std::vector<float> posterior;        // Combined belief
    std::vector<float> precision;        // Precision (inverse variance)
    float free_energy = 0.0f;
    float surprise = 0.0f;
    std::chrono::steady_clock::time_point last_update;
    bool needs_update = true;  // For event-driven computation
};

struct HPCOutput {
    // Per-level states
    HPCLevelState l0;  // VSE/Sensory
    HPCLevelState l1;  // Intent
    HPCLevelState l2;  // Situational
    HPCLevelState l3;  // Conceptual
    HPCLevelState l4;  // Abstract/World-Self
    
    // Cross-level messages (top-down)
    std::vector<float> l4_to_l3;
    std::vector<float> l3_to_l2;
    std::vector<float> l2_to_l1;
    std::vector<float> l1_to_l0;
    std::vector<float> l4_to_l2;  // Skip
    std::vector<float> l3_to_l1;  // Skip
    std::vector<float> l2_to_l0;  // Skip
    
    // Cross-level messages (bottom-up)
    std::vector<float> l0_to_l1;
    std::vector<float> l1_to_l2;
    std::vector<float> l2_to_l3;
    std::vector<float> l3_to_l4;
    std::vector<float> l0_to_l2;  // Skip
    std::vector<float> l1_to_l3;  // Skip
    std::vector<float> l2_to_l4;  // Skip
    
    // LatentWorldModel integration
    predictive::LatentWorldModelOutput lwm_output;
    bool lwm_active = false;
    
    // Global metrics
    float total_free_energy = 0.0f;
    float total_surprise = 0.0f;
    bool level_escalation = false;
    int highest_active_level = 0;
    
    // Per-level access
    HPCLevelState& operator[](int level) {
        switch (level) {
            case 0: return l0;
            case 1: return l1;
            case 2: return l2;
            case 3: return l3;
            case 4: return l4;
            default: return l0;
        }
    }
    const HPCLevelState& operator[](int level) const {
        switch (level) {
            case 0: return l0;
            case 1: return l1;
            case 2: return l2;
            case 3: return l3;
            case 4: return l4;
            default: return l0;
        }
    }
};

class HierarchicalPredictiveCoding {
public:
    explicit HierarchicalPredictiveCoding(const HPCConfig& cfg = HPCConfig{});
    ~HierarchicalPredictiveCoding() = default;
    
    void setGenerativeModel(brain::inference::GenerativeModel* gm) { generative_model_ = gm; }
    void setLatentWorldModel(predictive::LatentWorldModel* lwm) { latent_world_model_ = lwm; }
    void setConfig(const HPCConfig& cfg) { config_ = cfg; }
    
    // Main step: process observation through 5-level hierarchy
    HPCOutput step(const std::array<float, 24>& observation, 
                   const std::vector<float>& action = {},
                   const std::array<std::vector<float>, 5>* level_observations = nullptr);
    
    // Event-driven step (only compute levels where surprise > threshold)
    HPCOutput stepEventDriven(const std::array<float, 24>& observation,
                              const std::vector<float>& action = {});
    
    // Get current level states
    const HPCLevelState& getL0() const { return l0_; }
    const HPCLevelState& getL1() const { return l1_; }
    const HPCLevelState& getL2() const { return l2_; }
    const HPCLevelState& getL3() const { return l3_; }
    const HPCLevelState& getL4() const { return l4_; }
    const HPCLevelState& getLevel(int level) const;
    HPCLevelState& getLevel(int level);
    
    // LatentWorldModel imagination for planning
    predictive::LatentWorldModel::ImaginationResult imagine(
        const std::vector<std::vector<float>>& policy_actions,
        int horizon = -1) const;
    
    // Reset levels
    void reset();
    
    // Precision re-estimation
    void reestimatePrecision();
    
    // Surprise and free energy accessors
    float getSurprise() const { return output_.total_surprise; }
    float getFreeEnergy() const { return output_.total_free_energy; }
    float getLevelSurprise(int level) const { return getLevel(level).surprise; }
    
    // Set level observations (for LWM integration)
    void setLevelObservations(const std::array<std::vector<float>, 5>& obs) {
        level_observations_ = obs;
        has_level_observations_ = true;
    }

private:
    HPCConfig config_;
    brain::inference::GenerativeModel* generative_model_ = nullptr;
    predictive::LatentWorldModel* latent_world_model_ = nullptr;
    
    // Level states
    HPCLevelState l0_, l1_, l2_, l3_, l4_;
    HPCOutput output_;
    
    // Level observations for LWM
    std::array<std::vector<float>, 5> level_observations_;
    bool has_level_observations_ = false;
    
    // Level-specific processing
    void updateL0(const std::array<float, 24>& observation, const std::vector<float>& action);
    void updateL1();
    void updateL2();
    void updateL3();
    void updateL4();
    
    // Event-driven level update check
    bool shouldUpdateLevel(int level) const;
    
    // Message passing
    void passTopDown();
    void passBottomUp();
    void passSkipConnections();
    
    // Free energy computation
    float computeFreeEnergy(const HPCLevelState& level) const;
    
    // Prediction using generative model / LWM
    std::vector<float> predict(const std::vector<float>& state, 
                               const std::vector<float>& action,
                               int level) const;
    
    // Softmax for normalization
    void softmax(std::vector<float>& vec) const;
    
    // Resize helper
    void resizeVector(std::vector<float>& vec, size_t new_size, float fill = 0.0f);
    
    // Project between dimensions
    std::vector<float> project(const std::vector<float>& src, size_t target_dim) const;
};

} // namespace core
} // namespace brain
} // namespace yuki