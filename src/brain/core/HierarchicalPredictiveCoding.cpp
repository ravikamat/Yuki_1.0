#include "brain/core/HierarchicalPredictiveCoding.h"
#include "brain/inference/GenerativeModel.h"
#include "brain/predictive/LatentWorldModel.h"
#include "brain/core/Logger.h"
#include <algorithm>
#include <cmath>
#include <numeric>

namespace yuki {
namespace brain {
namespace core {

HierarchicalPredictiveCoding::HierarchicalPredictiveCoding(const HPCConfig& cfg)
    : config_(cfg) {
    
    // Initialize L0 (VSE/Sensory)
    l0_.prediction.assign(config_.l0_dim, 0.0f);
    l0_.prediction_error.assign(config_.l0_dim, 0.0f);
    l0_.posterior.assign(config_.l0_dim, 0.0f);
    l0_.precision.assign(config_.l0_dim, config_.l0_precision);
    
    // Initialize L1 (Intent)
    l1_.prediction.assign(config_.l1_dim, 0.0f);
    l1_.prediction_error.assign(config_.l1_dim, 0.0f);
    l1_.posterior.assign(config_.l1_dim, 0.0f);
    l1_.precision.assign(config_.l1_dim, config_.l1_precision);
    
    // Initialize L2 (Situational)
    l2_.prediction.assign(config_.l2_dim, 0.0f);
    l2_.prediction_error.assign(config_.l2_dim, 0.0f);
    l2_.posterior.assign(config_.l2_dim, 0.0f);
    l2_.precision.assign(config_.l2_dim, config_.l2_precision);
    
    // Initialize L3 (Conceptual)
    l3_.prediction.assign(config_.l3_dim, 0.0f);
    l3_.prediction_error.assign(config_.l3_dim, 0.0f);
    l3_.posterior.assign(config_.l3_dim, 0.0f);
    l3_.precision.assign(config_.l3_dim, config_.l3_precision);
    
    // Initialize L4 (Abstract/World-Self)
    l4_.prediction.assign(config_.l4_dim, 0.0f);
    l4_.prediction_error.assign(config_.l4_dim, 0.0f);
    l4_.posterior.assign(config_.l4_dim, 0.0f);
    l4_.precision.assign(config_.l4_dim, config_.l4_precision);
    
    // Cross-level messages
    output_.l1_to_l0.resize(config_.l0_dim, 0.0f);
    output_.l2_to_l1.resize(config_.l1_dim, 0.0f);
    output_.l3_to_l2.resize(config_.l2_dim, 0.0f);
    output_.l4_to_l3.resize(config_.l3_dim, 0.0f);
    output_.l2_to_l0.resize(config_.l0_dim, 0.0f);
    output_.l3_to_l1.resize(config_.l1_dim, 0.0f);
    output_.l4_to_l2.resize(config_.l2_dim, 0.0f);
    
    output_.l0_to_l1.resize(config_.l1_dim, 0.0f);
    output_.l1_to_l2.resize(config_.l2_dim, 0.0f);
    output_.l2_to_l3.resize(config_.l3_dim, 0.0f);
    output_.l3_to_l4.resize(config_.l4_dim, 0.0f);
    output_.l0_to_l2.resize(config_.l2_dim, 0.0f);
    output_.l1_to_l3.resize(config_.l3_dim, 0.0f);
    output_.l2_to_l4.resize(config_.l4_dim, 0.0f);
    
    yuki::core::Logger::instance().log(yuki::core::LogLevel::INFO, "HPC", 
        "Initialized 5-level Hierarchical Predictive Coding (L0=24D, L1=64D, L2=128D, L3=256D, L4=512D)");
}

HPCOutput HierarchicalPredictiveCoding::step(const std::array<float, 24>& observation, 
                                             const std::vector<float>& action,
                                             const std::array<std::vector<float>, 5>* level_observations) {
    
    if (level_observations) {
        level_observations_ = *level_observations;
        has_level_observations_ = true;
    }
    
    if (config_.event_driven) {
        return stepEventDriven(observation, action);
    }
    
    // Bottom-up pass: L0 -> L1 -> L2 -> L3 -> L4
    updateL0(observation, action);
    passBottomUp();  // L0 -> L1
    
    updateL1();
    passBottomUp();  // L1 -> L2
    
    updateL2();
    passBottomUp();  // L2 -> L3
    
    updateL3();
    passBottomUp();  // L3 -> L4
    
    updateL4();
    
    // Top-down pass: L4 -> L3 -> L2 -> L1 -> L0
    passTopDown();
    passSkipConnections();
    
    // Recompute with top-down influence
    updateL3();
    updateL2();
    updateL1();
    updateL0(observation, action);
    
    // LatentWorldModel integration
    if (config_.use_latent_world_model && latent_world_model_ && has_level_observations_) {
        output_.lwm_output = latent_world_model_->forward(level_observations_, action, true);
        output_.lwm_active = true;
        
        // Fuse LWM posteriors into HPC levels
        for (int l = 0; l < 5; ++l) {
            const auto& lwm_state = output_.lwm_output.rssm_states[l];
            HPCLevelState& hpc_level = getLevel(l);
            
            // Combine HPC posterior with LWM deterministic state
            size_t dim = hpc_level.posterior.size();
            for (size_t i = 0; i < dim && i < lwm_state.deterministic.size(); ++i) {
                hpc_level.posterior[i] = 0.7f * hpc_level.posterior[i] + 0.3f * lwm_state.deterministic[i];
            }
        }
    }
    
    // Compute global metrics
    output_.total_free_energy = l0_.free_energy + l1_.free_energy + l2_.free_energy + l3_.free_energy + l4_.free_energy;
    output_.total_surprise = l0_.surprise + l1_.surprise + l2_.surprise + l3_.surprise + l4_.surprise;
    output_.level_escalation = output_.total_surprise > config_.surprise_threshold;
    
    // Find highest active level
    output_.highest_active_level = 0;
    for (int l = 4; l >= 0; --l) {
        if (getLevel(l).surprise > config_.min_surprise_for_compute) {
            output_.highest_active_level = l;
            break;
        }
    }
    
    // Copy states to output
    output_.l0 = l0_;
    output_.l1 = l1_;
    output_.l2 = l2_;
    output_.l3 = l3_;
    output_.l4 = l4_;
    
    return output_;
}

HPCOutput HierarchicalPredictiveCoding::stepEventDriven(const std::array<float, 24>& observation,
                                                         const std::vector<float>& action) {
    // Only update levels where surprise exceeds threshold or forced
    bool update_l0 = true;  // Always update sensory
    bool update_l1 = shouldUpdateLevel(1);
    bool update_l2 = shouldUpdateLevel(2);
    bool update_l3 = shouldUpdateLevel(3);
    bool update_l4 = shouldUpdateLevel(4);
    
    if (update_l0) updateL0(observation, action);
    if (update_l0 || update_l1) passBottomUp();  // L0 -> L1
    
    if (update_l1) updateL1();
    if (update_l1 || update_l2) passBottomUp();  // L1 -> L2
    
    if (update_l2) updateL2();
    if (update_l2 || update_l3) passBottomUp();  // L2 -> L3
    
    if (update_l3) updateL3();
    if (update_l3 || update_l4) passBottomUp();  // L3 -> L4
    
    if (update_l4) updateL4();
    
    // Top-down always flows (slow but necessary for consistency)
    passTopDown();
    passSkipConnections();
    
    if (update_l3) updateL3();
    if (update_l2) updateL2();
    if (update_l1) updateL1();
    if (update_l0) updateL0(observation, action);
    
    // LWM integration
    if (config_.use_latent_world_model && latent_world_model_ && has_level_observations_) {
        output_.lwm_output = latent_world_model_->forward(level_observations_, action, true);
        output_.lwm_active = true;
    }
    
    // Global metrics
    output_.total_free_energy = l0_.free_energy + l1_.free_energy + l2_.free_energy + l3_.free_energy + l4_.free_energy;
    output_.total_surprise = l0_.surprise + l1_.surprise + l2_.surprise + l3_.surprise + l4_.surprise;
    output_.level_escalation = output_.total_surprise > config_.surprise_threshold;
    
    output_.highest_active_level = 0;
    for (int l = 4; l >= 0; --l) {
        if (getLevel(l).surprise > config_.min_surprise_for_compute) {
            output_.highest_active_level = l;
            break;
        }
    }
    
    output_.l0 = l0_;
    output_.l1 = l1_;
    output_.l2 = l2_;
    output_.l3 = l3_;
    output_.l4 = l4_;
    
    return output_;
}

bool HierarchicalPredictiveCoding::shouldUpdateLevel(int level) const {
    if (!config_.event_driven) return true;
    
    const HPCLevelState& state = getLevel(level);
    return state.surprise > config_.min_surprise_for_compute || state.needs_update;
}

void HierarchicalPredictiveCoding::updateL0(const std::array<float, 24>& observation, 
                                             const std::vector<float>& action) {
    // L0: VSE/Sensory level - predicts immediate sensory input
    // Input: 24D observation from VSE
    
    // Generate prediction from L1 (top-down) and action
    if (generative_model_) {
        // Use generative model to predict sensory observation
        std::vector<float> l1_state = l1_.posterior;
        if (l1_state.size() > config_.l0_dim) l1_state.resize(config_.l0_dim);
        
        auto pred = predict(l1_state, action, 0);
        if (pred.size() == config_.l0_dim) {
            l0_.prediction = pred;
        }
    }
    
    // Top-down influence from L1
    if (!output_.l1_to_l0.empty()) {
        for (size_t i = 0; i < config_.l0_dim && i < output_.l1_to_l0.size(); ++i) {
            l0_.prediction[i] = config_.top_down_weight * output_.l1_to_l0[i] + 
                               (1.0f - config_.top_down_weight) * l0_.prediction[i];
        }
    }
    
    // Skip connection from L2
    if (!output_.l2_to_l0.empty()) {
        for (size_t i = 0; i < config_.l0_dim && i < output_.l2_to_l0.size(); ++i) {
            l0_.prediction[i] += config_.skip_weight * output_.l2_to_l0[i];
        }
    }
    
    // Compute prediction error: observation - prediction
    for (size_t i = 0; i < config_.l0_dim && i < 24; ++i) {
        l0_.prediction_error[i] = observation[i] - l0_.prediction[i];
    }
    
    // Precision-weighted prediction error
    std::vector<float> pwpe(config_.l0_dim);
    for (size_t i = 0; i < config_.l0_dim; ++i) {
        pwpe[i] = l0_.prediction_error[i] * l0_.precision[i];
    }
    
    // Update posterior: posterior = prediction + precision_weighted_error * lr
    for (size_t i = 0; i < config_.l0_dim; ++i) {
        l0_.posterior[i] = l0_.prediction[i] + config_.l0_lr * pwpe[i];
    }
    
    // Bottom-up message to L1
    output_.l0_to_l1 = l0_.posterior;
    if (output_.l0_to_l1.size() > config_.l1_dim) {
        output_.l0_to_l1.resize(config_.l1_dim);
    }
    
    // Skip to L2
    output_.l0_to_l2 = l0_.posterior;
    if (output_.l0_to_l2.size() > config_.l2_dim) {
        output_.l0_to_l2.resize(config_.l2_dim);
    }
    
    // Free energy at L0
    l0_.free_energy = computeFreeEnergy(l0_);
    l0_.surprise = 0.0f;
    for (float e : l0_.prediction_error) {
        l0_.surprise += e * e;
    }
    l0_.surprise /= config_.l0_dim;
    
    l0_.last_update = std::chrono::steady_clock::now();
    l0_.needs_update = false;
}

void HierarchicalPredictiveCoding::updateL1() {
    // L1: Intent level - predicts intent/classification
    
    // Bottom-up from L0
    if (!output_.l0_to_l1.empty()) {
        for (size_t i = 0; i < config_.l1_dim && i < output_.l0_to_l1.size(); ++i) {
            l1_.prediction[i] = config_.bottom_up_weight * output_.l0_to_l1[i] + 
                               (1.0f - config_.bottom_up_weight) * l1_.prediction[i];
        }
    }
    
    // Top-down from L2
    if (!output_.l2_to_l1.empty()) {
        for (size_t i = 0; i < config_.l1_dim && i < output_.l2_to_l1.size(); ++i) {
            l1_.prediction[i] = config_.top_down_weight * output_.l2_to_l1[i] + 
                               (1.0f - config_.top_down_weight) * l1_.prediction[i];
        }
    }
    
    // Skip from L3
    if (!output_.l3_to_l1.empty()) {
        for (size_t i = 0; i < config_.l1_dim && i < output_.l3_to_l1.size(); ++i) {
            l1_.prediction[i] += config_.skip_weight * output_.l3_to_l1[i];
        }
    }
    
    // Prediction error (target = L0 posterior projected up)
    for (size_t i = 0; i < config_.l1_dim && i < output_.l0_to_l1.size(); ++i) {
        l1_.prediction_error[i] = output_.l0_to_l1[i] - l1_.prediction[i];
    }
    
    // Precision-weighted update
    std::vector<float> pwpe(config_.l1_dim);
    for (size_t i = 0; i < config_.l1_dim; ++i) {
        pwpe[i] = l1_.prediction_error[i] * l1_.precision[i];
    }
    
    for (size_t i = 0; i < config_.l1_dim; ++i) {
        l1_.posterior[i] = l1_.prediction[i] + config_.l1_lr * pwpe[i];
    }
    
    // Bottom-up to L2
    output_.l1_to_l2 = l1_.posterior;
    if (output_.l1_to_l2.size() > config_.l2_dim) {
        output_.l1_to_l2.resize(config_.l2_dim);
    }
    
    // Skip to L3
    output_.l1_to_l3 = l1_.posterior;
    if (output_.l1_to_l3.size() > config_.l3_dim) {
        output_.l1_to_l3.resize(config_.l3_dim);
    }
    
    // Top-down to L0
    output_.l1_to_l0 = l1_.posterior;
    if (output_.l1_to_l0.size() > config_.l0_dim) {
        output_.l1_to_l0.resize(config_.l0_dim);
    }
    
    // Free energy
    l1_.free_energy = computeFreeEnergy(l1_);
    l1_.surprise = 0.0f;
    for (float e : l1_.prediction_error) {
        l1_.surprise += e * e;
    }
    l1_.surprise /= config_.l1_dim;
    
    l1_.last_update = std::chrono::steady_clock::now();
    l1_.needs_update = false;
}

void HierarchicalPredictiveCoding::updateL2() {
    // L2: Situational level - predicts situations/events
    
    // Bottom-up from L1
    if (!output_.l1_to_l2.empty()) {
        for (size_t i = 0; i < config_.l2_dim && i < output_.l1_to_l2.size(); ++i) {
            l2_.prediction[i] = config_.bottom_up_weight * output_.l1_to_l2[i] + 
                               (1.0f - config_.bottom_up_weight) * l2_.prediction[i];
        }
    }
    
    // Top-down from L3
    if (!output_.l3_to_l2.empty()) {
        for (size_t i = 0; i < config_.l2_dim && i < output_.l3_to_l2.size(); ++i) {
            l2_.prediction[i] = config_.top_down_weight * output_.l3_to_l2[i] + 
                               (1.0f - config_.top_down_weight) * l2_.prediction[i];
        }
    }
    
    // Skip from L4
    if (!output_.l4_to_l2.empty()) {
        for (size_t i = 0; i < config_.l2_dim && i < output_.l4_to_l2.size(); ++i) {
            l2_.prediction[i] += config_.skip_weight * output_.l4_to_l2[i];
        }
    }
    
    // Skip from L0
    if (!output_.l0_to_l2.empty()) {
        for (size_t i = 0; i < config_.l2_dim && i < output_.l0_to_l2.size(); ++i) {
            l2_.prediction[i] += config_.skip_weight * output_.l0_to_l2[i];
        }
    }
    
    // Prediction error
    for (size_t i = 0; i < config_.l2_dim && i < output_.l1_to_l2.size(); ++i) {
        l2_.prediction_error[i] = output_.l1_to_l2[i] - l2_.prediction[i];
    }
    
    // Precision-weighted update
    std::vector<float> pwpe(config_.l2_dim);
    for (size_t i = 0; i < config_.l2_dim; ++i) {
        pwpe[i] = l2_.prediction_error[i] * l2_.precision[i];
    }
    
    for (size_t i = 0; i < config_.l2_dim; ++i) {
        l2_.posterior[i] = l2_.prediction[i] + config_.l2_lr * pwpe[i];
    }
    
    // Bottom-up to L3
    output_.l2_to_l3 = l2_.posterior;
    if (output_.l2_to_l3.size() > config_.l3_dim) {
        output_.l2_to_l3.resize(config_.l3_dim);
    }
    
    // Skip to L4
    output_.l2_to_l4 = l2_.posterior;
    if (output_.l2_to_l4.size() > config_.l4_dim) {
        output_.l2_to_l4.resize(config_.l4_dim);
    }
    
    // Top-down to L1
    output_.l2_to_l1 = l2_.posterior;
    if (output_.l2_to_l1.size() > config_.l1_dim) {
        output_.l2_to_l1.resize(config_.l1_dim);
    }
    
    // Skip to L0
    output_.l2_to_l0 = l2_.posterior;
    if (output_.l2_to_l0.size() > config_.l0_dim) {
        output_.l2_to_l0.resize(config_.l0_dim);
    }
    
    // Free energy
    l2_.free_energy = computeFreeEnergy(l2_);
    l2_.surprise = 0.0f;
    for (float e : l2_.prediction_error) {
        l2_.surprise += e * e;
    }
    l2_.surprise /= config_.l2_dim;
    
    l2_.last_update = std::chrono::steady_clock::now();
    l2_.needs_update = false;
}

void HierarchicalPredictiveCoding::updateL3() {
    // L3: Conceptual level - predicts concepts/beliefs
    
    // Bottom-up from L2
    if (!output_.l2_to_l3.empty()) {
        for (size_t i = 0; i < config_.l3_dim && i < output_.l2_to_l3.size(); ++i) {
            l3_.prediction[i] = config_.bottom_up_weight * output_.l2_to_l3[i] + 
                               (1.0f - config_.bottom_up_weight) * l3_.prediction[i];
        }
    }
    
    // Top-down from L4
    if (!output_.l4_to_l3.empty()) {
        for (size_t i = 0; i < config_.l3_dim && i < output_.l4_to_l3.size(); ++i) {
            l3_.prediction[i] = config_.top_down_weight * output_.l4_to_l3[i] + 
                               (1.0f - config_.top_down_weight) * l3_.prediction[i];
        }
    }
    
    // Skip from L1
    if (!output_.l1_to_l3.empty()) {
        for (size_t i = 0; i < config_.l3_dim && i < output_.l1_to_l3.size(); ++i) {
            l3_.prediction[i] += config_.skip_weight * output_.l1_to_l3[i];
        }
    }
    
    // Use generative model / LWM for high-level prediction
    if (generative_model_) {
        std::vector<float> l2_state = l2_.posterior;
        if (l2_state.size() > config_.l3_dim) l2_state.resize(config_.l3_dim);
        
        auto pred = predict(l2_state, {}, 2);
        if (pred.size() == config_.l3_dim) {
            for (size_t i = 0; i < config_.l3_dim; ++i) {
                l3_.prediction[i] = 0.5f * l3_.prediction[i] + 0.5f * pred[i];
            }
        }
    }
    
    // Prediction error
    for (size_t i = 0; i < config_.l3_dim && i < output_.l2_to_l3.size(); ++i) {
        l3_.prediction_error[i] = output_.l2_to_l3[i] - l3_.prediction[i];
    }
    
    // Precision-weighted update
    std::vector<float> pwpe(config_.l3_dim);
    for (size_t i = 0; i < config_.l3_dim; ++i) {
        pwpe[i] = l3_.prediction_error[i] * l3_.precision[i];
    }
    
    for (size_t i = 0; i < config_.l3_dim; ++i) {
        l3_.posterior[i] = l3_.prediction[i] + config_.l3_lr * pwpe[i];
    }
    
    // Bottom-up to L4
    output_.l3_to_l4 = l3_.posterior;
    if (output_.l3_to_l4.size() > config_.l4_dim) {
        output_.l3_to_l4.resize(config_.l4_dim);
    }
    
    // Top-down to L2
    output_.l3_to_l2 = l3_.posterior;
    if (output_.l3_to_l2.size() > config_.l2_dim) {
        output_.l3_to_l2.resize(config_.l2_dim);
    }
    
    // Skip to L1
    output_.l3_to_l1 = l3_.posterior;
    if (output_.l3_to_l1.size() > config_.l1_dim) {
        output_.l3_to_l1.resize(config_.l1_dim);
    }
    
    // Free energy
    l3_.free_energy = computeFreeEnergy(l3_);
    l3_.surprise = 0.0f;
    for (float e : l3_.prediction_error) {
        l3_.surprise += e * e;
    }
    l3_.surprise /= config_.l3_dim;
    
    l3_.last_update = std::chrono::steady_clock::now();
    l3_.needs_update = false;
}

void HierarchicalPredictiveCoding::updateL4() {
    // L4: Abstract/World-Self level - predicts high-level world/self state
    
    // Bottom-up from L3
    if (!output_.l3_to_l4.empty()) {
        for (size_t i = 0; i < config_.l4_dim && i < output_.l3_to_l4.size(); ++i) {
            l4_.prediction[i] = config_.bottom_up_weight * output_.l3_to_l4[i] + 
                               (1.0f - config_.bottom_up_weight) * l4_.prediction[i];
        }
    }
    
    // Skip from L2
    if (!output_.l2_to_l4.empty()) {
        for (size_t i = 0; i < config_.l4_dim && i < output_.l2_to_l4.size(); ++i) {
            l4_.prediction[i] += config_.skip_weight * output_.l2_to_l4[i];
        }
    }
    
    // Use generative model for highest-level prediction
    if (generative_model_) {
        std::vector<float> l3_state = l3_.posterior;
        if (l3_state.size() > config_.l4_dim) l3_state.resize(config_.l4_dim);
        
        auto pred = predict(l3_state, {}, 3);
        if (pred.size() == config_.l4_dim) {
            for (size_t i = 0; i < config_.l4_dim; ++i) {
                l4_.prediction[i] = 0.5f * l4_.prediction[i] + 0.5f * pred[i];
            }
        }
    }
    
    // Prediction error
    for (size_t i = 0; i < config_.l4_dim && i < output_.l3_to_l4.size(); ++i) {
        l4_.prediction_error[i] = output_.l3_to_l4[i] - l4_.prediction[i];
    }
    
    // Precision-weighted update
    std::vector<float> pwpe(config_.l4_dim);
    for (size_t i = 0; i < config_.l4_dim; ++i) {
        pwpe[i] = l4_.prediction_error[i] * l4_.precision[i];
    }
    
    for (size_t i = 0; i < config_.l4_dim; ++i) {
        l4_.posterior[i] = l4_.prediction[i] + config_.l4_lr * pwpe[i];
    }
    
    // Top-down to L3
    output_.l4_to_l3 = l4_.posterior;
    if (output_.l4_to_l3.size() > config_.l3_dim) {
        output_.l4_to_l3.resize(config_.l3_dim);
    }
    
    // Skip to L2
    output_.l4_to_l2 = l4_.posterior;
    if (output_.l4_to_l2.size() > config_.l2_dim) {
        output_.l4_to_l2.resize(config_.l2_dim);
    }
    
    // Free energy
    l4_.free_energy = computeFreeEnergy(l4_);
    l4_.surprise = 0.0f;
    for (float e : l4_.prediction_error) {
        l4_.surprise += e * e;
    }
    l4_.surprise /= config_.l4_dim;
    
    l4_.last_update = std::chrono::steady_clock::now();
    l4_.needs_update = false;
}

void HierarchicalPredictiveCoding::passTopDown() {
    // L4 → L3
    if (!l4_.posterior.empty()) {
        output_.l4_to_l3 = l4_.posterior;
        if (output_.l4_to_l3.size() > config_.l3_dim) {
            output_.l4_to_l3.resize(config_.l3_dim);
        }
    }
    
    // L3 → L2
    if (!l3_.posterior.empty()) {
        output_.l3_to_l2 = l3_.posterior;
        if (output_.l3_to_l2.size() > config_.l2_dim) {
            output_.l3_to_l2.resize(config_.l2_dim);
        }
    }
    
    // L2 → L1
    if (!l2_.posterior.empty()) {
        output_.l2_to_l1 = l2_.posterior;
        if (output_.l2_to_l1.size() > config_.l1_dim) {
            output_.l2_to_l1.resize(config_.l1_dim);
        }
    }
    
    // L1 → L0
    if (!l1_.posterior.empty()) {
        output_.l1_to_l0 = l1_.posterior;
        if (output_.l1_to_l0.size() > config_.l0_dim) {
            output_.l1_to_l0.resize(config_.l0_dim);
        }
    }
}

void HierarchicalPredictiveCoding::passBottomUp() {
    // L0 → L1
    if (!l0_.posterior.empty()) {
        output_.l0_to_l1 = l0_.posterior;
        if (output_.l0_to_l1.size() > config_.l1_dim) {
            output_.l0_to_l1.resize(config_.l1_dim);
        }
    }
    
    // L1 → L2
    if (!l1_.posterior.empty()) {
        output_.l1_to_l2 = l1_.posterior;
        if (output_.l1_to_l2.size() > config_.l2_dim) {
            output_.l1_to_l2.resize(config_.l2_dim);
        }
    }
    
    // L2 → L3
    if (!l2_.posterior.empty()) {
        output_.l2_to_l3 = l2_.posterior;
        if (output_.l2_to_l3.size() > config_.l3_dim) {
            output_.l2_to_l3.resize(config_.l3_dim);
        }
    }
    
    // L3 → L4
    if (!l3_.posterior.empty()) {
        output_.l3_to_l4 = l3_.posterior;
        if (output_.l3_to_l4.size() > config_.l4_dim) {
            output_.l3_to_l4.resize(config_.l4_dim);
        }
    }
}

void HierarchicalPredictiveCoding::passSkipConnections() {
    // L4 → L2
    if (!l4_.posterior.empty()) {
        output_.l4_to_l2 = l4_.posterior;
        if (output_.l4_to_l2.size() > config_.l2_dim) {
            output_.l4_to_l2.resize(config_.l2_dim);
        }
    }
    
    // L3 → L1
    if (!l3_.posterior.empty()) {
        output_.l3_to_l1 = l3_.posterior;
        if (output_.l3_to_l1.size() > config_.l1_dim) {
            output_.l3_to_l1.resize(config_.l1_dim);
        }
    }
    
    // L2 → L0
    if (!l2_.posterior.empty()) {
        output_.l2_to_l0 = l2_.posterior;
        if (output_.l2_to_l0.size() > config_.l0_dim) {
            output_.l2_to_l0.resize(config_.l0_dim);
        }
    }
    
    // L0 → L2
    if (!l0_.posterior.empty()) {
        output_.l0_to_l2 = l0_.posterior;
        if (output_.l0_to_l2.size() > config_.l2_dim) {
            output_.l0_to_l2.resize(config_.l2_dim);
        }
    }
    
    // L1 → L3
    if (!l1_.posterior.empty()) {
        output_.l1_to_l3 = l1_.posterior;
        if (output_.l1_to_l3.size() > config_.l3_dim) {
            output_.l1_to_l3.resize(config_.l3_dim);
        }
    }
    
    // L2 → L4
    if (!l2_.posterior.empty()) {
        output_.l2_to_l4 = l2_.posterior;
        if (output_.l2_to_l4.size() > config_.l4_dim) {
            output_.l2_to_l4.resize(config_.l4_dim);
        }
    }
}

float HierarchicalPredictiveCoding::computeFreeEnergy(const HPCLevelState& level) const {
    // Variational free energy: F = 0.5 * precision * prediction_error^2 - 0.5 * log(precision) + const
    float fe = 0.0f;
    for (size_t i = 0; i < level.prediction_error.size(); ++i) {
        float pe = level.prediction_error[i];
        float prec = level.precision[i];
        fe += 0.5f * prec * pe * pe - 0.5f * std::log(prec + 1e-8f);
    }
    return fe;
}

std::vector<float> HierarchicalPredictiveCoding::predict(const std::vector<float>& state, 
                                                          const std::vector<float>& action,
                                                          int level) const {
    if (!generative_model_) {
        size_t dim = (level == 0) ? config_.l0_dim : 
                     (level == 1) ? config_.l1_dim :
                     (level == 2) ? config_.l2_dim :
                     (level == 3) ? config_.l3_dim : config_.l4_dim;
        return std::vector<float>(dim, 0.0f);
    }
    
    // Use generative model's forward pass
    // For now, return identity (placeholder)
    size_t dim = (level == 0) ? config_.l0_dim : 
                 (level == 1) ? config_.l1_dim :
                 (level == 2) ? config_.l2_dim :
                 (level == 3) ? config_.l3_dim : config_.l4_dim;
    
    std::vector<float> result(dim);
    for (size_t i = 0; i < dim && i < state.size(); ++i) {
        result[i] = state[i];
    }
    return result;
}

void HierarchicalPredictiveCoding::softmax(std::vector<float>& vec) const {
    float max_val = *std::max_element(vec.begin(), vec.end());
    float sum = 0.0f;
    for (float& v : vec) {
        v = std::exp(v - max_val);
        sum += v;
    }
    for (float& v : vec) {
        v /= (sum + 1e-8f);
    }
}

void HierarchicalPredictiveCoding::resizeVector(std::vector<float>& vec, size_t new_size, float fill) {
    if (vec.size() < new_size) {
        vec.resize(new_size, fill);
    } else if (vec.size() > new_size) {
        vec.resize(new_size);
    }
}

std::vector<float> HierarchicalPredictiveCoding::project(const std::vector<float>& src, size_t target_dim) const {
    std::vector<float> result(target_dim, 0.0f);
    size_t copy_size = std::min(src.size(), target_dim);
    for (size_t i = 0; i < copy_size; ++i) {
        result[i] = src[i];
    }
    return result;
}

predictive::LatentWorldModel::ImaginationResult HierarchicalPredictiveCoding::imagine(
    const std::vector<std::vector<float>>& policy_actions,
    int horizon) const {
    
    if (!latent_world_model_) {
        return {};
    }
    
    // Get current RSSM states as initial states
    std::array<predictive::RSSMState, 5> initial_states;
    // LWM level dimensions (must match LatentWorldModelConfig)
    static const size_t sto_dims[5] = {8, 16, 32, 32, 64};
    static const size_t disc_dims[5] = {16, 32, 32, 64, 64};
    for (int l = 0; l < 5; ++l) {
        // Convert HPC state to RSSM state (simplified)
        const HPCLevelState& hpc = getLevel(l);
        predictive::RSSMState rssm;
        rssm.deterministic = hpc.posterior;
        rssm.stochastic.assign(sto_dims[l], 0.0f);
        rssm.discrete_logits.assign(disc_dims[l], 0.0f);
        rssm.discrete_probs.assign(disc_dims[l], 0.0f);
        initial_states[l] = rssm;
    }
    
    return latent_world_model_->imagine(initial_states, policy_actions, horizon);
}

void HierarchicalPredictiveCoding::reset() {
    std::fill(l0_.prediction.begin(), l0_.prediction.end(), 0.0f);
    std::fill(l0_.prediction_error.begin(), l0_.prediction_error.end(), 0.0f);
    std::fill(l0_.posterior.begin(), l0_.posterior.end(), 0.0f);
    
    std::fill(l1_.prediction.begin(), l1_.prediction.end(), 0.0f);
    std::fill(l1_.prediction_error.begin(), l1_.prediction_error.end(), 0.0f);
    std::fill(l1_.posterior.begin(), l1_.posterior.end(), 0.0f);
    
    std::fill(l2_.prediction.begin(), l2_.prediction.end(), 0.0f);
    std::fill(l2_.prediction_error.begin(), l2_.prediction_error.end(), 0.0f);
    std::fill(l2_.posterior.begin(), l2_.posterior.end(), 0.0f);
    
    std::fill(l3_.prediction.begin(), l3_.prediction.end(), 0.0f);
    std::fill(l3_.prediction_error.begin(), l3_.prediction_error.end(), 0.0f);
    std::fill(l3_.posterior.begin(), l3_.posterior.end(), 0.0f);
    
    std::fill(l4_.prediction.begin(), l4_.prediction.end(), 0.0f);
    std::fill(l4_.prediction_error.begin(), l4_.prediction_error.end(), 0.0f);
    std::fill(l4_.posterior.begin(), l4_.posterior.end(), 0.0f);
    
    output_.total_free_energy = 0.0f;
    output_.total_surprise = 0.0f;
    output_.level_escalation = false;
    output_.highest_active_level = 0;
    output_.lwm_active = false;
    has_level_observations_ = false;
}

void HierarchicalPredictiveCoding::reestimatePrecision() {
    // Re-estimate precision from prediction error statistics
    auto update_prec = [this](HPCLevelState& level, float base_prec) {
        for (size_t i = 0; i < level.precision.size(); ++i) {
            float error_var = level.prediction_error[i] * level.prediction_error[i];
            level.precision[i] = base_prec / (1.0f + error_var);
            level.precision[i] = std::clamp(level.precision[i], 0.1f, 100.0f);
        }
    };
    
    update_prec(l0_, config_.l0_precision);
    update_prec(l1_, config_.l1_precision);
    update_prec(l2_, config_.l2_precision);
    update_prec(l3_, config_.l3_precision);
    update_prec(l4_, config_.l4_precision);
}

const HPCLevelState& HierarchicalPredictiveCoding::getLevel(int level) const {
    switch (level) {
        case 0: return l0_;
        case 1: return l1_;
        case 2: return l2_;
        case 3: return l3_;
        case 4: return l4_;
        default: return l0_;
    }
}

HPCLevelState& HierarchicalPredictiveCoding::getLevel(int level) {
    switch (level) {
        case 0: return l0_;
        case 1: return l1_;
        case 2: return l2_;
        case 3: return l3_;
        case 4: return l4_;
        default: return l0_;
    }
}

} // namespace core
} // namespace brain
} // namespace yuki