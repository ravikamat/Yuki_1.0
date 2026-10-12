#pragma once
// LatentWorldModel — Hierarchical RSSM + JEPA for 5-level HPC
// L0=VSE (24D), L1=Intent (64D), L2=Situational (128D), L3=Conceptual (256D), L4=Abstract (512D)

#include <array>
#include <vector>
#include <memory>
#include <random>
#include <cmath>
#include <algorithm>
#include "brain/inference/GenerativeModel.h"

namespace yuki {
namespace brain {
namespace predictive {

struct LatentWorldModelConfig {
    // Per-level dimensions
    struct LevelConfig {
        size_t deterministic_dim;  // RSSM deterministic state
        size_t stochastic_dim;     // RSSM stochastic state (continuous)
        size_t discrete_categories; // Discrete latent categories
        size_t jepa_context_dim;   // JEPA context encoder dim
        size_t jepa_target_dim;    // JEPA target encoder dim
        float kl_weight = 1.0f;
        float free_nats = 3.0f;
        float learning_rate = 3e-4f;
    };
    
    std::array<LevelConfig, 5> levels = {{
        // L0: VSE - 24D sensory
        {32, 8, 16, 64, 64, 1.0f, 3.0f, 3e-4f},
        // L1: Intent - 64D
        {64, 16, 32, 128, 128, 1.0f, 3.0f, 3e-4f},
        // L2: Situational - 128D
        {128, 32, 32, 256, 256, 1.0f, 3.0f, 3e-4f},
        // L3: Conceptual - 256D
        {256, 32, 64, 512, 512, 1.0f, 3.0f, 3e-4f},
        // L4: Abstract - 512D
        {512, 64, 64, 1024, 1024, 1.0f, 3.0f, 3e-4f}
    }};
    
    int batch_size = 32;
    int sequence_length = 50;
    float grad_clip = 100.0f;
    int imagination_horizon = 15;
    int imagination_samples = 10;
    float discrete_entropy_coeff = 0.1f;
};

struct RSSMState {
    std::vector<float> deterministic;  // h_t
    std::vector<float> stochastic;     // z_t (continuous)
    std::vector<float> discrete_logits; // logits for categorical
    std::vector<float> discrete_probs;  // gumbel-softmax samples
};

struct JEPALatents {
    std::vector<float> context;  // Context encoder output
    std::vector<float> target;   // Target encoder output
};

struct LatentWorldModelOutput {
    std::array<RSSMState, 5> rssm_states;
    std::array<JEPALatents, 5> jepa_latents;
    std::array<std::vector<float>, 5> predictions;      // Next step predictions per level
    std::array<float, 5> kl_losses;
    std::array<float, 5> jepa_losses;
    std::array<float, 5> reconstruction_losses;
    float total_loss = 0.0f;
};

class LatentWorldModel {
public:
    explicit LatentWorldModel(const LatentWorldModelConfig& cfg = LatentWorldModelConfig{});
    ~LatentWorldModel() = default;

    // Forward pass: process observation through all 5 levels
    LatentWorldModelOutput forward(
        const std::array<std::vector<float>, 5>& observations,
        const std::vector<float>& action = {},
        bool training = true);

    // Imagination: rollout policy in latent space
    struct ImaginationResult {
        std::vector<std::array<RSSMState, 5>> trajectory;
        std::vector<std::array<std::vector<float>, 5>> predicted_obs;
        float cumulative_reward = 0.0f;
    };
    ImaginationResult imagine(
        const std::array<RSSMState, 5>& initial_states,
        const std::vector<std::vector<float>>& policy_actions,
        int horizon = -1) const;

    // Training step with BPTT
    float trainStep(
        const std::vector<std::array<std::vector<float>, 5>>& obs_sequence,
        const std::vector<std::vector<float>>& action_sequence,
        const std::vector<float>& reward_sequence);

    // Get current latent states
    const std::array<RSSMState, 5>& getRSSMStates() const { return rssm_states_; }
    const std::array<JEPALatents, 5>& getJEPALatents() const { return jepa_latents_; }

    // Level-specific encoders/decoders
    std::vector<float> encodeObservation(const std::vector<float>& obs, int level) const;
    std::vector<float> decodeObservation(const std::vector<float>& latent, int level) const;

    // Discrete latent sampling (Gumbel-Softmax)
    std::vector<float> sampleDiscrete(const std::vector<float>& logits, float temperature = 1.0f) const;
    std::vector<float> gumbelSoftmax(const std::vector<float>& logits, float temperature) const;

