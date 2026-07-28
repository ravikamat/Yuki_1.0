#include "brain/memory/MultimodalEncoder.h"
#include "brain/core/ConfigManager.h"
#include "brain/language/Word2Vec.h"
#include <cmath>
#include <fstream>
#include <sstream>
#include <random>
#include <algorithm>

namespace yuki::memory {

// ============================================================================
// BindingMatrix Implementation
// ============================================================================

BindingMatrix::BindingMatrix() {
    std::unordered_map<std::string, float> float_cfg;
    if (ConfigManager::instance().loadFloatConfig("data/multimodal_config.txt", float_cfg)) {
        if (float_cfg.count("temperature")) {
            temperature_ = float_cfg["temperature"];
        }
    }
}

BindingMatrix::BindingMatrix(const std::string& config_path) {
    std::unordered_map<std::string, float> float_cfg;
    if (ConfigManager::instance().loadFloatConfig(config_path, float_cfg)) {
        if (float_cfg.count("temperature")) {
            temperature_ = float_cfg["temperature"];
        }
    }
}

void BindingMatrix::initializeWeights(ModalityType type, size_t in_dim) {
    size_t out_dim = hdcDim();
    W_[type] = std::make_unique<yuki::learning::neural::Matrix>(out_dim, in_dim);
    b_[type] = std::vector<float>(out_dim, 0.0f);

    std::mt19937 rng(std::random_device{}());
    float limit = std::sqrt(6.0f / static_cast<float>(in_dim + out_dim));
    std::uniform_real_distribution<float> dist(-limit, limit);

    for (size_t r = 0; r < out_dim; ++r) {
        for (size_t c = 0; c < in_dim; ++c) {
            (*W_[type])(r, c) = dist(rng);
        }
    }
}

Hypervector BindingMatrix::project(const std::vector<float>& features, ModalityType type) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (features.empty()) return Hypervector{};

    if (W_.find(type) == W_.end() || W_[type]->cols != features.size()) {
        initializeWeights(type, features.size());
    }

    size_t out_dim = hdcDim();
    Hypervector result;

    for (size_t i = 0; i < out_dim; ++i) {
        float sum = b_[type][i];
        for (size_t j = 0; j < features.size(); ++j) {
            sum += (*W_[type])(i, j) * features[j];
        }
        if (sum >= 0.0f) {
            result.set(i, true);
        }
    }

    return result;
}

float BindingMatrix::similarity(const Hypervector& a, const Hypervector& b) {
    size_t match = 0;
    size_t total = 10000;
    for (size_t i = 0; i < total; ++i) {
        if (a.get(i) == b.get(i)) {
            match++;
        }
    }
    return (2.0f * static_cast<float>(match) / static_cast<float>(total)) - 1.0f;
}

float BindingMatrix::infoNCELoss(const Hypervector& anchor,
                                 const Hypervector& positive,
                                 const std::vector<Hypervector>& negatives) const {
    float pos_sim = similarity(anchor, positive) / temperature_;
    float pos_exp = std::exp(pos_sim);
    float sum_exp = pos_exp;

    for (const auto& neg : negatives) {
        sum_exp += std::exp(similarity(anchor, neg) / temperature_);
    }

    return -std::log(pos_exp / std::max(sum_exp, 1e-7f));
}

void BindingMatrix::trainStep(const ModalitySample& anchor,
                              const ModalitySample& positive,
                              const std::vector<ModalitySample>& negatives,
                              float learning_rate) {
    std::lock_guard<std::mutex> lock(mtx_);

    if (W_.find(anchor.type) == W_.end()) initializeWeights(anchor.type, anchor.features.size());
    if (W_.find(positive.type) == W_.end()) initializeWeights(positive.type, positive.features.size());

    Hypervector h_anc = anchor.hdc;
    Hypervector h_pos = positive.hdc;
    std::vector<Hypervector> h_negs;
    for (const auto& neg : negatives) h_negs.push_back(neg.hdc);

    float pos_sim = similarity(h_anc, h_pos) / temperature_;
    float pos_exp = std::exp(pos_sim);
    float sum_exp = pos_exp;

    std::vector<float> neg_exps;
    for (const auto& neg : h_negs) {
        float s = similarity(h_anc, neg) / temperature_;
        float e = std::exp(s);
        neg_exps.push_back(e);
        sum_exp += e;
    }

    float p_pos = pos_exp / std::max(sum_exp, 1e-7f);
    float grad_pos = (p_pos - 1.0f) / temperature_;

    size_t out_dim = hdcDim();
    for (size_t i = 0; i < out_dim; ++i) {
        float sign_anc = h_anc.get(i) ? 1.0f : -1.0f;
        float sign_pos = h_pos.get(i) ? 1.0f : -1.0f;

        for (size_t j = 0; j < anchor.features.size(); ++j) {
            (*W_[anchor.type])(i, j) -= learning_rate * grad_pos * sign_pos * anchor.features[j];
        }
        b_[anchor.type][i] -= learning_rate * grad_pos * sign_pos;

        for (size_t j = 0; j < positive.features.size(); ++j) {
            (*W_[positive.type])(i, j) -= learning_rate * grad_pos * sign_anc * positive.features[j];
        }
        b_[positive.type][i] -= learning_rate * grad_pos * sign_anc;
    }
}

