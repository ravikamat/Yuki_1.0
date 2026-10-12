#pragma once
// LatentTokenProjector — Bidirectional Latent ↔ Token projection
// Enables grounded language generation from latent space and vice versa

#include <vector>
#include <string>
#include <array>
#include <memory>
#include <unordered_map>
#include "brain/predictive/LatentWorldModel.h"
#include "brain/inference/GenerativeModel.h"

namespace yuki {
namespace brain {
namespace language {

struct LatentTokenConfig {
    // Projection dimensions
    size_t latent_dim = 512;        // STANDARD manifold dimension
    size_t vocab_size = 50000;      // Token vocabulary size
    size_t hidden_dim = 1024;       // Projector hidden dimension
    size_t num_layers = 3;          // Projector depth
    
    // Training
    float learning_rate = 3e-4f;
    float dropout = 0.1f;
    float label_smoothing = 0.1f;
    
    // Uncertainty handling
    float surprise_threshold = 1.5f;      // High surprise → clarify
    float entropy_threshold = 0.8f;       // High entropy → clarify
    float confidence_floor = 0.1f;        // Minimum generation confidence
    
    // Verification
    bool verify_against_lwm = true;       // Verify generated tokens against LWM
    int verification_beam_width = 4;      // Beam search for verification
    float verification_threshold = 0.7f;  // Minimum verification score
};

struct ProjectionResult {
    std::vector<int> tokens;              // Generated token IDs
    std::vector<float> logits;            // Logits per token
    std::vector<float> confidence;        // Per-token confidence
    float sequence_confidence = 0.0f;     // Overall sequence confidence
    float surprise = 0.0f;                // LWM surprise during generation
    float entropy = 0.0f;                 // Generation entropy
    bool needs_clarification = false;     // True if uncertainty too high
    std::string clarification_reason;     // Why clarification needed
};

struct VerificationResult {
    bool passed = false;
    float score = 0.0f;                   // Verification score [0,1]
    std::vector<float> token_scores;      // Per-token verification scores
    std::vector<int> corrected_tokens;    // Suggested corrections
    std::string failure_reason;           // Why verification failed
};

class LatentTokenProjector {
public:
    explicit LatentTokenProjector(const LatentTokenConfig& cfg = LatentTokenConfig{});
    ~LatentTokenProjector() = default;
    
    void setLatentWorldModel(predictive::LatentWorldModel* lwm) { latent_world_model_ = lwm; }
    void setGenerativeModel(inference::GenerativeModel* gm) { generative_model_ = gm; }
    void setVocabulary(const std::unordered_map<std::string, int>& vocab, 
                       const std::vector<std::string>& id_to_token);
    
    // ─── Latent → Token (Generation) ────────────────────────────────────────
    
    // Generate token sequence from latent state with uncertainty quantification
    ProjectionResult generateFromLatent(
        const std::array<predictive::RSSMState, 5>& latent_states,
        const std::vector<int>& prompt_tokens = {},
        int max_length = 128,
        float temperature = 0.8f,
        int top_k = 40,
        float top_p = 0.9f);
    
    // Generate with beam search for higher quality
    std::vector<ProjectionResult> generateBeam(
        const std::array<predictive::RSSMState, 5>& latent_states,
        const std::vector<int>& prompt_tokens,
        int beam_width = 4,
        int max_length = 128);
    
    // Streaming generation (token by token for real-time)
    struct StreamState {
        std::vector<int> generated_tokens;
        std::array<predictive::RSSMState, 5> current_latent;
        std::vector<float> next_logits;
        bool done = false;
    };
    StreamState beginStream(const std::array<predictive::RSSMState, 5>& latent_states,
                            const std::vector<int>& prompt_tokens);
    bool stepStream(StreamState& state, int& next_token);
    
    // ─── Token → Latent (Comprehension) ─────────────────────────────────────
    
    // Encode token sequence into latent space
    std::array<predictive::RSSMState, 5> encodeToLatent(
        const std::vector<int>& tokens,
        const std::vector<float>& context_embedding = {});
    
