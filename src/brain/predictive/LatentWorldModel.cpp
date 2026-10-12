#include "brain/predictive/LatentWorldModel.h"
#include "brain/core/Logger.h"
#include <numeric>
#include <random>
#include <sstream>

namespace yuki {
namespace brain {
namespace predictive {

LatentWorldModel::LatentWorldModel(const LatentWorldModelConfig& cfg)
    : config_(cfg) {
    initializeNetworks();
    initializeAdamStates();
    
    // Initialize RSSM states
    for (int l = 0; l < 5; ++l) {
        rssm_states_[l].deterministic.assign(config_.levels[l].deterministic_dim, 0.0f);
        rssm_states_[l].stochastic.assign(config_.levels[l].stochastic_dim, 0.0f);
        rssm_states_[l].discrete_logits.assign(config_.levels[l].discrete_categories, 0.0f);
        rssm_states_[l].discrete_probs.assign(config_.levels[l].discrete_categories, 0.0f);
    }
    
    yuki::core::Logger::instance().log(yuki::core::LogLevel::INFO, "LatentWorldModel", 
        "Initialized 5-level hierarchical RSSM+JEPA");
}

void LatentWorldModel::initializeNetworks() {
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
        const auto& lc = config_.levels[l];
        size_t det_dim = lc.deterministic_dim;
        size_t sto_dim = lc.stochastic_dim;
        size_t disc_dim = lc.discrete_categories;
        size_t jepa_ctx = lc.jepa_context_dim;
        size_t jepa_tgt = lc.jepa_target_dim;
        size_t obs_dim = (l == 0) ? 24 : (l == 1 ? 64 : (l == 2 ? 128 : (l == 3 ? 256 : 512)));
        size_t act_dim = 12;

        auto& net = networks_[l];

        // RSSM deterministic transition
        net.W_hh = init_weight(det_dim, det_dim);
        net.W_zh = init_weight(det_dim, sto_dim + disc_dim);
        net.W_ah = init_weight(det_dim, act_dim);
        net.b_h = init_bias(det_dim);

        // RSSM prior
        net.W_hz_mu = init_weight(sto_dim, det_dim);
        net.W_hz_logvar = init_weight(sto_dim, det_dim);
        net.b_z_mu = init_bias(sto_dim);
        net.b_z_logvar = init_bias(sto_dim);

        // RSSM posterior
        net.W_hxz_mu = init_weight(sto_dim, det_dim + obs_dim);
        net.W_hxz_logvar = init_weight(sto_dim, det_dim + obs_dim);
        net.b_xz_mu = init_bias(sto_dim);
        net.b_xz_logvar = init_bias(sto_dim);

        // RSSM discrete
        net.W_hd = init_weight(disc_dim, det_dim);
        net.b_d = init_bias(disc_dim);

        // Observation decoder
        net.W_hx = init_weight(obs_dim, det_dim + sto_dim + disc_dim);
        net.b_x = init_bias(obs_dim);

        // JEPA context encoder
        net.W_jc = init_weight(jepa_ctx, obs_dim);
        net.b_jc = init_bias(jepa_ctx);

        // JEPA target encoder
        net.W_jt = init_weight(jepa_tgt, obs_dim);
        net.b_jt = init_bias(jepa_tgt);

        // JEPA predictor
        net.W_jp = init_weight(jepa_tgt, jepa_ctx);
        net.b_jp = init_bias(jepa_tgt);
    }
}

void LatentWorldModel::initializeAdamStates() {
    for (int l = 0; l < 5; ++l) {
        auto& net = networks_[l];
        std::vector<std::vector<float>*> params = {
            &net.W_hh, &net.W_zh, &net.W_ah, &net.b_h,
            &net.W_hz_mu, &net.W_hz_logvar, &net.b_z_mu, &net.b_z_logvar,
            &net.W_hxz_mu, &net.W_hxz_logvar, &net.b_xz_mu, &net.b_xz_logvar,
            &net.W_hd, &net.b_d,
            &net.W_hx, &net.b_x,
            &net.W_jc, &net.b_jc,
            &net.W_jt, &net.b_jt,
            &net.W_jp, &net.b_jp
        };
        
        optimizer_states_[l].resize(params.size());
        for (size_t i = 0; i < params.size(); ++i) {
            optimizer_states_[l][i].m.assign(params[i]->size(), 0.0f);
            optimizer_states_[l][i].v.assign(params[i]->size(), 0.0f);
            optimizer_states_[l][i].t = 0;
        }
    }
}

LatentWorldModelOutput LatentWorldModel::forward(
    const std::array<std::vector<float>, 5>& observations,
    const std::vector<float>& action,
    bool training) {
    
    LatentWorldModelOutput output;
    std::vector<float> act = action.empty() ? std::vector<float>(12, 0.0f) : action;

    for (int l = 0; l < 5; ++l) {
        // Encode observation
        std::vector<float> obs_enc = encodeObs(observations[l], l);
        
        // RSSM step
        rssm_states_[l] = rssmStep(rssm_states_[l], obs_enc, act, l, training);
        
        // JEPA step
        jepa_latents_[l] = jepaStep(obs_enc, l);
        
        // Decode/predict next observation
        std::vector<float> latent_concat;
        latent_concat.reserve(config_.levels[l].deterministic_dim + 
                             config_.levels[l].stochastic_dim + 
                             config_.levels[l].discrete_categories);
        latent_concat.insert(latent_concat.end(), rssm_states_[l].deterministic.begin(), rssm_states_[l].deterministic.end());
        latent_concat.insert(latent_concat.end(), rssm_states_[l].stochastic.begin(), rssm_states_[l].stochastic.end());
        latent_concat.insert(latent_concat.end(), rssm_states_[l].discrete_probs.begin(), rssm_states_[l].discrete_probs.end());
        
        output.predictions[l] = decodeObs(latent_concat, l);
        
        // Store states
        output.rssm_states[l] = rssm_states_[l];
        output.jepa_latents[l] = jepa_latents_[l];
    }

    // Compute losses if training
    if (training) {
        output.total_loss = 0.0f;
        for (int l = 0; l < 5; ++l) {
            // Reconstruction loss (simplified MSE)
            float recon_loss = 0.0f;
            for (size_t i = 0; i < output.predictions[l].size() && i < observations[l].size(); ++i) {
                float diff = output.predictions[l][i] - observations[l][i];
                recon_loss += diff * diff;
            }
            recon_loss /= std::max<size_t>(1, output.predictions[l].size());
            output.reconstruction_losses[l] = recon_loss;

            // KL loss (prior vs posterior) - simplified
            output.kl_losses[l] = 0.0f; // Would compute from prior/posterior params

            // JEPA loss
            output.jepa_losses[l] = jepaLoss(jepa_latents_[l].context, jepa_latents_[l].target);

            output.total_loss += output.reconstruction_losses[l] 
                               + config_.levels[l].kl_weight * output.kl_losses[l]
                               + 0.5f * output.jepa_losses[l];
        }
    }

    return output;
}

RSSMState LatentWorldModel::rssmStep(const RSSMState& prev, const std::vector<float>& obs_encoded,
                                      const std::vector<float>& action, int level, bool training) {
    const auto& lc = config_.levels[level];
    const auto& net = networks_[level];
    size_t det_dim = lc.deterministic_dim;
    size_t sto_dim = lc.stochastic_dim;
    size_t disc_dim = lc.discrete_categories;
    size_t obs_dim = obs_encoded.size();

    RSSMState next;
    next.deterministic.resize(det_dim);
    next.stochastic.resize(sto_dim);
    next.discrete_logits.resize(disc_dim);
    next.discrete_probs.resize(disc_dim);

    // Deterministic transition: h_t = f(h_{t-1}, z_{t-1}, d_{t-1}, a_{t-1})
    // Concatenate previous stochastic and discrete
    std::vector<float> z_concat;
    z_concat.reserve(sto_dim + disc_dim);
    z_concat.insert(z_concat.end(), prev.stochastic.begin(), prev.stochastic.end());
    z_concat.insert(z_concat.end(), prev.discrete_probs.begin(), prev.discrete_probs.end());

    // h_t = tanh(W_hh * h_{t-1} + W_zh * [z_{t-1}; d_{t-1}] + W_ah * a_{t-1} + b_h)
    std::vector<float> h_term = matvec(net.W_hh, prev.deterministic, det_dim, det_dim);
    std::vector<float> zh_term = matvec(net.W_zh, z_concat, det_dim, sto_dim + disc_dim);
    std::vector<float> ah_term = matvec(net.W_ah, action, det_dim, action.size());
    
    for (size_t i = 0; i < det_dim; ++i) {
        next.deterministic[i] = h_term[i] + zh_term[i] + ah_term[i] + net.b_h[i];
    }
    next.deterministic = tanh(next.deterministic);

    // Prior: p(z_t | h_t)
    std::vector<float> p_mu = matvec(net.W_hz_mu, next.deterministic, sto_dim, det_dim);
    std::vector<float> p_logvar = matvec(net.W_hz_logvar, next.deterministic, sto_dim, det_dim);
    for (size_t i = 0; i < sto_dim; ++i) {
        p_mu[i] += net.b_z_mu[i];
        p_logvar[i] += net.b_z_logvar[i];
    }

    // Posterior: q(z_t | h_t, x_t)
    std::vector<float> hx_concat;
    hx_concat.reserve(det_dim + obs_dim);
    hx_concat.insert(hx_concat.end(), next.deterministic.begin(), next.deterministic.end());
    hx_concat.insert(hx_concat.end(), obs_encoded.begin(), obs_encoded.end());

    std::vector<float> q_mu = matvec(net.W_hxz_mu, hx_concat, sto_dim, det_dim + obs_dim);
    std::vector<float> q_logvar = matvec(net.W_hxz_logvar, hx_concat, sto_dim, det_dim + obs_dim);
    for (size_t i = 0; i < sto_dim; ++i) {
        q_mu[i] += net.b_xz_mu[i];
        q_logvar[i] += net.b_xz_logvar[i];
    }

    // Sample from posterior (reparameterization trick)
    if (training) {
        static thread_local std::mt19937 rng(std::random_device{}());
        std::normal_distribution<float> dist(0.0f, 1.0f);
        for (size_t i = 0; i < sto_dim; ++i) {
            float std = std::exp(0.5f * q_logvar[i]);
            next.stochastic[i] = q_mu[i] + std * dist(rng);
        }
    } else {
        next.stochastic = q_mu;
    }

    // Discrete latent: logits = W_hd * h_t + b_d
    next.discrete_logits = matvec(net.W_hd, next.deterministic, disc_dim, det_dim);
    for (size_t i = 0; i < disc_dim; ++i) {
        next.discrete_logits[i] += net.b_d[i];
    }

    // Gumbel-Softmax for discrete sampling
    next.discrete_probs = gumbelSoftmax(next.discrete_logits, training ? 1.0f : 0.1f);

    return next;
}

JEPALatents LatentWorldModel::jepaStep(const std::vector<float>& obs_encoded, int level) {
    const auto& lc = config_.levels[level];
    const auto& net = networks_[level];
    
    JEPALatents result;
    result.context.resize(lc.jepa_context_dim);
    result.target.resize(lc.jepa_target_dim);

    // Context encoder
    result.context = matvec(net.W_jc, obs_encoded, lc.jepa_context_dim, obs_encoded.size());
    for (size_t i = 0; i < lc.jepa_context_dim; ++i) {
        result.context[i] += net.b_jc[i];
    }
    result.context = relu(result.context);

    // Target encoder (for JEPA, we encode a future/masked observation)
    // For now, use same observation (in training, this would be a future frame)
    result.target = matvec(net.W_jt, obs_encoded, lc.jepa_target_dim, obs_encoded.size());
    for (size_t i = 0; i < lc.jepa_target_dim; ++i) {
        result.target[i] += net.b_jt[i];
    }
    result.target = relu(result.target);

    return result;
}

std::vector<float> LatentWorldModel::encodeObs(const std::vector<float>& obs, int level) const {
    // Simple linear projection for now - could be MLP
    size_t target_dim = (level == 0) ? 24 : (level == 1 ? 64 : (level == 2 ? 128 : (level == 3 ? 256 : 512)));
    std::vector<float> result(target_dim, 0.0f);
    size_t copy_size = std::min(obs.size(), target_dim);
    for (size_t i = 0; i < copy_size; ++i) {
        result[i] = obs[i];
    }
    return result;
}

std::vector<float> LatentWorldModel::decodeObs(const std::vector<float>& latent, int level) const {
    const auto& net = networks_[level];
    size_t obs_dim = (level == 0) ? 24 : (level == 1 ? 64 : (level == 2 ? 128 : (level == 3 ? 256 : 512)));
    
    std::vector<float> result = matvec(net.W_hx, latent, obs_dim, latent.size());
    for (size_t i = 0; i < obs_dim; ++i) {
        result[i] += net.b_x[i];
    }
    return result;
}

std::vector<float> LatentWorldModel::sampleDiscrete(const std::vector<float>& logits, float temperature) const {
    return gumbelSoftmax(logits, temperature);
}

std::vector<float> LatentWorldModel::gumbelSoftmax(const std::vector<float>& logits, float temperature) const {
    static thread_local std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<float> uniform(0.0f, 1.0f);
    std::vector<float> result(logits.size());

    // Sample Gumbel noise
    std::vector<float> gumbel(logits.size());
    for (float& g : gumbel) {
        float u = uniform(rng);
        g = -std::log(-std::log(u + 1e-8f) + 1e-8f);
    }

    // Add noise and apply temperature
    std::vector<float> scaled(logits.size());
    float max_logit = *std::max_element(logits.begin(), logits.end());
    for (size_t i = 0; i < logits.size(); ++i) {
        scaled[i] = (logits[i] + gumbel[i]) / temperature;
        scaled[i] -= max_logit;
    }

    // Softmax
    float sum = 0.0f;
    for (float& v : scaled) {
        v = std::exp(v);
        sum += v;
    }
    for (float& v : scaled) {
        v /= (sum + 1e-8f);
    }

    return scaled;
}

LatentWorldModel::ImaginationResult LatentWorldModel::imagine(
    const std::array<RSSMState, 5>& initial_states,
    const std::vector<std::vector<float>>& policy_actions,
    int horizon) const {
    
    ImaginationResult result;
    int h = (horizon > 0) ? horizon : config_.imagination_horizon;
    
    std::array<RSSMState, 5> current_states = initial_states;
    
    for (int step = 0; step < h && step < (int)policy_actions.size(); ++step) {
        const auto& action = policy_actions[step];
        std::array<RSSMState, 5> next_states;
        std::array<std::vector<float>, 5> predicted_obs;
        
        for (int l = 0; l < 5; ++l) {
            // Use prior for imagination (no observation)
            // Simplified: just use deterministic transition
            const auto& lc = config_.levels[l];
            const auto& net = networks_[l];
            
            next_states[l].deterministic.resize(lc.deterministic_dim);
            next_states[l].stochastic.resize(lc.stochastic_dim);
            next_states[l].discrete_logits.resize(lc.discrete_categories);
            next_states[l].discrete_probs.resize(lc.discrete_categories);
            
            // h_t = f(h_{t-1}, z_{t-1}, d_{t-1}, a_t)
            std::vector<float> z_concat;
            z_concat.reserve(lc.stochastic_dim + lc.discrete_categories);
            z_concat.insert(z_concat.end(), current_states[l].stochastic.begin(), current_states[l].stochastic.end());
            z_concat.insert(z_concat.end(), current_states[l].discrete_probs.begin(), current_states[l].discrete_probs.end());
            
            std::vector<float> h_term = matvec(net.W_hh, current_states[l].deterministic, lc.deterministic_dim, lc.deterministic_dim);
            std::vector<float> zh_term = matvec(net.W_zh, z_concat, lc.deterministic_dim, lc.stochastic_dim + lc.discrete_categories);
            std::vector<float> ah_term = matvec(net.W_ah, action, lc.deterministic_dim, action.size());
            
            for (size_t i = 0; i < lc.deterministic_dim; ++i) {
                next_states[l].deterministic[i] = h_term[i] + zh_term[i] + ah_term[i] + net.b_h[i];
            }
            next_states[l].deterministic = tanh(next_states[l].deterministic);
            
            // Sample from prior
            std::vector<float> p_mu = matvec(net.W_hz_mu, next_states[l].deterministic, lc.stochastic_dim, lc.deterministic_dim);
            std::vector<float> p_logvar = matvec(net.W_hz_logvar, next_states[l].deterministic, lc.stochastic_dim, lc.deterministic_dim);
            for (size_t i = 0; i < lc.stochastic_dim; ++i) {
                p_mu[i] += net.b_z_mu[i];
                p_logvar[i] += net.b_z_logvar[i];
                next_states[l].stochastic[i] = p_mu[i]; // Mean for imagination
            }
            
            // Discrete
            next_states[l].discrete_logits = matvec(net.W_hd, next_states[l].deterministic, lc.discrete_categories, lc.deterministic_dim);
            for (size_t i = 0; i < lc.discrete_categories; ++i) {
                next_states[l].discrete_logits[i] += net.b_d[i];
            }
            next_states[l].discrete_probs = gumbelSoftmax(next_states[l].discrete_logits, 0.1f);
            
            // Decode observation
            std::vector<float> latent_concat;
            latent_concat.reserve(lc.deterministic_dim + lc.stochastic_dim + lc.discrete_categories);
            latent_concat.insert(latent_concat.end(), next_states[l].deterministic.begin(), next_states[l].deterministic.end());
            latent_concat.insert(latent_concat.end(), next_states[l].stochastic.begin(), next_states[l].stochastic.end());
            latent_concat.insert(latent_concat.end(), next_states[l].discrete_probs.begin(), next_states[l].discrete_probs.end());
            
            predicted_obs[l] = decodeObs(latent_concat, l);
        }
        
        result.trajectory.push_back(next_states);
        result.predicted_obs.push_back(predicted_obs);
        current_states = next_states;
    }
    
    return result;
}

float LatentWorldModel::trainStep(
    const std::vector<std::array<std::vector<float>, 5>>& obs_sequence,
    const std::vector<std::vector<float>>& action_sequence,
    const std::vector<float>& reward_sequence) {
    
    // BPTT training - simplified single step for now
    if (obs_sequence.empty()) return 0.0f;
    
    float total_loss = 0.0f;
    for (size_t t = 0; t < obs_sequence.size(); ++t) {
        const auto& obs = obs_sequence[t];
        const auto& action = t < action_sequence.size() ? action_sequence[t] : std::vector<float>(12, 0.0f);
        
        auto output = forward(obs, action, true);
        total_loss += output.total_loss;
    }
    
    // Gradient updates would go here (Adam)
    // Simplified: just return loss
    return total_loss / obs_sequence.size();
}

std::vector<float> LatentWorldModel::encodeObservation(const std::vector<float>& obs, int level) const {
    return encodeObs(obs, level);
}

std::vector<float> LatentWorldModel::decodeObservation(const std::vector<float>& latent, int level) const {
    return decodeObs(latent, level);
}

std::vector<float> LatentWorldModel::matvec(const std::vector<float>& W, const std::vector<float>& x, 
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

std::vector<float> LatentWorldModel::matmul(const std::vector<float>& A, const std::vector<float>& B,
                                            size_t rows_a, size_t cols_a, size_t cols_b) const {
    std::vector<float> result(rows_a * cols_b, 0.0f);
    for (size_t i = 0; i < rows_a; ++i) {
        for (size_t j = 0; j < cols_b; ++j) {
            float sum = 0.0f;
            for (size_t k = 0; k < cols_a; ++k) {
                sum += A[i * cols_a + k] * B[k * cols_b + j];
            }
            result[i * cols_b + j] = sum;
        }
    }
    return result;
}

void LatentWorldModel::addVec(std::vector<float>& a, const std::vector<float>& b) const {
    for (size_t i = 0; i < a.size() && i < b.size(); ++i) {
        a[i] += b[i];
    }
}

void LatentWorldModel::scaleVec(std::vector<float>& a, float s) const {
    for (float& v : a) v *= s;
}

std::vector<float> LatentWorldModel::relu(const std::vector<float>& x) const {
    std::vector<float> result(x.size());
    for (size_t i = 0; i < x.size(); ++i) {
        result[i] = std::max(0.0f, x[i]);
    }
    return result;
}

std::vector<float> LatentWorldModel::tanh(const std::vector<float>& x) const {
    std::vector<float> result(x.size());
    for (size_t i = 0; i < x.size(); ++i) {
        result[i] = std::tanh(x[i]);
    }
    return result;
}

std::vector<float> LatentWorldModel::sigmoid(const std::vector<float>& x) const {
    std::vector<float> result(x.size());
    for (size_t i = 0; i < x.size(); ++i) {
        result[i] = 1.0f / (1.0f + std::exp(-x[i]));
    }
    return result;
}

std::vector<float> LatentWorldModel::softmax(const std::vector<float>& x) const {
    std::vector<float> result(x.size());
    float max_val = *std::max_element(x.begin(), x.end());
    float sum = 0.0f;
    for (size_t i = 0; i < x.size(); ++i) {
        result[i] = std::exp(x[i] - max_val);
        sum += result[i];
    }
    for (float& v : result) v /= (sum + 1e-8f);
    return result;
}

float LatentWorldModel::klDivergence(const std::vector<float>& q_mu, const std::vector<float>& q_logvar,
                                     const std::vector<float>& p_mu, const std::vector<float>& p_logvar) const {
    float kl = 0.0f;
    for (size_t i = 0; i < q_mu.size(); ++i) {
        float q_var = std::exp(q_logvar[i]);
        float p_var = std::exp(p_logvar[i]);
        kl += 0.5f * (p_logvar[i] - q_logvar[i] + 
                     (q_var + (q_mu[i] - p_mu[i]) * (q_mu[i] - p_mu[i])) / p_var - 1.0f);
    }
    return kl;
}

float LatentWorldModel::jepaLoss(const std::vector<float>& pred, const std::vector<float>& target) const {
    float loss = 0.0f;
    for (size_t i = 0; i < pred.size() && i < target.size(); ++i) {
        float diff = pred[i] - target[i];
        loss += diff * diff;
    }
    return loss / std::max<size_t>(1, pred.size());
}

void LatentWorldModel::clipGradients(std::vector<float>& grad, float max_norm) const {
    float norm = 0.0f;
    for (float g : grad) norm += g * g;
    norm = std::sqrt(norm);
    if (norm > max_norm) {
        float scale = max_norm / (norm + 1e-8f);
        for (float& g : grad) g *= scale;
    }
}

void LatentWorldModel::adamUpdate(std::vector<float>& param, std::vector<float>& grad, 
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

std::string LatentWorldModel::serialize() const {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(8);
    
    // Config
    for (int l = 0; l < 5; ++l) {
        const auto& lc = config_.levels[l];
        oss << lc.deterministic_dim << ';' << lc.stochastic_dim << ';' 
            << lc.discrete_categories << ';' << lc.jepa_context_dim << ';'
            << lc.jepa_target_dim << ';' << lc.kl_weight << ';'
            << lc.free_nats << ';' << lc.learning_rate << ';';
    }
    oss << config_.batch_size << ';' << config_.sequence_length << ';'
        << config_.grad_clip << ';' << config_.imagination_horizon << ';'
        << config_.imagination_samples << ';' << config_.discrete_entropy_coeff << ';';
    
    // Networks (simplified - just sizes)
    for (int l = 0; l < 5; ++l) {
        const auto& net = networks_[l];
        auto write_vec = [&](const std::vector<float>& v) {
            for (float val : v) oss << val << ';';
        };
        write_vec(net.W_hh); write_vec(net.W_zh); write_vec(net.W_ah); write_vec(net.b_h);
        write_vec(net.W_hz_mu); write_vec(net.W_hz_logvar); write_vec(net.b_z_mu); write_vec(net.b_z_logvar);
        write_vec(net.W_hxz_mu); write_vec(net.W_hxz_logvar); write_vec(net.b_xz_mu); write_vec(net.b_xz_logvar);
        write_vec(net.W_hd); write_vec(net.b_d);
        write_vec(net.W_hx); write_vec(net.b_x);
        write_vec(net.W_jc); write_vec(net.b_jc);
        write_vec(net.W_jt); write_vec(net.b_jt);
        write_vec(net.W_jp); write_vec(net.b_jp);
    }
    
    return oss.str();
}

bool LatentWorldModel::deserialize(const std::string& data) {
    std::istringstream iss(data);
    std::string token;
    
    // Config
    for (int l = 0; l < 5; ++l) {
        auto& lc = config_.levels[l];
        if (!std::getline(iss, token, ';')) return false; lc.deterministic_dim = std::stoul(token);
        if (!std::getline(iss, token, ';')) return false; lc.stochastic_dim = std::stoul(token);
        if (!std::getline(iss, token, ';')) return false; lc.discrete_categories = std::stoul(token);
        if (!std::getline(iss, token, ';')) return false; lc.jepa_context_dim = std::stoul(token);
        if (!std::getline(iss, token, ';')) return false; lc.jepa_target_dim = std::stoul(token);
        if (!std::getline(iss, token, ';')) return false; lc.kl_weight = std::stof(token);
        if (!std::getline(iss, token, ';')) return false; lc.free_nats = std::stof(token);
        if (!std::getline(iss, token, ';')) return false; lc.learning_rate = std::stof(token);
    }
    if (!std::getline(iss, token, ';')) return false; config_.batch_size = std::stoi(token);
    if (!std::getline(iss, token, ';')) return false; config_.sequence_length = std::stoi(token);
    if (!std::getline(iss, token, ';')) return false; config_.grad_clip = std::stof(token);
    if (!std::getline(iss, token, ';')) return false; config_.imagination_horizon = std::stoi(token);
    if (!std::getline(iss, token, ';')) return false; config_.imagination_samples = std::stoi(token);
    if (!std::getline(iss, token, ';')) return false; config_.discrete_entropy_coeff = std::stof(token);
    
    // Reinitialize networks with new config
    initializeNetworks();
    initializeAdamStates();
    
    // Deserialize networks
    for (int l = 0; l < 5; ++l) {
        auto& net = networks_[l];
        auto read_vec = [&](std::vector<float>& v) {
            for (float& val : v) {
                if (!std::getline(iss, token, ';')) return false;
                val = std::stof(token);
            }
            return true;
        };
        read_vec(net.W_hh); read_vec(net.W_zh); read_vec(net.W_ah); read_vec(net.b_h);
        read_vec(net.W_hz_mu); read_vec(net.W_hz_logvar); read_vec(net.b_z_mu); read_vec(net.b_z_logvar);
        read_vec(net.W_hxz_mu); read_vec(net.W_hxz_logvar); read_vec(net.b_xz_mu); read_vec(net.b_xz_logvar);
        read_vec(net.W_hd); read_vec(net.b_d);
        read_vec(net.W_hx); read_vec(net.b_x);
        read_vec(net.W_jc); read_vec(net.b_jc);
        read_vec(net.W_jt); read_vec(net.b_jt);
        read_vec(net.W_jp); read_vec(net.b_jp);
    }
    
    return true;
}

} // namespace predictive
} // namespace brain
} // namespace yuki