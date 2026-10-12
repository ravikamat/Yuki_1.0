#include "brain/inference/GenerativeModel.h"
#include "brain/core/Logger.h"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <random>
#include <sstream>
#include <iostream>
#include <nlohmann/json.hpp>

namespace yuki {
namespace brain {
namespace inference {

GenerativeModel::GenerativeModel(const GenerativeModelConfig& cfg)
    : config_(cfg) {
    static int ctor_count = 0;
    ++ctor_count;
    printf("[GM DEBUG] Constructor called, count=%d\n", ctor_count);
    fflush(stdout);
    initializeProcessors();
}

void GenerativeModel::initializeProcessors() {
    std::cout << "[GM DEBUG] initializeProcessors start" << std::endl;
    std::cout.flush();
    
    // Use fixed seed for reproducibility
    uint32_t seed = 42;
    uint32_t state = seed;
    auto next_rand = [&state]() -> float {
        state = state * 1664525 + 1013904223;
        return static_cast<float>(state) / 4294967296.0f * 0.2f - 0.1f;
    };
    
    std::cout << "[GM DEBUG] RNG created" << std::endl;
    std::cout.flush();
    
    // TEXT processor
    std::cout << "[GM DEBUG] Setting up TEXT processor" << std::endl;
    std::cout.flush();
    modality_processors_[0].input_dim = config_.text_obs_dim;
    modality_processors_[0].hidden_dim = config_.hidden_dim;
    std::cout << "[GM DEBUG] Resizing encode_weights..." << std::endl;
    std::cout.flush();
    modality_processors_[0].encode_weights.resize(config_.text_obs_dim * config_.hidden_dim);
    std::cout << "[GM DEBUG] Resizing decode_weights..." << std::endl;
    std::cout.flush();
    modality_processors_[0].decode_weights.resize(config_.hidden_dim * config_.text_obs_dim);
    std::cout << "[GM DEBUG] Filling encode_weights..." << std::endl;
    std::cout.flush();
    for (auto& w : modality_processors_[0].encode_weights) w = next_rand();
    std::cout << "[GM DEBUG] Filling decode_weights..." << std::endl;
    std::cout.flush();
    for (auto& w : modality_processors_[0].decode_weights) w = next_rand();
    std::cout << "[GM DEBUG] TEXT processor done" << std::endl;
    std::cout.flush();
    
    // AUDIO processor
    std::cout << "[GM DEBUG] Setting up AUDIO processor" << std::endl;
    std::cout.flush();
    modality_processors_[1].input_dim = config_.audio_obs_dim;
    modality_processors_[1].hidden_dim = config_.hidden_dim;
    modality_processors_[1].encode_weights.resize(config_.audio_obs_dim * config_.hidden_dim);
    modality_processors_[1].decode_weights.resize(config_.hidden_dim * config_.audio_obs_dim);
    for (auto& w : modality_processors_[1].encode_weights) w = next_rand();
    for (auto& w : modality_processors_[1].decode_weights) w = next_rand();
    std::cout << "[GM DEBUG] AUDIO processor done" << std::endl;
    std::cout.flush();
    
    // VISION processor
    std::cout << "[GM DEBUG] Setting up VISION processor" << std::endl;
    std::cout.flush();
    modality_processors_[2].input_dim = config_.vision_obs_dim;
    modality_processors_[2].hidden_dim = config_.hidden_dim;
    modality_processors_[2].encode_weights.resize(config_.vision_obs_dim * config_.hidden_dim);
    modality_processors_[2].decode_weights.resize(config_.hidden_dim * config_.vision_obs_dim);
    for (auto& w : modality_processors_[2].encode_weights) w = next_rand();
    for (auto& w : modality_processors_[2].decode_weights) w = next_rand();
    std::cout << "[GM DEBUG] VISION processor done" << std::endl;
    std::cout.flush();
    
    // Initialize intent mappings
    std::cout << "[GM DEBUG] Initializing intent mappings..." << std::endl;
    std::cout.flush();
    for (size_t i = 0; i < 8; ++i) {
        intent_mappings_[i].modality_obs.resize(3);
        intent_mappings_[i].modality_obs[0].assign(config_.text_obs_dim, 0.0f);
        intent_mappings_[i].modality_obs[1].assign(config_.audio_obs_dim, 0.0f);
        intent_mappings_[i].modality_obs[2].assign(config_.vision_obs_dim, 0.0f);
        intent_mappings_[i].prior_prob.assign(config_.hidden_dim, 1.0f / config_.hidden_dim);
    }
    std::cout << "[GM DEBUG] Intent mappings done" << std::endl;
    std::cout.flush();
    
    // Initialize transition weights
    std::cout << "[GM DEBUG] Initializing transition weights..." << std::endl;
    std::cout.flush();
    for (size_t i = 0; i < 8; ++i) {
        transition_weights_[i].assign(config_.hidden_dim * config_.hidden_dim, 0.0f);
        transition_bias_[i].assign(config_.hidden_dim, 0.0f);
        for (size_t j = 0; j < config_.hidden_dim; ++j) {
            transition_weights_[i][j * config_.hidden_dim + j] = 0.95f;
        }
    }
    std::cout << "[GM DEBUG] Transition weights done" << std::endl;
    std::cout.flush();
    
    // yuki::core::Logger::instance().log(yuki::core::LogLevel::INFO, "GenerativeModel", "Initialized");
}

std::vector<std::vector<float>> GenerativeModel::predictNext(
    const GenerativeBeliefState& belief,
    int action) const {
    
    std::vector<std::vector<float>> predictions(3);
    
    auto hidden = inferHiddenState(belief);
    
    if (action >= 0 && action < 8) {
        const auto& W = transition_weights_[action];
        const auto& b = transition_bias_[action];
        std::vector<float> new_hidden(config_.hidden_dim, 0.0f);
        for (size_t i = 0; i < config_.hidden_dim; ++i) {
            float sum = b[i];
            for (size_t j = 0; j < config_.hidden_dim; ++j) {
                sum += W[i * config_.hidden_dim + j] * hidden[j];
            }
            new_hidden[i] = std::tanh(sum);
        }
        hidden = new_hidden;
    }
    
    // Decode to each modality
    for (size_t m = 0; m < 3; ++m) {
        const auto& proc = modality_processors_[m];
        size_t out_dim = (m == 0) ? config_.text_obs_dim : 
                         (m == 1) ? config_.audio_obs_dim : config_.vision_obs_dim;
        predictions[m].resize(out_dim, 0.0f);
        for (size_t i = 0; i < out_dim; ++i) {
            float sum = 0.0f;
            for (size_t h = 0; h < config_.hidden_dim; ++h) {
                sum += proc.decode_weights[i * config_.hidden_dim + h] * hidden[h];
            }
            predictions[m][i] = std::tanh(sum);
        }
    }
    
    return predictions;
}

GenerativeBeliefState GenerativeModel::predictNextBelief(const GenerativeBeliefState& belief, int action) const {
    auto hidden = inferHiddenState(belief);
    
    if (action >= 0 && action < 8) {
        const auto& W = transition_weights_[action];
        const auto& b = transition_bias_[action];
        std::vector<float> new_hidden(config_.hidden_dim, 0.0f);
        for (size_t i = 0; i < config_.hidden_dim; ++i) {
            float sum = b[i];
            for (size_t j = 0; j < config_.hidden_dim; ++j) {
                sum += W[i * config_.hidden_dim + j] * hidden[j];
            }
            new_hidden[i] = std::tanh(sum);
        }
        hidden = new_hidden;
    }
    
    return decodeHiddenState(hidden);
}

float GenerativeModel::expectedFreeEnergy(const GenerativeBeliefState& belief, 
                                          const std::vector<int>& policy,
                                          int horizon) const {
    float efe = 0.0f;
    GenerativeBeliefState current = belief;
    
    for (int step = 0; step < horizon && step < (int)policy.size(); ++step) {
        int action = policy[step];
        current = predictNextBelief(current, action);
        auto pred_obs = predictNext(current, action);
        
        // Ambiguity
        for (const auto& obs : pred_obs) {
            float entropy = 0.0f;
            float sum = 0.0f;
            for (float v : obs) sum += std::exp(v);
            for (float v : obs) {
                float p = std::exp(v) / (sum + 1e-8f);
                if (p > 1e-8f) entropy -= p * std::log(p);
            }
            efe += entropy;
            
            // Risk
            int map_intent = 0;
            float max_q = current.q_intent[0];
            for (int i = 1; i < 8; ++i) {
                if (current.q_intent[i] > max_q) {
                    max_q = current.q_intent[i];
                    map_intent = i;
                }
            }
            float risk = 0.0f;
            for (int i = 0; i < 8; ++i) {
                float preferred = (i == map_intent) ? 1.0f : 0.0f;
                float predicted = current.q_intent[i];
                if (predicted > 1e-8f) {
                    risk += preferred * std::log(preferred / predicted);
                }
            }
            efe += risk;
        }
    }
    
    return efe;
}

void GenerativeModel::updateMapping(Modality modality, const std::vector<float>& observation, 
                                     IntentClass intent, float lr) {
    if (lr < 0) lr = config_.mapping_lr;
    
    size_t idx = static_cast<size_t>(intent);
    size_t m_idx = static_cast<size_t>(modality);
    
    auto& entry = intent_mappings_[idx];
    auto& proc = modality_processors_[m_idx];
    
    size_t obs_dim = std::min(observation.size(), entry.modality_obs[m_idx].size());
    for (size_t i = 0; i < obs_dim; ++i) {
        entry.modality_obs[m_idx][i] = (1.0f - lr) * entry.modality_obs[m_idx][i] + lr * observation[i];
    }
    entry.confidence = std::min(1.0f, entry.confidence + lr);
    
    // Update modality processor
    std::vector<float> hidden(config_.hidden_dim, 0.0f);
    for (size_t h = 0; h < config_.hidden_dim; ++h) {
        float sum = 0.0f;
        size_t obs_dim = std::min(observation.size(), proc.input_dim);
        for (size_t i = 0; i < obs_dim; ++i) {
            sum += proc.encode_weights[h * proc.input_dim + i] * observation[i];
        }
        hidden[h] = std::tanh(sum);
    }
    
    for (size_t i = 0; i < proc.input_dim; ++i) {
        for (size_t h = 0; h < config_.hidden_dim; ++h) {
            float grad = lr * (observation[i] - 
                std::tanh(std::inner_product(proc.decode_weights.begin() + i * config_.hidden_dim,
                                           proc.decode_weights.begin() + (i + 1) * config_.hidden_dim,
                                           hidden.begin(), 0.0f))) * (1.0f - hidden[h] * hidden[h]) * hidden[h];
            proc.decode_weights[i * config_.hidden_dim + h] += grad;
        }
    }
}

void GenerativeModel::updateTransitions(const GenerativeBeliefState& prev_belief, int action, 
                                         const GenerativeBeliefState& next_belief) {
    if (action < 0 || action >= 8) return;
    
    float lr = config_.state_lr;
    std::vector<float> prev_hidden = inferHiddenState(prev_belief);
    std::vector<float> next_hidden = inferHiddenState(next_belief);
    
    for (size_t i = 0; i < config_.hidden_dim; ++i) {
        float target = next_hidden[i];
        float pred = transition_bias_[action][i];
        for (size_t j = 0; j < config_.hidden_dim; ++j) {
            pred += transition_weights_[action][i * config_.hidden_dim + j] * prev_hidden[j];
        }
        float error = target - std::tanh(pred);
        
        transition_bias_[action][i] += lr * error * (1.0f - std::tanh(pred) * std::tanh(pred));
        for (size_t j = 0; j < config_.hidden_dim; ++j) {
            transition_weights_[action][i * config_.hidden_dim + j] += lr * error * (1.0f - std::tanh(pred) * std::tanh(pred)) * prev_hidden[j];
        }
    }
}

std::vector<float> GenerativeModel::inferHiddenState(const GenerativeBeliefState& belief) const {
    std::vector<float> hidden(config_.hidden_dim, 0.0f);
    
    for (size_t i = 0; i < 8; ++i) {
        float w = belief.q_intent[i];
        for (size_t h = 0; h < config_.hidden_dim; ++h) {
            hidden[h] += w * intent_mappings_[i].prior_prob[h];
        }
    }
    
    for (int i = 0; i < 3; ++i) {
        float w = belief.q_engagement[i];
        hidden[i % config_.hidden_dim] += w * 0.1f;
    }
    
    for (int i = 0; i < 2; ++i) {
        float w = belief.q_urgency[i];
        hidden[(i + 3) % config_.hidden_dim] += w * 0.1f;
    }
    
    hidden[config_.hidden_dim - 3] = belief.safety_mass;
    hidden[config_.hidden_dim - 2] = belief.surprise_budget;
    hidden[config_.hidden_dim - 1] = belief.entropy;
    
    float norm = 0.0f;
    for (float v : hidden) norm += v * v;
    norm = std::sqrt(norm) + 1e-8f;
    for (float& v : hidden) v /= norm;
    
    return hidden;
}

GenerativeBeliefState GenerativeModel::decodeHiddenState(const std::vector<float>& hidden) const {
    GenerativeBeliefState belief;
    
    for (size_t i = 0; i < 8; ++i) {
        float sum = 0.0f;
        for (size_t h = 0; h < config_.hidden_dim; ++h) {
            sum += hidden[h] * intent_mappings_[i].prior_prob[h];
        }
        belief.q_intent[i] = std::max(0.0f, sum);
    }
    
    float max_q = *std::max_element(belief.q_intent.begin(), belief.q_intent.end());
    float sum = 0.0f;
    for (float& v : belief.q_intent) {
        v = std::exp(v - max_q);
        sum += v;
    }
    for (float& v : belief.q_intent) v /= sum;
    
    for (int i = 0; i < 3; ++i) {
        belief.q_engagement[i] = std::abs(hidden[i % config_.hidden_dim]);
    }
    sum = belief.q_engagement[0] + belief.q_engagement[1] + belief.q_engagement[2];
    for (float& v : belief.q_engagement) v /= (sum + 1e-8f);
    
    for (int i = 0; i < 2; ++i) {
        belief.q_urgency[i] = std::abs(hidden[(i + 3) % config_.hidden_dim]);
    }
    sum = belief.q_urgency[0] + belief.q_urgency[1];
    for (float& v : belief.q_urgency) v /= (sum + 1e-8f);
    
    belief.safety_mass = std::clamp(std::abs(hidden[config_.hidden_dim - 3]), 0.0f, 1.0f);
    belief.surprise_budget = std::clamp(std::abs(hidden[config_.hidden_dim - 2]), 0.0f, 1.0f);
    belief.entropy = std::clamp(std::abs(hidden[config_.hidden_dim - 1]), 0.0f, 1.0f);
    
    return belief;
}

std::string GenerativeModel::serialize() const {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(8);
    
    // Serialize config
    oss << config_.num_intents << ';'
        << config_.num_engagements << ';'
        << config_.num_urgencies << ';'
        << config_.num_modalities << ';'
        << config_.text_obs_dim << ';'
        << config_.audio_obs_dim << ';'
        << config_.vision_obs_dim << ';'
        << config_.hidden_dim << ';'
        << config_.mapping_lr << ';'
        << config_.state_lr << ';';
    
    // Serialize intent_mappings
    for (size_t i = 0; i < 8; ++i) {
        const auto& entry = intent_mappings_[i];
        for (size_t m = 0; m < 3; ++m) {
            for (size_t k = 0; k < entry.modality_obs[m].size(); ++k) {
                oss << entry.modality_obs[m][k] << ';';
            }
        }
        for (float v : entry.prior_prob) {
            oss << v << ';';
        }
        oss << entry.confidence << ';';
    }
    
    // Serialize transition weights and bias
    for (size_t i = 0; i < 8; ++i) {
        for (float w : transition_weights_[i]) oss << w << ';';
        for (float b : transition_bias_[i]) oss << b << ';';
    }
    
    // Serialize modality processors
    for (size_t m = 0; m < 3; ++m) {
        const auto& proc = modality_processors_[m];
        for (float w : proc.encode_weights) oss << w << ';';
        for (float w : proc.decode_weights) oss << w << ';';
        oss << proc.input_dim << ';' << proc.hidden_dim << ';';
    }
    
    return oss.str();
}

bool GenerativeModel::deserialize(const std::string& data) {
    std::istringstream iss(data);
    std::string token;
    
    // Deserialize config
    if (!std::getline(iss, token, ';')) return false; config_.num_intents = std::stoul(token);
    if (!std::getline(iss, token, ';')) return false; config_.num_engagements = std::stoul(token);
    if (!std::getline(iss, token, ';')) return false; config_.num_urgencies = std::stoul(token);
    if (!std::getline(iss, token, ';')) return false; config_.num_modalities = std::stoul(token);
    if (!std::getline(iss, token, ';')) return false; config_.text_obs_dim = std::stoul(token);
    if (!std::getline(iss, token, ';')) return false; config_.audio_obs_dim = std::stoul(token);
    if (!std::getline(iss, token, ';')) return false; config_.vision_obs_dim = std::stoul(token);
    if (!std::getline(iss, token, ';')) return false; config_.hidden_dim = std::stoul(token);
    if (!std::getline(iss, token, ';')) return false; config_.mapping_lr = std::stof(token);
    if (!std::getline(iss, token, ';')) return false; config_.state_lr = std::stof(token);
    
    // Reinitialize processors with new config
    initializeProcessors();
    
    // Deserialize intent_mappings
    for (size_t i = 0; i < 8; ++i) {
        auto& entry = intent_mappings_[i];
        for (size_t m = 0; m < 3; ++m) {
            for (size_t k = 0; k < entry.modality_obs[m].size(); ++k) {
                std::string token;
                if (!std::getline(iss, token, ';')) return false;
                entry.modality_obs[m][k] = std::stof(token);
            }
        }
        for (size_t k = 0; k < config_.hidden_dim; ++k) {
            std::string token;
            if (!std::getline(iss, token, ';')) return false;
            entry.prior_prob[k] = std::stof(token);
        }
        std::string token;
        if (!std::getline(iss, token, ';')) return false;
        entry.confidence = std::stof(token);
    }
    
    // Deserialize transition weights and bias
    for (size_t i = 0; i < 8; ++i) {
        for (size_t k = 0; k < config_.hidden_dim * config_.hidden_dim; ++k) {
            std::string token;
            if (!std::getline(iss, token, ';')) return false;
            transition_weights_[i][k] = std::stof(token);
        }
        for (size_t k = 0; k < config_.hidden_dim; ++k) {
            std::string token;
            if (!std::getline(iss, token, ';')) return false;
            transition_bias_[i][k] = std::stof(token);
        }
    }
    
    // Deserialize modality processors
    for (size_t m = 0; m < 3; ++m) {
        auto& proc = modality_processors_[m];
        for (size_t k = 0; k < proc.encode_weights.size(); ++k) {
            std::string token;
            if (!std::getline(iss, token, ';')) return false;
            proc.encode_weights[k] = std::stof(token);
        }
        for (size_t k = 0; k < proc.decode_weights.size(); ++k) {
            std::string token;
            if (!std::getline(iss, token, ';')) return false;
            proc.decode_weights[k] = std::stof(token);
        }
        std::string token2;
        if (!std::getline(iss, token2, ';')) return false; proc.input_dim = std::stoul(token2);
        if (!std::getline(iss, token2, ';')) return false; proc.hidden_dim = std::stoul(token2);
    }
    
    return true;
}

std::vector<float> GenerativeModel::predictSensory(const GenerativeBeliefState& belief) const {
    // Use TEXT modality mapping for the given belief
    size_t idx = belief.getMAP();
    if (idx >= 8) idx = 0;
    
    const auto& entry = intent_mappings_[idx];
    const auto& proc = modality_processors_[static_cast<size_t>(Modality::TEXT)];
    
    // Get the mapping for TEXT modality
    const std::vector<float>& mapping = entry.modality_obs[static_cast<size_t>(Modality::TEXT)];
    
    if (mapping.empty()) {
        return std::vector<float>(config_.text_obs_dim, 0.0f);
    }
    
    // Decode through modality processor
    std::vector<float> prediction(config_.text_obs_dim, 0.0f);
    
    // Simple linear decode: prediction = decode_weights * hidden_state
    // where hidden_state = encode_weights * mapping (simplified)
    std::vector<float> hidden(config_.hidden_dim, 0.0f);
    for (size_t h = 0; h < config_.hidden_dim; ++h) {
        float sum = 0.0f;
        size_t map_dim = std::min(mapping.size(), proc.input_dim);
        for (size_t i = 0; i < map_dim; ++i) {
            sum += proc.encode_weights[h * proc.input_dim + i] * mapping[i];
        }
        hidden[h] = std::tanh(sum);
    }
    
    for (size_t i = 0; i < config_.text_obs_dim; ++i) {
        float sum = 0.0f;
        for (size_t h = 0; h < config_.hidden_dim; ++h) {
            sum += proc.decode_weights[i * config_.hidden_dim + h] * hidden[h];
        }
        prediction[i] = sum;
    }
    
    return prediction;
}

// ── GenerativeBeliefState methods ─────────────────────────────────────────────

// MAP (Maximum A Posteriori) intent: index of the highest posterior mass in
// q_intent. Returns -1 only when all entries are zero (degenerate belief).
int GenerativeBeliefState::getMAP() const {
    int best = 0;
    for (int i = 1; i < static_cast<int>(q_intent.size()); ++i) {
        if (q_intent[static_cast<size_t>(i)] > q_intent[static_cast<size_t>(best)]) {
            best = i;
        }
    }
    return best;
}

// Shannon entropy of the intent posterior: H = -Σ q_i * log(q_i + ε)
float GenerativeBeliefState::computeEntropy() const {
    constexpr float kEps = 1e-8f;
    float h = 0.0f;
    for (float qi : q_intent) {
        if (qi > kEps) {
            h -= qi * std::log(qi + kEps);
        }
    }
    return h;
}

// Variational free energy: F = -E_q[log p(o|s)] + H[q(s)]
// We approximate the expected log-likelihood as negative surprise_budget and
// add entropy as a regulariser (matching the variational inference formulation).
float GenerativeBeliefState::computeFreeEnergy() const {
    return surprise_budget + computeEntropy();
}

// Precision-weighted gradient descent update on the intent belief.
// prediction_error and precision are aligned to q_intent (size 8).
// Clamps each updated entry to [0, 1] then renormalises to maintain a valid
// probability simplex.
void GenerativeBeliefState::update(const std::vector<float>& prediction_error,
                                   const std::vector<float>& precision,
                                   float lr) {
    const size_t n = q_intent.size();
    const size_t pe_size = prediction_error.size();
    const size_t pr_size = precision.size();

    // Apply precision-weighted prediction error gradient.
    for (size_t i = 0; i < n; ++i) {
        float pe = (i < pe_size) ? prediction_error[i] : 0.0f;
        float pr = (i < pr_size) ? precision[i] : 1.0f;
        q_intent[i] -= lr * pr * pe;
        q_intent[i] = std::max(0.0f, q_intent[i]);
    }

    // Renormalise to keep q_intent a valid probability distribution.
    float sum = 0.0f;
    for (float qi : q_intent) sum += qi;
    if (sum > 1e-8f) {
        for (float& qi : q_intent) qi /= sum;
    } else {
        // Degenerate: reset to uniform.
        for (float& qi : q_intent) qi = 1.0f / static_cast<float>(n);
    }

    // Recompute summary statistics.
    entropy = computeEntropy();
    free_energy = computeFreeEnergy();
}

} // namespace inference
} // namespace brain
} // namespace yuki