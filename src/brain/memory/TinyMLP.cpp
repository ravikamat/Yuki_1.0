#include "TinyMLP.h"
#include <cstring>

namespace yuki::memory {

TinyMLP::TinyMLP() {
    W1_.fill(0.0f); b1_.fill(0.0f);
    W2_.fill(0.0f); b2_.fill(0.0f);
}

TinyMLP::TinyMLP(uint64_t seed) {
    initWeights(seed);
}

void TinyMLP::initWeights(uint64_t seed) {
    std::mt19937_64 rng(seed);
    std::normal_distribution<float> dist(0.0f, 1.0f);

    float w1_scale = std::sqrt(2.0f / INPUT_DIM);
    for (auto& w : W1_) w = dist(rng) * w1_scale;
    for (auto& b : b1_) b = 0.0f;

    float w2_scale = std::sqrt(2.0f / HIDDEN_DIM);
    for (auto& w : W2_) w = dist(rng) * w2_scale;
    for (auto& b : b2_) b = 0.0f;

    initialized_ = true;
    update_count_ = 0;
}

std::array<float, TinyMLP::OUTPUT_DIM> TinyMLP::forward(
    const std::array<float, TinyMLP::INPUT_DIM>& input) const
{
    std::array<float, OUTPUT_DIM> output;

    // Hidden layer: h1_raw[j] = b1[j] + Σ_k input[k] * W1[k][j]
    std::array<float, HIDDEN_DIM> h1_raw;
    for (int j = 0; j < HIDDEN_DIM; ++j) {
        float sum = b1_[j];
        for (int k = 0; k < INPUT_DIM; ++k) {
            sum += input[k] * W1_[k * HIDDEN_DIM + j];
        }
        h1_raw[j] = sum;
    }

    std::array<float, HIDDEN_DIM> h1;
    for (int j = 0; j < HIDDEN_DIM; ++j) {
        h1[j] = relu(h1_raw[j]);
    }

    // Output layer: z[i] = b2[i] + Σ_j h1[j] * W2[j][i]
    for (int i = 0; i < OUTPUT_DIM; ++i) {
        float sum = b2_[i];
        for (int j = 0; j < HIDDEN_DIM; ++j) {
            sum += h1[j] * W2_[j * OUTPUT_DIM + i];
        }
        // Numerical safety clip
        output[i] = std::max(-10.0f, std::min(10.0f, sum));
    }

    return output;
}

void TinyMLP::groupSoftmax(const std::array<float, OUTPUT_DIM>& logits,
                           float temperature,
                           std::array<float, OUTPUT_DIM>& probs)
{
    // SDM group: 0-7
    float max_sdm = -std::numeric_limits<float>::infinity();
    for (int i = 0; i < 8; ++i) max_sdm = std::max(max_sdm, logits[i]);
    float sum_sdm = 0.0f;
    for (int i = 0; i < 8; ++i) {
        probs[i] = std::exp((logits[i] - max_sdm) / temperature);
        sum_sdm += probs[i];
    }
    for (int i = 0; i < 8; ++i) probs[i] /= sum_sdm;

    // LSH group: 8-15
    float max_lsh = -std::numeric_limits<float>::infinity();
    for (int i = 8; i < 16; ++i) max_lsh = std::max(max_lsh, logits[i]);
    float sum_lsh = 0.0f;
    for (int i = 8; i < 16; ++i) {
        probs[i] = std::exp((logits[i] - max_lsh) / temperature);
        sum_lsh += probs[i];
    }
    for (int i = 8; i < 16; ++i) probs[i] /= sum_lsh;

    // Action group: 16-20
    float max_act = -std::numeric_limits<float>::infinity();
    for (int i = 16; i < 21; ++i) max_act = std::max(max_act, logits[i]);
    float sum_act = 0.0f;
    for (int i = 16; i < 21; ++i) {
        probs[i] = std::exp((logits[i] - max_act) / temperature);
        sum_act += probs[i];
    }
    for (int i = 16; i < 21; ++i) probs[i] /= sum_act;

    // Reserved 21-23: uniform (no gradient)
    for (int i = 21; i < 24; ++i) probs[i] = 1.0f / 3.0f;
}

void TinyMLP::learn(const std::vector<LearningSample>& batch) {
    if (batch.empty()) return;

    // Accumulate gradients
    std::array<float, INPUT_DIM * HIDDEN_DIM> dW1{}; dW1.fill(0.0f);
    std::array<float, HIDDEN_DIM> db1{}; db1.fill(0.0f);
    std::array<float, HIDDEN_DIM * OUTPUT_DIM> dW2{}; dW2.fill(0.0f);
    std::array<float, OUTPUT_DIM> db2{}; db2.fill(0.0f);

    for (const auto& s : batch) {
        // Forward to get hidden activations
        std::array<float, HIDDEN_DIM> h1_raw;
        for (int j = 0; j < HIDDEN_DIM; ++j) {
            float sum = b1_[j];
            for (int k = 0; k < INPUT_DIM; ++k) {
                sum += s.input[k] * W1_[k * HIDDEN_DIM + j];
            }
            h1_raw[j] = sum;
        }

        std::array<float, HIDDEN_DIM> h1;
        for (int j = 0; j < HIDDEN_DIM; ++j) h1[j] = relu(h1_raw[j]);

        // Policy probabilities from stored logits
        std::array<float, OUTPUT_DIM> probs;
        groupSoftmax(s.logits, TEMPERATURE, probs);

        // REINFORCE gradient: ∂L/∂z_i = -r * (1_{i==a} - p_i)
        std::array<float, OUTPUT_DIM> dz{};
        float r = s.reward;

        for (int i = 0; i < 8; ++i) {
            float ind = (i == s.sdm_action) ? 1.0f : 0.0f;
            dz[i] = -r * (ind - probs[i]);
        }
        for (int i = 8; i < 16; ++i) {
            float ind = (i == (s.lsh_action + 8)) ? 1.0f : 0.0f;
            dz[i] = -r * (ind - probs[i]);
        }
        for (int i = 16; i < 21; ++i) {
            float ind = (i == (s.action_action + 16)) ? 1.0f : 0.0f;
            dz[i] = -r * (ind - probs[i]);
        }
        // Reserved 21-23: no gradient

        for (auto& g : dz) clipGradient(g);

        // Backprop to hidden
        std::array<float, HIDDEN_DIM> dh1{};
        for (int j = 0; j < HIDDEN_DIM; ++j) {
            float sum = 0.0f;
            for (int i = 0; i < OUTPUT_DIM; ++i) {
                sum += dz[i] * W2_[j * OUTPUT_DIM + i];
            }
            dh1[j] = sum * reluDeriv(h1_raw[j]);
        }

        // Accumulate
        for (int i = 0; i < OUTPUT_DIM; ++i) {
            for (int j = 0; j < HIDDEN_DIM; ++j) {
                dW2[j * OUTPUT_DIM + i] += dz[i] * h1[j];
            }
            db2[i] += dz[i];
        }

        for (int j = 0; j < HIDDEN_DIM; ++j) {
            for (int k = 0; k < INPUT_DIM; ++k) {
                dW1[k * HIDDEN_DIM + j] += dh1[j] * s.input[k];
            }
            db1[j] += dh1[j];
        }
    }

    // Apply: average + learning rate + weight decay
    float scale = LEARNING_RATE / static_cast<float>(batch.size());

    for (size_t i = 0; i < W1_.size(); ++i) {
        clipGradient(dW1[i]);
        W1_[i] = W1_[i] * (1.0f - WEIGHT_DECAY) + dW1[i] * scale;
    }
    for (size_t i = 0; i < b1_.size(); ++i) {
        clipGradient(db1[i]);
        b1_[i] += db1[i] * scale;
    }
    for (size_t i = 0; i < W2_.size(); ++i) {
        clipGradient(dW2[i]);
        W2_[i] = W2_[i] * (1.0f - WEIGHT_DECAY) + dW2[i] * scale;
    }
    for (size_t i = 0; i < b2_.size(); ++i) {
        clipGradient(db2[i]);
        b2_[i] += db2[i] * scale;
    }

    // NaN/Inf recovery: if any weight corrupted, reinit from seed
    bool bad = false;
    for (auto w : W1_) if (!isFinite(w)) { bad = true; break; }
    if (!bad) for (auto w : b1_) if (!isFinite(w)) { bad = true; break; }
    if (!bad) for (auto w : W2_) if (!isFinite(w)) { bad = true; break; }
    if (!bad) for (auto w : b2_) if (!isFinite(w)) { bad = true; break; }

    if (bad) {
        std::cerr << "[TinyMLP] NaN/Inf detected after update. Reverting to seed.\n";
        initWeights(DMC_MLP_SEED_v1);
        return;
    }

    ++update_count_;
}

std::vector<uint8_t> TinyMLP::serialize() const {
    std::vector<uint8_t> blob;
    uint32_t version = 1;
    blob.insert(blob.end(), reinterpret_cast<const uint8_t*>(&version),
                reinterpret_cast<const uint8_t*>(&version) + sizeof(version));
    blob.insert(blob.end(), reinterpret_cast<const uint8_t*>(&update_count_),
                reinterpret_cast<const uint8_t*>(&update_count_) + sizeof(update_count_));

    auto append = [&blob](const auto& arr) {
        const uint8_t* p = reinterpret_cast<const uint8_t*>(arr.data());
        blob.insert(blob.end(), p, p + arr.size() * sizeof(float));
    };
    append(W1_); append(b1_); append(W2_); append(b2_);
    return blob;
}

bool TinyMLP::deserialize(const std::vector<uint8_t>& blob) {
    size_t expected = sizeof(uint32_t) + sizeof(int) +
        (W1_.size() + b1_.size() + W2_.size() + b2_.size()) * sizeof(float);
    if (blob.size() != expected) return false;

    size_t off = 0;
    uint32_t ver = *reinterpret_cast<const uint32_t*>(blob.data() + off);
    off += sizeof(ver);
    if (ver != 1) return false;

    update_count_ = *reinterpret_cast<const int*>(blob.data() + off);
    off += sizeof(update_count_);

    auto read = [&blob, &off](auto& arr) {
        std::memcpy(arr.data(), blob.data() + off, arr.size() * sizeof(float));
        off += arr.size() * sizeof(float);
    };
    read(W1_); read(b1_); read(W2_); read(b2_);

    initialized_ = true;
    return true;
}

uint64_t TinyMLP::computeWeightHash() const {
    uint64_t h = 0xCBF29CE484222325ULL;
    auto feed = [&h](const auto& arr) {
        const uint8_t* p = reinterpret_cast<const uint8_t*>(arr.data());
        for (size_t i = 0; i < arr.size() * sizeof(float); ++i) {
            h ^= p[i]; h *= 0x100000001B3ULL;
        }
    };
    feed(W1_); feed(b1_); feed(W2_); feed(b2_);
    return h;
}

void TinyMLP::clipGradient(float& g) {
    if (g > GRADIENT_CLIP) g = GRADIENT_CLIP;
    else if (g < -GRADIENT_CLIP) g = -GRADIENT_CLIP;
    else if (std::isnan(g) || std::isinf(g)) g = 0.0f;
}

bool TinyMLP::isFinite(float x) {
    return std::isfinite(x);
}

} // namespace yuki::memory