    // Serialization
    std::string serialize() const;
    bool deserialize(const std::string& data);

    // Config accessor
    const LatentWorldModelConfig& getConfig() const { return config_; }

    // Integration with GenerativeModel
    void setGenerativeModel(inference::GenerativeModel* gm) { generative_model_ = gm; }

private:
    inference::GenerativeModel* generative_model_ = nullptr;
    LatentWorldModelConfig config_;
    std::array<RSSMState, 5> rssm_states_;
    std::array<JEPALatents, 5> jepa_latents_;

    // Per-level networks
    struct LevelNetworks {
        // RSSM: deterministic transition
        std::vector<float> W_hh;   // deterministic_dim x deterministic_dim
        std::vector<float> W_zh;   // deterministic_dim x stochastic_dim
        std::vector<float> W_ah;   // deterministic_dim x action_dim
        std::vector<float> b_h;    // deterministic_dim

        // RSSM: prior (stochastic)
        std::vector<float> W_hz_mu;    // stochastic_dim x deterministic_dim
        std::vector<float> W_hz_logvar; // stochastic_dim x deterministic_dim
        std::vector<float> b_z_mu;
        std::vector<float> b_z_logvar;

        // RSSM: posterior (stochastic)
        std::vector<float> W_hxz_mu;    // stochastic_dim x (deterministic_dim + obs_dim)
        std::vector<float> W_hxz_logvar; // stochastic_dim x (deterministic_dim + obs_dim)
        std::vector<float> b_xz_mu;
        std::vector<float> b_xz_logvar;

        // RSSM: discrete latent
        std::vector<float> W_hd;    // discrete_categories x deterministic_dim
        std::vector<float> b_d;

        // Observation decoder
        std::vector<float> W_hx;    // obs_dim x (deterministic_dim + stochastic_dim + discrete_categories)
        std::vector<float> b_x;

        // JEPA context encoder
        std::vector<float> W_jc;    // jepa_context_dim x obs_dim
        std::vector<float> b_jc;

        // JEPA target encoder
        std::vector<float> W_jt;    // jepa_target_dim x obs_dim
        std::vector<float> b_jt;

        // JEPA predictor
        std::vector<float> W_jp;    // jepa_target_dim x jepa_context_dim
        std::vector<float> b_jp;
    };
    std::array<LevelNetworks, 5> networks_;

    // Optimizer state (Adam)
    struct AdamState {
        std::vector<float> m, v;
        int t = 0;
    };
    std::array<std::vector<AdamState>, 5> optimizer_states_;

    void initializeNetworks();
    void initializeAdamStates();

    // RSSM forward per level
    RSSMState rssmStep(const RSSMState& prev, const std::vector<float>& obs_encoded, 
                       const std::vector<float>& action, int level, bool training);

    // JEPA forward per level
    JEPALatents jepaStep(const std::vector<float>& obs_encoded, int level);

    // Observation encoder/decoder per level
    std::vector<float> encodeObs(const std::vector<float>& obs, int level) const;
    std::vector<float> decodeObs(const std::vector<float>& latent, int level) const;

    // Matrix operations
    std::vector<float> matvec(const std::vector<float>& W, const std::vector<float>& x, 
                              size_t rows, size_t cols) const;
    std::vector<float> matmul(const std::vector<float>& A, const std::vector<float>& B,
                              size_t rows_a, size_t cols_a, size_t cols_b) const;
    void addVec(std::vector<float>& a, const std::vector<float>& b) const;
    void scaleVec(std::vector<float>& a, float s) const;

    // Activation functions
    std::vector<float> relu(const std::vector<float>& x) const;
    std::vector<float> tanh(const std::vector<float>& x) const;
    std::vector<float> sigmoid(const std::vector<float>& x) const;
    std::vector<float> softmax(const std::vector<float>& x) const;

    // KL divergence
    float klDivergence(const std::vector<float>& q_mu, const std::vector<float>& q_logvar,
                       const std::vector<float>& p_mu, const std::vector<float>& p_logvar) const;

    // JEPA loss (MSE between predicted and target latents)
    float jepaLoss(const std::vector<float>& pred, const std::vector<float>& target) const;

    // Gradient clipping
    void clipGradients(std::vector<float>& grad, float max_norm) const;

    // Adam update
    void adamUpdate(std::vector<float>& param, std::vector<float>& grad, AdamState& state, float lr) const;
};

} // namespace predictive
} // namespace brain
} // namespace yuki