    // ─── Verification & Uncertainty ─────────────────────────────────────────
    
    // Verify generated sequence against LWM predictions
    VerificationResult verifyAgainstLWM(
        const std::vector<int>& tokens,
        const std::array<predictive::RSSMState, 5>& latent_states);
    
    // Compute uncertainty metrics for a token sequence
    struct UncertaintyMetrics {
        float token_entropy = 0.0f;
        float sequence_surprise = 0.0f;
        float avg_confidence = 0.0f;
        float max_surprise = 0.0f;
        int low_confidence_count = 0;
    };
    UncertaintyMetrics computeUncertainty(
        const std::vector<int>& tokens,
        const std::array<predictive::RSSMState, 5>& latent_states);
    
    // Determine if clarification is needed
    bool shouldClarify(const UncertaintyMetrics& metrics) const;
    std::string getClarificationPrompt(const UncertaintyMetrics& metrics) const;
    
    // ─── Training / Adaptation ──────────────────────────────────────────────
    
    // Update projector from (latent, token) pairs
    float trainStep(const std::array<predictive::RSSMState, 5>& latent,
                    const std::vector<int>& tokens);
    
    // Contrastive learning: align latent and token representations
    float contrastiveStep(const std::vector<std::pair<
        std::array<predictive::RSSMState, 5>, std::vector<int>>>& pairs);
    
    // ─── Serialization ──────────────────────────────────────────────────────
    
    std::string serialize() const;
    bool deserialize(const std::string& data);

private:
    LatentTokenConfig config_;
    predictive::LatentWorldModel* latent_world_model_ = nullptr;
    inference::GenerativeModel* generative_model_ = nullptr;
    
    // Vocabulary
    std::unordered_map<std::string, int> token_to_id_;
    std::vector<std::string> id_to_token_;
    
    // Projector networks (latent ↔ token)
    struct ProjectorNetwork {
        // Latent → Token (decoder)
        std::vector<float> W_latent_to_hidden;
        std::vector<float> b_latent_to_hidden;
        std::vector<float> W_hidden_to_vocab;
        std::vector<float> b_hidden_to_vocab;
        
        // Token → Latent (encoder)
        std::vector<float> W_token_embedding;
        std::vector<float> W_token_to_hidden;
        std::vector<float> b_token_to_hidden;
        std::vector<float> W_hidden_to_latent;
        std::vector<float> b_hidden_to_latent;
    };
    std::array<ProjectorNetwork, 5> projectors_;  // Per-level projectors
    
    // Optimizer state (Adam)
    struct AdamState { std::vector<float> m, v; int t = 0; };
    std::array<std::vector<AdamState>, 5> optimizer_states_;
    
    void initializeNetworks();
    void initializeAdamStates();
    
    // Forward pass helpers
    std::vector<float> latentToHidden(const std::vector<float>& latent, int level) const;
    std::vector<float> hiddenToVocab(const std::vector<float>& hidden, int level) const;
    std::vector<float> tokenToHidden(const std::vector<int>& tokens, int level) const;
    std::vector<float> hiddenToLatent(const std::vector<float>& hidden, int level) const;
    
    // Sampling
    int sampleToken(const std::vector<float>& logits, float temperature, int top_k, float top_p) const;
    std::vector<float> softmax(const std::vector<float>& logits) const;
    
    // Verification against LWM
    float computeVerificationScore(const std::vector<int>& tokens,
                                    const std::array<predictive::RSSMState, 5>& latent) const;
    
    // Matrix operations
    std::vector<float> matvec(const std::vector<float>& W, const std::vector<float>& x,
                              size_t rows, size_t cols) const;
    std::vector<float> relu(const std::vector<float>& x) const;
    std::vector<float> gelu(const std::vector<float>& x) const;
    std::vector<float> dropout(const std::vector<float>& x, float rate) const;
    
    // Adam optimizer
    void adamUpdate(std::vector<float>& param, std::vector<float>& grad,
                    AdamState& state, float lr) const;
};

} // namespace language
} // namespace brain
} // namespace yuki