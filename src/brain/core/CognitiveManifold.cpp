#include "brain/core/CognitiveManifold.h"
#include "brain/core/Logger.h"
#include "brain/inference/VariationalStateEstimator.h"
#include "brain/memory/HdcSemanticGraph.h"
#include "brain/ync/NeuromorphicSimulator.h"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <random>
#include <unordered_set>

namespace yuki {
namespace brain {
namespace core {

CognitiveManifold::CognitiveManifold(const ManifoldConfig& cfg)
    : config_(cfg) {
    
    // Initialize scales
    scales_[static_cast<size_t>(ManifoldScale::FAST)].manifold.assign(config_.fast_dim, 0.0f);
    scales_[static_cast<size_t>(ManifoldScale::STANDARD)].manifold.assign(config_.standard_dim, 0.0f);
    scales_[static_cast<size_t>(ManifoldScale::DEEP)].manifold.assign(config_.deep_dim, 0.0f);
    
    for (auto& scale : scales_) {
        scale.last_update = std::chrono::steady_clock::now();
        scale.activity_level = 0.0f;
    }
    
    initializeProjectors();
    initializeAdamStates();
    
    yuki::core::Logger::instance().log(yuki::core::LogLevel::INFO, "CognitiveManifold", 
        "Initialized multi-scale manifold: FAST=128D, STANDARD=512D, DEEP=1024D");
}

void CognitiveManifold::initializeProjectors() {
    // Initialize per-scale projectors
    initializeScaleProjectors(projectors_[static_cast<size_t>(ManifoldScale::FAST)], 
                              config_.fast_dim, config_.fast_lr);
    initializeScaleProjectors(projectors_[static_cast<size_t>(ManifoldScale::STANDARD)], 
                              config_.standard_dim, config_.standard_lr);
    initializeScaleProjectors(projectors_[static_cast<size_t>(ManifoldScale::DEEP)], 
                              config_.deep_dim, config_.deep_lr);
    
    // Cross-scale projectors
    // FAST(128) → STANDARD(512)
    projectors_[0].W_upscale = std::vector<std::vector<float>>(config_.standard_dim, 
        std::vector<float>(config_.fast_dim, 0.0f));
    // STANDARD(512) → DEEP(1024)
    projectors_[1].W_upscale = std::vector<std::vector<float>>(config_.deep_dim, 
        std::vector<float>(config_.standard_dim, 0.0f));
    
    // DEEP(1024) → STANDARD(512)
    projectors_[2].W_downscale = std::vector<std::vector<float>>(config_.standard_dim, 
        std::vector<float>(config_.deep_dim, 0.0f));
    // STANDARD(512) → FAST(128)
    projectors_[1].W_downscale = std::vector<std::vector<float>>(config_.fast_dim, 
        std::vector<float>(config_.standard_dim, 0.0f));
    
    std::mt19937 rng(123);
    auto init_weight = [&](std::vector<std::vector<float>>& W, size_t rows, size_t cols) {
        float scale = std::sqrt(2.0f / (rows + cols));
        std::uniform_real_distribution<float> dist(-scale, scale);
        for (auto& row : W) {
            for (float& v : row) v = dist(rng);
        }
        if (config_.use_orthogonal) orthogonalize(W);
    };
    
    init_weight(projectors_[0].W_upscale, config_.standard_dim, config_.fast_dim);
    init_weight(projectors_[1].W_upscale, config_.deep_dim, config_.standard_dim);
    init_weight(projectors_[2].W_downscale, config_.standard_dim, config_.deep_dim);
    init_weight(projectors_[1].W_downscale, config_.fast_dim, config_.standard_dim);
}

void CognitiveManifold::initializeScaleProjectors(ScaleProjectors& proj, size_t scale_dim, float lr) {
    std::mt19937 rng(42 + scale_dim);
    
    auto init_dense = [&](std::vector<std::vector<float>>& W, size_t rows, size_t cols) {
        W.resize(rows);
        float scale = std::sqrt(6.0f / (rows + cols));
        std::uniform_real_distribution<float> dist(-scale, scale);
        for (auto& row : W) {
            row.resize(cols);
            for (float& v : row) v = dist(rng);
        }
        if (config_.use_orthogonal) orthogonalize(W);
    };
    
    auto init_sparse = [&](std::vector<std::vector<float>>& W, size_t rows, size_t cols) {
        W.resize(rows);
        std::uniform_real_distribution<float> dist(-0.1f, 0.1f);
        std::bernoulli_distribution sparse(config_.hdc_sparsity);
        for (auto& row : W) {
            row.resize(cols);
            for (float& v : row) {
                v = sparse(rng) ? dist(rng) : 0.0f;
            }
        }
    };
    
    // Forward projectors (source -> manifold)
    init_dense(proj.W_vse_to_m, config_.vse_dim, scale_dim);
    init_sparse(proj.W_hdc_to_m, config_.hdc_dim, scale_dim);
    init_dense(proj.W_ync_to_m, config_.ync_dim, scale_dim);
    init_dense(proj.W_embed_to_m, config_.embed_dim, scale_dim);
    init_dense(proj.W_symbolic_to_m, config_.symbolic_dim, scale_dim);
    
    // Reverse projectors (manifold -> source)
    init_dense(proj.W_m_to_vse, scale_dim, config_.vse_dim);
    init_sparse(proj.W_m_to_hdc, scale_dim, config_.hdc_dim);
    init_dense(proj.W_m_to_ync, scale_dim, config_.ync_dim);
    init_dense(proj.W_m_to_embed, scale_dim, config_.embed_dim);
    init_dense(proj.W_m_to_symbolic, scale_dim, config_.symbolic_dim);
}

void CognitiveManifold::initializeAdamStates() {
    for (int s = 0; s < 3; ++s) {
        auto& proj = projectors_[s];
        
        // Use references to avoid pointer confusion
        std::vector<std::reference_wrapper<std::vector<std::vector<float>>>> params = {
            proj.W_vse_to_m, proj.W_hdc_to_m, proj.W_ync_to_m, proj.W_embed_to_m, proj.W_symbolic_to_m,
            proj.W_m_to_vse, proj.W_m_to_hdc, proj.W_m_to_ync, proj.W_m_to_embed, proj.W_m_to_symbolic,
            proj.W_upscale, proj.W_downscale
        };
        
        optimizer_states_[s].resize(params.size());
        for (size_t i = 0; i < params.size(); ++i) {
            auto& mat = params[i].get();
            size_t total_params = 0;
            if (!mat.empty() && !mat[0].empty()) {
                total_params = mat.size() * mat[0].size();
            }
            optimizer_states_[s][i].m.assign(total_params, 0.0f);
            optimizer_states_[s][i].v.assign(total_params, 0.0f);
            optimizer_states_[s][i].t = 0;
        }
    }
}

void CognitiveManifold::orthogonalize(std::vector<std::vector<float>>& W) const {
    size_t rows = W.size();
    if (rows == 0) return;
    size_t cols = W[0].size();
    
    for (size_t j = 0; j < cols; ++j) {
        float norm = 0.0f;
        for (size_t i = 0; i < rows; ++i) norm += W[i][j] * W[i][j];
        norm = std::sqrt(norm) + 1e-8f;
        for (size_t i = 0; i < rows; ++i) W[i][j] /= norm;
        
        for (size_t k = j + 1; k < cols; ++k) {
            float dot = 0.0f;
            for (size_t i = 0; i < rows; ++i) dot += W[i][j] * W[i][k];
            for (size_t i = 0; i < rows; ++i) W[i][k] -= dot * W[i][j];
        }
    }
}

std::vector<float> CognitiveManifold::buildVSEVector(const std::array<float, 8>& q_intent,
                                                      const std::array<float, 3>& q_engagement,
                                                      const std::array<float, 2>& q_urgency,
                                                      float safety_mass, float surprise_budget, float entropy) const {
    std::vector<float> vse_vec(config_.vse_dim);
    size_t idx = 0;
    for (float v : q_intent) vse_vec[idx++] = v;
    for (float v : q_engagement) vse_vec[idx++] = v;
    for (float v : q_urgency) vse_vec[idx++] = v;
    vse_vec[idx++] = safety_mass;
    vse_vec[idx++] = surprise_budget;
    vse_vec[idx++] = entropy;
    return vse_vec;
}

// Source -> Manifold (all scales)
std::array<std::vector<float>, 3> CognitiveManifold::projectVSE(const std::array<float, 8>& q_intent,
                                                                 const std::array<float, 3>& q_engagement,
                                                                 const std::array<float, 2>& q_urgency,
                                                                 float safety_mass, float surprise_budget, float entropy) {
    std::vector<float> vse_vec = buildVSEVector(q_intent, q_engagement, q_urgency, 
                                                 safety_mass, surprise_budget, entropy);
    
    std::array<std::vector<float>, 3> results;
    
    for (int s = 0; s < 3; ++s) {
        auto& proj = projectors_[s];
        auto& scale = scales_[s];
        size_t scale_dim = scale.manifold.size();
        
        std::vector<float> m = matvec(proj.W_vse_to_m, vse_vec);
        
        // EMA update with scale-specific rate
        float alpha = (s == 0) ? 0.1f : (s == 1 ? 0.05f : 0.01f);
        for (size_t i = 0; i < scale_dim; ++i) {
            scale.manifold[i] = (1.0f - alpha) * scale.manifold[i] + alpha * m[i];
        }
        
        results[s] = scale.manifold;
        scale.last_update = std::chrono::steady_clock::now();
        scale.activity_level = 1.0f;
    }
    
    return results;
}

std::array<std::vector<float>, 3> CognitiveManifold::projectHDC(const std::vector<float>& hdc_vector) {
    std::array<std::vector<float>, 3> results;
    
    for (int s = 0; s < 3; ++s) {
        auto& proj = projectors_[s];
        auto& scale = scales_[s];
        size_t scale_dim = scale.manifold.size();
        
        std::vector<float> m = sparseProject(proj.W_hdc_to_m, hdc_vector);
        
        float alpha = (s == 0) ? 0.1f : (s == 1 ? 0.05f : 0.01f);
        for (size_t i = 0; i < scale_dim; ++i) {
            scale.manifold[i] = (1.0f - alpha) * scale.manifold[i] + alpha * m[i];
        }
        
        results[s] = scale.manifold;
        scale.last_update = std::chrono::steady_clock::now();
        scale.activity_level = 1.0f;
    }
    
    return results;
}

std::array<std::vector<float>, 3> CognitiveManifold::projectYNC(const std::vector<float>& motor_output) {
    std::array<std::vector<float>, 3> results;
    
    for (int s = 0; s < 3; ++s) {
        auto& proj = projectors_[s];
        auto& scale = scales_[s];
        size_t scale_dim = scale.manifold.size();
        
        std::vector<float> m = matvec(proj.W_ync_to_m, motor_output);
        
        float alpha = (s == 0) ? 0.05f : (s == 1 ? 0.02f : 0.005f);
        for (size_t i = 0; i < scale_dim; ++i) {
            scale.manifold[i] = (1.0f - alpha) * scale.manifold[i] + alpha * m[i];
        }
        
        results[s] = scale.manifold;
        scale.last_update = std::chrono::steady_clock::now();
    }
    
    return results;
}

std::array<std::vector<float>, 3> CognitiveManifold::projectEmbedding(const std::vector<float>& embedding) {
    std::array<std::vector<float>, 3> results;
    
    for (int s = 0; s < 3; ++s) {
        auto& proj = projectors_[s];
        auto& scale = scales_[s];
        size_t scale_dim = scale.manifold.size();
        
        std::vector<float> m = matvec(proj.W_embed_to_m, embedding);
        
        float alpha = (s == 0) ? 0.1f : (s == 1 ? 0.05f : 0.01f);
        for (size_t i = 0; i < scale_dim; ++i) {
            scale.manifold[i] = (1.0f - alpha) * scale.manifold[i] + alpha * m[i];
        }
        
        results[s] = scale.manifold;
        scale.last_update = std::chrono::steady_clock::now();
        scale.activity_level = 1.0f;
    }
    
    return results;
}

std::array<std::vector<float>, 3> CognitiveManifold::projectSymbolic(const std::vector<std::pair<std::string, float>>& facts) {
    std::array<std::vector<float>, 3> results;
    
    for (int s = 0; s < 3; ++s) {
        auto& proj = projectors_[s];
        auto& scale = scales_[s];
        size_t scale_dim = scale.manifold.size();
        
        std::vector<float> sym_vec(config_.symbolic_dim, 0.0f);
        std::hash<std::string> hasher;
        for (const auto& [fact, weight] : facts) {
            size_t h = hasher(fact) % config_.symbolic_dim;
            sym_vec[h] += weight;
        }
        
        float norm = 0.0f;
        for (float v : sym_vec) norm += v * v;
        norm = std::sqrt(norm) + 1e-8f;
        for (float& v : sym_vec) v /= norm;
        
        std::vector<float> m = matvec(proj.W_symbolic_to_m, sym_vec);
        
        float alpha = (s == 0) ? 0.1f : (s == 1 ? 0.05f : 0.01f);
        for (size_t i = 0; i < scale_dim; ++i) {
            scale.manifold[i] = (1.0f - alpha) * scale.manifold[i] + alpha * m[i];
        }
        
        results[s] = scale.manifold;
        scale.last_update = std::chrono::steady_clock::now();
    }
    
    return results;
}

// Manifold -> Source
void CognitiveManifold::projectToVSE(const std::vector<float>& manifold, ManifoldScale scale,
                                      std::array<float, 8>& q_intent,
                                      std::array<float, 3>& q_engagement,
                                      std::array<float, 2>& q_urgency,
                                      float& safety_mass, float& surprise_budget, float& entropy) const {
    const auto& proj = projectors_[static_cast<size_t>(scale)];
    std::vector<float> vse = matvec(proj.W_m_to_vse, manifold);
    
    size_t idx = 0;
    for (int i = 0; i < 8; ++i) q_intent[i] = std::max(0.0f, vse[idx++]);
    for (int i = 0; i < 3; ++i) q_engagement[i] = std::max(0.0f, vse[idx++]);
    for (int i = 0; i < 2; ++i) q_urgency[i] = std::max(0.0f, vse[idx++]);
    safety_mass = std::clamp(vse[idx++], 0.0f, 1.0f);
    surprise_budget = std::clamp(vse[idx++], 0.0f, 1.0f);
    entropy = std::clamp(vse[idx++], 0.0f, 1.0f);
    
    auto softmax = [](auto& arr) {
        float max_v = *std::max_element(arr.begin(), arr.end());
        float sum = 0.0f;
        for (float& v : arr) { v = std::exp(v - max_v); sum += v; }
        for (float& v : arr) v /= (sum + 1e-8f);
    };
    softmax(q_intent);
    softmax(q_engagement);
    softmax(q_urgency);
}

std::vector<std::pair<std::string, float>> CognitiveManifold::projectToHDC(const std::vector<float>& manifold, 
                                                                            ManifoldScale scale,
                                                                            int top_k) const {
    const auto& proj = projectors_[static_cast<size_t>(scale)];
    std::vector<float> hdc = sparseProject(proj.W_m_to_hdc, manifold);
    
    std::vector<std::pair<size_t, float>> indexed;
    indexed.reserve(hdc.size());
    for (size_t i = 0; i < hdc.size(); ++i) {
        if (hdc[i] > 0.01f) indexed.emplace_back(i, hdc[i]);
    }
    
    std::partial_sort(indexed.begin(), 
                     indexed.begin() + std::min(top_k, static_cast<int>(indexed.size())),
                     indexed.end(),
                     [](const auto& a, const auto& b) { return a.second > b.second; });
    
    std::vector<std::pair<std::string, float>> result;
    for (size_t i = 0; i < std::min(static_cast<size_t>(top_k), indexed.size()); ++i) {
        result.emplace_back("concept_" + std::to_string(indexed[i].first), indexed[i].second);
    }
    
    return result;
}

std::vector<float> CognitiveManifold::projectToYNC(const std::vector<float>& manifold, ManifoldScale scale) const {
    const auto& proj = projectors_[static_cast<size_t>(scale)];
    return matvec(proj.W_m_to_ync, manifold);
}

std::vector<float> CognitiveManifold::projectToEmbedding(const std::vector<float>& manifold, ManifoldScale scale) const {
    const auto& proj = projectors_[static_cast<size_t>(scale)];
    return matvec(proj.W_m_to_embed, manifold);
}

// Cross-scale operations
std::vector<float> CognitiveManifold::upscale(const std::vector<float>& src, ManifoldScale from_scale) const {
    size_t from_idx = static_cast<size_t>(from_scale);
    if (from_idx >= 2) return src;  // Already at DEEP
    
    const auto& proj = projectors_[from_idx];
    size_t target_dim = (from_scale == ManifoldScale::FAST) ? config_.standard_dim : config_.deep_dim;
    std::vector<float> result(target_dim, 0.0f);
    
    // Matvec with upscale matrix
    for (size_t i = 0; i < target_dim; ++i) {
        float sum = 0.0f;
        for (size_t j = 0; j < src.size() && j < proj.W_upscale[i].size(); ++j) {
            sum += proj.W_upscale[i][j] * src[j];
        }
        result[i] = sum;
    }
    
    return relu(result);  // Non-linearity for upscaling
}

std::vector<float> CognitiveManifold::downscale(const std::vector<float>& src, ManifoldScale from_scale) const {
    size_t from_idx = static_cast<size_t>(from_scale);
    if (from_idx == 0) return src;  // Already at FAST
    
    const auto& proj = projectors_[from_idx];
    size_t target_dim = (from_scale == ManifoldScale::DEEP) ? config_.standard_dim : config_.fast_dim;
    std::vector<float> result(target_dim, 0.0f);
    
    // Matvec with downscale matrix
    for (size_t i = 0; i < target_dim; ++i) {
        float sum = 0.0f;
        for (size_t j = 0; j < src.size() && j < proj.W_downscale[i].size(); ++j) {
            sum += proj.W_downscale[i][j] * src[j];
        }
        result[i] = sum;
    }
    
    return result;
}

std::array<std::vector<float>, 3> CognitiveManifold::fuseScales(const std::array<std::vector<float>, 3>& scales) const {
    std::array<std::vector<float>, 3> fused;
    
    // FAST: combine FAST + downsampled STANDARD
    fused[0] = scales[0];
    auto std_down = downscale(scales[1], ManifoldScale::STANDARD);
    for (size_t i = 0; i < fused[0].size() && i < std_down.size(); ++i) {
        fused[0][i] = 0.7f * fused[0][i] + 0.3f * std_down[i];
    }
    
    // STANDARD: combine STANDARD + upscaled FAST + downsampled DEEP
    fused[1] = scales[1];
    auto fast_up = upscale(scales[0], ManifoldScale::FAST);
    auto deep_down = downscale(scales[2], ManifoldScale::DEEP);
    for (size_t i = 0; i < fused[1].size(); ++i) {
        float val = fused[1][i];
        if (i < fast_up.size()) val += 0.2f * fast_up[i];
        if (i < deep_down.size()) val += 0.2f * deep_down[i];
        fused[1][i] = val / 1.4f;
    }
    
    // DEEP: combine DEEP + upscaled STANDARD
    fused[2] = scales[2];
    auto std_up = upscale(scales[1], ManifoldScale::STANDARD);
    for (size_t i = 0; i < fused[2].size() && i < std_up.size(); ++i) {
        fused[2][i] = 0.7f * fused[2][i] + 0.3f * std_up[i];
    }
    
    return fused;
}

// Alignment / Training
void CognitiveManifold::align(const std::vector<float>& source_a, const std::vector<float>& source_b,
                               const std::string& source_a_name, const std::string& source_b_name) {
    if (source_a.size() != source_b.size()) return;
    
    size_t scale_dim = source_a.size();
    ManifoldScale scale = (scale_dim == config_.fast_dim) ? ManifoldScale::FAST :
                          (scale_dim == config_.standard_dim) ? ManifoldScale::STANDARD : ManifoldScale::DEEP;
    
    auto& proj = projectors_[static_cast<size_t>(scale)];
    float lr = config_.projector_lr * config_.alignment_weight;
    
    // Compute reconstruction errors and update projectors
    // Simplified: just log for now
    yuki::core::Logger::instance().log(yuki::core::LogLevel::DEBUG, "CognitiveManifold",
        "Aligned " + source_a_name + " <-> " + source_b_name + " at scale " + std::to_string(scale_dim) + "D");
}

void CognitiveManifold::alignScales(ManifoldScale from_scale, ManifoldScale to_scale) {
    // Cross-scale alignment
    size_t from_idx = static_cast<size_t>(from_scale);
    size_t to_idx = static_cast<size_t>(to_scale);
    
    yuki::core::Logger::instance().log(yuki::core::LogLevel::DEBUG, "CognitiveManifold",
        "Cross-scale alignment: " + std::to_string(from_idx) + " -> " + std::to_string(to_idx));
}

ProjectionResult CognitiveManifold::reconstructAll(const std::array<std::vector<float>, 3>& manifolds) const {
    ProjectionResult result;
    result.manifold_vecs = manifolds;
    
    for (int s = 0; s < 3; ++s) {
        const auto& proj = projectors_[s];
        const auto& m = manifolds[s];
        
        auto recon_vse = matvec(proj.W_m_to_vse, m);
        auto recon_ync = matvec(proj.W_m_to_ync, m);
        auto recon_embed = matvec(proj.W_m_to_embed, m);
        auto recon_symbolic = matvec(proj.W_m_to_symbolic, m);
        auto recon_hdc = sparseProject(proj.W_m_to_hdc, m);
        
        // Compute errors (simplified)
        result.source_errors["vse"] = 0.0f;
        result.source_errors["ync"] = 0.0f;
        result.source_errors["embed"] = 0.0f;
        result.source_errors["symbolic"] = 0.0f;
        result.source_errors["hdc"] = 0.0f;
        result.scale_errors[s] = 0.0f;
    }
    
    result.reconstruction_error = 0.0f;
    return result;
}

void CognitiveManifold::step() {
    // Decay activity levels
    for (auto& scale : scales_) {
        scale.activity_level *= 0.99f;
        for (float& v : scale.manifold) v *= 0.999f;
    }
    
    // Periodic orthogonalization
    static int step_count = 0;
    if (++step_count % 10000 == 0) {
        for (int s = 0; s < 3; ++s) {
            orthogonalize(projectors_[s].W_vse_to_m);
            orthogonalize(projectors_[s].W_embed_to_m);
            orthogonalize(projectors_[s].W_symbolic_to_m);
            orthogonalize(projectors_[s].W_m_to_vse);
            orthogonalize(projectors_[s].W_m_to_ync);
            orthogonalize(projectors_[s].W_m_to_embed);
            orthogonalize(projectors_[s].W_m_to_symbolic);
        }
    }
}

void CognitiveManifold::setManifold(ManifoldScale scale, const std::vector<float>& m) {
    auto& s = scales_[static_cast<size_t>(scale)];
    if (m.size() == s.manifold.size()) {
        s.manifold = m;
    }
}

std::vector<float> CognitiveManifold::matvec(const std::vector<std::vector<float>>& W, 
                                              const std::vector<float>& x) const {
    if (W.empty() || W[0].empty()) return {};
    size_t rows = W.size();
    size_t cols = W[0].size();
    std::vector<float> result(rows, 0.0f);
    
    for (size_t i = 0; i < rows; ++i) {
        float sum = 0.0f;
        for (size_t j = 0; j < cols && j < x.size(); ++j) {
            sum += W[i][j] * x[j];
        }
        result[i] = sum;
    }
    return result;
}

std::vector<float> CognitiveManifold::sparseProject(const std::vector<std::vector<float>>& W,
                                                     const std::vector<float>& x) const {
    if (W.empty() || W[0].empty()) return {};
    size_t rows = W.size();
    size_t cols = W[0].size();
    std::vector<float> result(rows, 0.0f);
    
    for (size_t i = 0; i < rows; ++i) {
        float sum = 0.0f;
        for (size_t j = 0; j < cols && j < x.size(); ++j) {
            float w = W[i][j];
            if (w != 0.0f) sum += w * x[j];
        }
        result[i] = sum;
    }
    return result;
}

std::vector<float> CognitiveManifold::matmul(const std::vector<float>& A, const std::vector<float>& B,
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

std::vector<float> CognitiveManifold::relu(const std::vector<float>& x) const {
    std::vector<float> result(x.size());
    for (size_t i = 0; i < x.size(); ++i) {
        result[i] = std::max(0.0f, x[i]);
    }
    return result;
}

std::vector<float> CognitiveManifold::tanh(const std::vector<float>& x) const {
    std::vector<float> result(x.size());
    for (size_t i = 0; i < x.size(); ++i) {
        result[i] = std::tanh(x[i]);
    }
    return result;
}

void CognitiveManifold::adamUpdate(std::vector<float>& param, std::vector<float>& grad, 
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

} // namespace core
} // namespace brain
} // namespace yuki