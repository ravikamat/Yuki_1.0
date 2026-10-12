#include "brain/language/LatentTokenProjector.h"
#include "brain/core/Logger.h"
#include "brain/predictive/LatentWorldModel.h"
#include "brain/inference/GenerativeModel.h"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <random>
#include <sstream>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

namespace yuki {
namespace brain {
namespace language {

LatentTokenProjector::LatentTokenProjector(const LatentTokenConfig& cfg)
    : config_(cfg) {
    initializeNetworks();
    initializeAdamStates();
    yuki::core::Logger::instance().log(yuki::core::LogLevel::INFO, "LatentTokenProjector",
        "Initialized Latent↔Token Projector (5 levels, vocab=" + std::to_string(config_.vocab_size) + ")");
}

void LatentTokenProjector::initializeNetworks() {
    std::mt19937 rng(42);
    auto init_weight = [&](size_t rows, size_t cols) {
        std::vector<float> W(rows * cols);
        float scale = std::sqrt(2.0f / (rows + cols));
        std::uniform_real_distribution<float> dist(-scale, scale);
        for (float& v : W) v = dist(rng);
        return W;
    };
    auto init_bias = [&](size_t size) {
        return std::vector<float>(size, 0.0f);
    };

    for (int l = 0; l < 5; ++l) {
        auto& proj = projectors_[l];
        size_t latent_dim = 0;
        switch (l) {
            case 0: latent_dim = 32 + 8 + 16; break;  // L0: det + sto + disc
            case 1: latent_dim = 64 + 16 + 32; break; // L1
            case 2: latent_dim = 128 + 32 + 32; break; // L2
            case 3: latent_dim = 256 + 32 + 64; break; // L3
            case 4: latent_dim = 512 + 64 + 64; break; // L4
        }

        // Latent → Hidden (decoder)
        proj.W_latent_to_hidden = init_weight(config_.hidden_dim, latent_dim);
        proj.b_latent_to_hidden = init_bias(config_.hidden_dim);
        
        // Hidden → Vocab
        proj.W_hidden_to_vocab = init_weight(config_.vocab_size, config_.hidden_dim);
        proj.b_hidden_to_vocab = init_bias(config_.vocab_size);
        
        // Token Embedding
        proj.W_token_embedding = init_weight(config_.hidden_dim, config_.vocab_size);
        
        // Token → Hidden (encoder)
        proj.W_token_to_hidden = init_weight(config_.hidden_dim, config_.hidden_dim);
        proj.b_token_to_hidden = init_bias(config_.hidden_dim);
        
        // Hidden → Latent (encoder)
        proj.W_hidden_to_latent = init_weight(latent_dim, config_.hidden_dim);
        proj.b_hidden_to_latent = init_bias(latent_dim);
    }
}

void LatentTokenProjector::initializeAdamStates() {
    for (int l = 0; l < 5; ++l) {
        auto& proj = projectors_[l];
        std::vector<std::vector<float>*> params = {
            &proj.W_latent_to_hidden, &proj.b_latent_to_hidden,
            &proj.W_hidden_to_vocab, &proj.b_hidden_to_vocab,
            &proj.W_token_embedding, &proj.W_token_to_hidden, &proj.b_token_to_hidden,
            &proj.W_hidden_to_latent, &proj.b_hidden_to_latent
        };
        
        optimizer_states_[l].resize(params.size());
        for (size_t i = 0; i < params.size(); ++i) {
            optimizer_states_[l][i].m.assign(params[i]->size(), 0.0f);
            optimizer_states_[l][i].v.assign(params[i]->size(), 0.0f);
            optimizer_states_[l][i].t = 0;
        }
    }
}

ProjectionResult LatentTokenProjector::generateFromLatent(
    const std::array<predictive::RSSMState, 5>& latent_states,
    const std::vector<int>& prompt_tokens,
    int max_length,
    float temperature,
    int top_k,
    float top_p) {
    
    ProjectionResult result;
    result.tokens = prompt_tokens;
    std::vector<int> generated;
    
    // Use STANDARD level (level 1) for generation by default
    int gen_level = 1;
    
    // Concatenate latent states for the generation level
    const auto& rssm = latent_states[gen_level];
    std::vector<float> latent;
    latent.reserve(rssm.deterministic.size() + rssm.stochastic.size() + rssm.discrete_probs.size());
    latent.insert(latent.end(), rssm.deterministic.begin(), rssm.deterministic.end());
    latent.insert(latent.end(), rssm.stochastic.begin(), rssm.stochastic.end());
    latent.insert(latent.end(), rssm.discrete_probs.begin(), rssm.discrete_probs.end());
    
    // Project latent to hidden
    std::vector<float> hidden = latentToHidden(latent, gen_level);
    hidden = gelu(hidden);
    
    // Generate tokens autoregressively
    std::vector<int> context = prompt_tokens;
    float total_confidence = 0.0f;
    float total_surprise = 0.0f;
    float total_entropy = 0.0f;
    
    for (int step = 0; step < max_length; ++step) {
        // Project hidden to vocab logits
        std::vector<float> logits = hiddenToVocab(hidden, gen_level);
        
        // Apply temperature
        for (float& logit : logits) logit /= std::max(temperature, 1e-6f);
        
        // Top-k / Top-p filtering
        std::vector<float> filtered_logits = logits;
        if (top_k > 0 && top_k < (int)logits.size()) {
            std::vector<std::pair<float, int>> indexed;
            indexed.reserve(logits.size());
            for (size_t i = 0; i < logits.size(); ++i) indexed.emplace_back(logits[i], i);
            std::nth_element(indexed.begin(), indexed.begin() + top_k, indexed.end(),
                            [](const auto& a, const auto& b) { return a.first > b.first; });
            float threshold = indexed[top_k].first;
            for (float& v : filtered_logits) if (v < threshold) v = -1e9f;
        }
        if (top_p < 1.0f) {
            auto probs = softmax(filtered_logits);
            std::vector<std::pair<float, int>> indexed;
            for (size_t i = 0; i < probs.size(); ++i) indexed.emplace_back(probs[i], i);
            std::sort(indexed.begin(), indexed.end(),
                     [](const auto& a, const auto& b) { return a.first > b.first; });
            float cumsum = 0.0f;
            size_t cutoff = 0;
            for (size_t i = 0; i < indexed.size(); ++i) {
                cumsum += indexed[i].first;
                if (cumsum >= top_p) { cutoff = i + 1; break; }
            }
            for (size_t i = cutoff; i < indexed.size(); ++i) {
                filtered_logits[indexed[i].second] = -1e9f;
            }
        }
        
        // Compute probabilities for confidence/entropy
        auto probs = softmax(filtered_logits);
        float entropy = 0.0f;
        float max_prob = 0.0f;
        for (float p : probs) {
            if (p > 1e-8f) entropy -= p * std::log(p);
            max_prob = std::max(max_prob, p);
        }
        
        // Sample token
        int token = sampleToken(filtered_logits, temperature, top_k, top_p);
        generated.push_back(token);
        result.tokens.push_back(token);
        result.logits.insert(result.logits.end(), filtered_logits.begin(), filtered_logits.end());
        result.confidence.push_back(max_prob);
        
        total_confidence += max_prob;
        total_entropy += entropy;
        
        // Update context for next step
        context.push_back(token);
        
        // Update hidden state with new token embedding
        if (token < (int)projectors_[gen_level].W_token_embedding.size() / config_.hidden_dim) {
            std::vector<float> token_emb(config_.hidden_dim);
            for (size_t i = 0; i < config_.hidden_dim; ++i) {
                token_emb[i] = projectors_[gen_level].W_token_embedding[token * config_.hidden_dim + i];
            }
            // Simple residual update
            for (size_t i = 0; i < config_.hidden_dim; ++i) {
                hidden[i] = 0.9f * hidden[i] + 0.1f * token_emb[i];
            }
        }
        
        // Check for EOS
        if (token == 0) break; // Assuming 0 is EOS
    }
    
    // Compute surprise from LWM if available
    if (latent_world_model_) {
        // Simplified: use entropy as proxy for surprise
        result.surprise = total_entropy / std::max(1, (int)generated.size());
    }
    result.entropy = total_entropy / std::max(1, (int)generated.size());
    result.sequence_confidence = total_confidence / std::max(1, (int)generated.size());
    
    // Uncertainty check
    UncertaintyMetrics metrics;
    metrics.token_entropy = result.entropy;
    metrics.sequence_surprise = result.surprise;
    metrics.avg_confidence = result.sequence_confidence;
    metrics.max_surprise = result.surprise;
    metrics.low_confidence_count = std::count_if(result.confidence.begin(), result.confidence.end(),
                                                  [this](float c) { return c < config_.confidence_floor; });
    
    result.needs_clarification = shouldClarify(metrics);
    if (result.needs_clarification) {
        result.clarification_reason = getClarificationPrompt(metrics);
    }
    
    return result;
}

std::vector<ProjectionResult> LatentTokenProjector::generateBeam(
    const std::array<predictive::RSSMState, 5>& latent_states,
    const std::vector<int>& prompt_tokens,
    int beam_width,
    int max_length) {
    
    std::vector<ProjectionResult> beams;
    // Simplified: return single beam for now
    beams.push_back(generateFromLatent(latent_states, prompt_tokens, max_length));
    return beams;
}

LatentTokenProjector::StreamState LatentTokenProjector::beginStream(
    const std::array<predictive::RSSMState, 5>& latent_states,
    const std::vector<int>& prompt_tokens) {
    
    StreamState state;
    state.generated_tokens = prompt_tokens;
    // Would maintain latent state for streaming
    return state;
}

bool LatentTokenProjector::stepStream(StreamState& state, int& next_token) {
    // Simplified streaming
    return false;
}

std::array<predictive::RSSMState, 5> LatentTokenProjector::encodeToLatent(
    const std::vector<int>& tokens,
    const std::vector<float>& context_embedding) {
    
    std::array<predictive::RSSMState, 5> result;
    // Simplified: just encode at STANDARD level
    int level = 1;
    std::vector<float> hidden = tokenToHidden(tokens, level);
    hidden = gelu(hidden);
    std::vector<float> latent = hiddenToLatent(hidden, level);
    
    // Fill result with zeros and set level 1
    for (int l = 0; l < 5; ++l) {
        size_t det_dim = (l == 0) ? 32 : (l == 1 ? 64 : (l == 2 ? 128 : (l == 3 ? 256 : 512)));
        size_t sto_dim = (l == 0) ? 8 : (l == 1 ? 16 : (l == 2 ? 32 : (l == 3 ? 32 : 64)));
        size_t disc_dim = (l == 0) ? 16 : (l == 1 ? 32 : (l == 2 ? 32 : (l == 3 ? 64 : 64)));
        
        result[l].deterministic.assign(det_dim, 0.0f);
        result[l].stochastic.assign(sto_dim, 0.0f);
        result[l].discrete_logits.assign(disc_dim, 0.0f);
        result[l].discrete_probs.assign(disc_dim, 0.0f);
    }
    
    if (latent.size() >= 64 + 16 + 32) {
        result[1].deterministic.assign(latent.begin(), latent.begin() + 64);
        result[1].stochastic.assign(latent.begin() + 64, latent.begin() + 64 + 16);
        result[1].discrete_probs.assign(latent.begin() + 64 + 16, latent.begin() + 64 + 16 + 32);
    }
    
    return result;
}

VerificationResult LatentTokenProjector::verifyAgainstLWM(
    const std::vector<int>& tokens,
    const std::array<predictive::RSSMState, 5>& latent_states) {
    
    VerificationResult result;
    result.score = computeVerificationScore(tokens, latent_states);
    result.passed = result.score >= config_.verification_threshold;
    
    if (!result.passed) {
        result.failure_reason = "Verification score " + std::to_string(result.score) + 
                               " below threshold " + std::to_string(config_.verification_threshold);
    }
    return result;
}

LatentTokenProjector::UncertaintyMetrics LatentTokenProjector::computeUncertainty(
    const std::vector<int>& tokens,
    const std::array<predictive::RSSMState, 5>& latent_states) {
    
    UncertaintyMetrics metrics;
    // Simplified: use entropy of token distribution
    metrics.token_entropy = 0.5f;
    metrics.sequence_surprise = 0.3f;
    metrics.avg_confidence = 0.7f;
    metrics.max_surprise = 0.5f;
    metrics.low_confidence_count = 0;
    return metrics;
}

bool LatentTokenProjector::shouldClarify(const UncertaintyMetrics& metrics) const {
    return metrics.sequence_surprise > config_.surprise_threshold ||
           metrics.token_entropy > config_.entropy_threshold ||
           metrics.avg_confidence < config_.confidence_floor;
}

std::string LatentTokenProjector::getClarificationPrompt(const UncertaintyMetrics& metrics) const {
    if (metrics.sequence_surprise > config_.surprise_threshold) {
        return "I'm uncertain about this - the situation is surprising. Could you clarify?";
    }
    if (metrics.token_entropy > config_.entropy_threshold) {
        return "I'm not confident in my response. Could you rephrase?";
    }
    if (metrics.avg_confidence < config_.confidence_floor) {
        return "My confidence is low. What specifically would you like to know?";
    }
    return "Could you clarify what you mean?";
}

float LatentTokenProjector::trainStep(const std::array<predictive::RSSMState, 5>& latent,
                                       const std::vector<int>& tokens) {
    // Simplified training step
    return 0.0f;
}

float LatentTokenProjector::contrastiveStep(const std::vector<std::pair<
    std::array<predictive::RSSMState, 5>, std::vector<int>>>& pairs) {
    return 0.0f;
}

std::string LatentTokenProjector::serialize() const {
    std::ostringstream oss;
    oss << "LatentTokenProjector";
    return oss.str();
}

bool LatentTokenProjector::deserialize(const std::string& data) {
    return true;
}

std::vector<float> LatentTokenProjector::latentToHidden(const std::vector<float>& latent, int level) const {
    const auto& proj = projectors_[level];
    return matvec(proj.W_latent_to_hidden, latent, config_.hidden_dim, latent.size());
}

std::vector<float> LatentTokenProjector::hiddenToVocab(const std::vector<float>& hidden, int level) const {
    const auto& proj = projectors_[level];
    return matvec(proj.W_hidden_to_vocab, hidden, config_.vocab_size, config_.hidden_dim);
}

std::vector<float> LatentTokenProjector::tokenToHidden(const std::vector<int>& tokens, int level) const {
    const auto& proj = projectors_[level];
    std::vector<float> result(config_.hidden_dim, 0.0f);
    for (int token : tokens) {
        if (token >= 0 && token < (int)config_.vocab_size) {
            for (size_t i = 0; i < config_.hidden_dim; ++i) {
                result[i] += proj.W_token_embedding[token * config_.hidden_dim + i];
            }
        }
    }
    return result;
}

std::vector<float> LatentTokenProjector::hiddenToLatent(const std::vector<float>& hidden, int level) const {
    const auto& proj = projectors_[level];
    size_t latent_dim = 0;
    switch (level) {
        case 0: latent_dim = 32 + 8 + 16; break;
        case 1: latent_dim = 64 + 16 + 32; break;
        case 2: latent_dim = 128 + 32 + 32; break;
        case 3: latent_dim = 256 + 32 + 64; break;
        case 4: latent_dim = 512 + 64 + 64; break;
    }
    return matvec(proj.W_hidden_to_latent, hidden, latent_dim, config_.hidden_dim);
}

int LatentTokenProjector::sampleToken(const std::vector<float>& logits, float temperature,
                                       int top_k, float top_p) const {
    auto probs = softmax(logits);
    static thread_local std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    float r = dist(rng);
    float cumsum = 0.0f;
    for (size_t i = 0; i < probs.size(); ++i) {
        cumsum += probs[i];
        if (r <= cumsum) return static_cast<int>(i);
    }
    return 0;
}

std::vector<float> LatentTokenProjector::softmax(const std::vector<float>& logits) const {
    std::vector<float> result(logits.size());
    float max_logit = *std::max_element(logits.begin(), logits.end());
    float sum = 0.0f;
    for (size_t i = 0; i < logits.size(); ++i) {
        result[i] = std::exp(logits[i] - max_logit);
        sum += result[i];
    }
    for (float& v : result) v /= (sum + 1e-8f);
    return result;
}

float LatentTokenProjector::computeVerificationScore(const std::vector<int>& tokens,
                                                      const std::array<predictive::RSSMState, 5>& latent) const {
    // Simplified: return fixed score
    return 0.8f;
}

std::vector<float> LatentTokenProjector::matvec(const std::vector<float>& W, const std::vector<float>& x,
                                                size_t rows, size_t cols) const {
    std::vector<float> result(rows, 0.0f);
    for (size_t i = 0; i < rows; ++i) {
        float sum = 0.0f;
        for (size_t j = 0; j < cols && j < x.size(); ++j) {
            sum += W[i * cols + j] * x[j];
        }
        result[i] = sum;
    }
    return result;
}

std::vector<float> LatentTokenProjector::relu(const std::vector<float>& x) const {
    std::vector<float> result(x.size());
    for (size_t i = 0; i < x.size(); ++i) result[i] = std::max(0.0f, x[i]);
    return result;
}

std::vector<float> LatentTokenProjector::gelu(const std::vector<float>& x) const {
    std::vector<float> result(x.size());
    for (size_t i = 0; i < x.size(); ++i) {
        float x_i = x[i];
        result[i] = 0.5f * x_i * (1.0f + std::tanh(std::sqrt(2.0f / M_PI) * (x_i + 0.044715f * x_i * x_i * x_i)));
    }
    return result;
}

std::vector<float> LatentTokenProjector::dropout(const std::vector<float>& x, float rate) const {
    if (rate <= 0.0f) return x;
    std::vector<float> result(x.size());
    static thread_local std::mt19937 rng(std::random_device{}());
    std::bernoulli_distribution keep(1.0f - rate);
    float scale = 1.0f / (1.0f - rate);
    for (size_t i = 0; i < x.size(); ++i) {
        result[i] = keep(rng) ? x[i] * scale : 0.0f;
    }
    return result;
}

void LatentTokenProjector::adamUpdate(std::vector<float>& param, std::vector<float>& grad,
                                       AdamState& state, float lr) const {
    state.t++;
    float beta1 = 0.9f, beta2 = 0.999f, eps = 1e-8f;
    for (size_t i = 0; i < param.size(); ++i) {
        state.m[i] = beta1 * state.m[i] + (1.0f - beta1) * grad[i];
        state.v[i] = beta2 * state.v[i] + (1.0f - beta2) * grad[i] * grad[i];
        float m_hat = state.m[i] / (1.0f - std::pow(beta1, state.t));
        float v_hat = state.v[i] / (1.0f - std::pow(beta2, state.t));
        param[i] -= lr * m_hat / (std::sqrt(v_hat) + eps);
    }
}

} // namespace language
} // namespace brain
} // namespace yuki