void BindingMatrix::save(const std::string& path) const {
    std::lock_guard<std::mutex> lock(mtx_);
    std::ofstream out(path, std::ios::binary);
    if (!out.is_open()) return;

    uint32_t magic = 0x59424D30; // "YBM0"
    out.write(reinterpret_cast<const char*>(&magic), sizeof(magic));

    uint32_t count = static_cast<uint32_t>(W_.size());
    out.write(reinterpret_cast<const char*>(&count), sizeof(count));

    for (const auto& [type, mat] : W_) {
        uint8_t t = static_cast<uint8_t>(type);
        out.write(reinterpret_cast<const char*>(&t), sizeof(t));
        uint32_t r = static_cast<uint32_t>(mat->rows);
        uint32_t c = static_cast<uint32_t>(mat->cols);
        out.write(reinterpret_cast<const char*>(&r), sizeof(r));
        out.write(reinterpret_cast<const char*>(&c), sizeof(c));

        for (size_t i = 0; i < r; ++i) {
            for (size_t j = 0; j < c; ++j) {
                float val = (*mat)(i, j);
                out.write(reinterpret_cast<const char*>(&val), sizeof(val));
            }
        }
        const auto& bias = b_.at(type);
        out.write(reinterpret_cast<const char*>(bias.data()), sizeof(float) * bias.size());
    }
}

bool BindingMatrix::load(const std::string& path) {
    std::lock_guard<std::mutex> lock(mtx_);
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) return false;

    uint32_t magic = 0;
    in.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    if (magic != 0x59424D30) return false;

    uint32_t count = 0;
    in.read(reinterpret_cast<char*>(&count), sizeof(count));

    for (uint32_t k = 0; k < count; ++k) {
        uint8_t t = 0;
        in.read(reinterpret_cast<char*>(&t), sizeof(t));
        uint32_t r = 0, c = 0;
        in.read(reinterpret_cast<char*>(&r), sizeof(r));
        in.read(reinterpret_cast<char*>(&c), sizeof(c));

        ModalityType type = static_cast<ModalityType>(t);
        W_[type] = std::make_unique<yuki::learning::neural::Matrix>(r, c);
        for (size_t i = 0; i < r; ++i) {
            for (size_t j = 0; j < c; ++j) {
                float val = 0.0f;
                in.read(reinterpret_cast<char*>(&val), sizeof(val));
                (*W_[type])(i, j) = val;
            }
        }
        b_[type].resize(r);
        in.read(reinterpret_cast<char*>(b_[type].data()), sizeof(float) * r);
    }
    return true;
}

// ============================================================================
// MultimodalEncoder Implementation
// ============================================================================

MultimodalEncoder::MultimodalEncoder(BindingMatrix* binding)
    : binding_(binding) {}

Hypervector MultimodalEncoder::permute(const Hypervector& hv, size_t shift) {
    Hypervector result;
    size_t dim = 10000;
    shift %= dim;

    for (size_t i = 0; i < dim; ++i) {
        size_t new_pos = (i + shift) % dim;
        result.set(new_pos, hv.get(i));
    }
    return result;
}

Hypervector MultimodalEncoder::fuse(const std::vector<float>& audio_mfcc,
                                     const std::vector<float>& visual_hog,
                                     const std::vector<float>& text_w2v) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (!binding_) return Hypervector{};

    Hypervector h_audio = binding_->project(audio_mfcc, ModalityType::AUDIO);
    Hypervector h_visual = binding_->project(visual_hog, ModalityType::VISUAL);
    Hypervector h_text = binding_->project(text_w2v, ModalityType::TEXT);

    Hypervector perm_text = permute(h_text, 1);

    Hypervector fused;
    size_t dim = 10000;
    for (size_t i = 0; i < dim; ++i) {
        bool bit = h_audio.get(i) ^ h_visual.get(i) ^ perm_text.get(i);
        fused.set(i, bit);
    }
    return fused;
}

std::vector<int64_t> MultimodalEncoder::queryVisualByText(const std::string& text_query,
                                                          const HdcSemanticGraph& graph,
                                                          size_t top_k) {
    std::lock_guard<std::mutex> lock(mtx_);
    std::vector<int64_t> results;
    if (!binding_) return results;

    std::vector<float> text_vec(300, 0.0f);
    for (size_t i = 0; i < text_query.size() && i < 300; ++i) {
        text_vec[i] = static_cast<float>(text_query[i]) / 255.0f;
    }

    Hypervector query_hdc = binding_->project(text_vec, ModalityType::TEXT);

    auto concepts = graph.getAllConcepts();
    std::vector<std::pair<float, int64_t>> scored;

    for (const auto& conceptItem : concepts) {
        float sim = BindingMatrix::similarity(query_hdc, conceptItem.identity);
        scored.push_back(std::make_pair(sim, conceptItem.id));
    }

    std::sort(scored.rbegin(), scored.rend());
    for (size_t i = 0; i < std::min(top_k, scored.size()); ++i) {
        results.push_back(scored[i].second);
    }

    return results;
}

void MultimodalEncoder::onlineTrainStep(const HdcSemanticGraph& graph, float lr) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (!binding_) return;

    auto concepts = graph.getAllConcepts();
    if (concepts.empty()) return;

    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<size_t> dist(0, concepts.size() - 1);

    size_t idx = dist(rng);
    (void)idx;

    std::vector<float> feat_a(300, 0.1f);
    std::vector<float> feat_v(3780, 0.1f);

    ModalitySample anc{ModalityType::TEXT, feat_a, binding_->project(feat_a, ModalityType::TEXT)};
    ModalitySample pos{ModalityType::VISUAL, feat_v, binding_->project(feat_v, ModalityType::VISUAL)};

    std::vector<ModalitySample> negs;
    for (size_t i = 0; i < 4; ++i) {
        std::vector<float> neg_v(3780, static_cast<float>(i + 1) * 0.2f);
        negs.push_back(ModalitySample{ModalityType::VISUAL, neg_v, binding_->project(neg_v, ModalityType::VISUAL)});
    }

    binding_->trainStep(anc, pos, negs, lr);
}

} // namespace yuki::memory
