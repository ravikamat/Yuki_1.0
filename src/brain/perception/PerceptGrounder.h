#pragma once
// PerceptGrounder — Interface for grounding symbolic observations from perception
// Supports both symbolic (simulator) and encoder-based (learned) grounding

#include "tests/cogni1/GridworldSimulator.h"
#include <vector>
#include <string>
#include <optional>

namespace yuki {
namespace brain {
namespace perception {

struct GroundedSymbol {
    std::string symbol;      // e.g., "on(block_1, block_2)"
    float confidence = 0.0f; // [0, 1]
    std::string source;      // "symbolic", "encoder", "hybrid"
};

using GroundingResult = std::vector<GroundedSymbol>;

class PerceptGrounder {
public:
    virtual ~PerceptGrounder() = default;

    // Ground raw observation into symbolic facts
    virtual GroundingResult ground(const test::cogni1::GridworldObservation& obs) = 0;

    // Calibrate confidence estimates using ground truth
    // predictions: model outputs, ground_truth: actual symbols
    virtual void calibrate(const std::vector<GroundingResult>& predictions,
                           const std::vector<std::vector<std::string>>& ground_truth) = 0;

    // Get calibration metrics
    struct CalibrationMetrics {
        float ece = 0.0f;           // Expected Calibration Error
        float mce = 0.0f;           // Maximum Calibration Error
        std::unordered_map<std::string, float> per_symbol_ece;
    };
    virtual CalibrationMetrics getCalibrationMetrics() const = 0;

    // Get grounder name
    virtual std::string name() const = 0;
};

// SymbolicGrounder: Uses simulator's symbolic observation + noise/dropout layer
class SymbolicGrounder : public PerceptGrounder {
public:
    struct Config {
        float dropout_rate = 0.0f;       // Random symbol dropout
        float gaussian_noise = 0.0f;     // Noise on continuous values
        float confusion_prob = 0.0f;     // Probability of wrong symbol
        bool add_spurious = false;       // Add spurious symbols
    };

    explicit SymbolicGrounder(const Config& cfg = Config{})
        : config_(cfg), rng_(std::random_device{}()) {}

    GroundingResult ground(const test::cogni1::GridworldObservation& obs) override {
        GroundingResult result;
        
        // Convert simulator facts to grounded symbols
        for (const auto& fact : obs.facts) {
            float conf = 1.0f;
            
            // Apply dropout
            if (dropout_dist_(rng_) < config_.dropout_rate) {
                continue; // Drop this symbol
            }
            
            // Apply confusion
            std::string symbol = fact;
            if (confusion_dist_(rng_) < config_.confusion_prob) {
                symbol = corruptSymbol(fact);
                conf *= 0.5f;
            }
            
            // Confidence with noise
            conf *= (1.0f - config_.gaussian_noise * std::abs(noise_dist_(rng_)));
            conf = std::clamp(conf, 0.0f, 1.0f);
            
            result.push_back({symbol, conf, "symbolic"});
        }
        
        // Add spurious symbols
        if (config_.add_spurious && spurious_dist_(rng_) < 0.1f) {
            result.push_back({"spurious_fact", 0.3f, "symbolic"});
        }
        
        return result;
    }

    void calibrate(const std::vector<GroundingResult>& predictions,
                   const std::vector<std::vector<std::string>>& ground_truth) override {
        // Symbolic grounder is already well-calibrated (deterministic)
        // Just compute ECE for reporting
        computeECE(predictions, ground_truth);
    }

    CalibrationMetrics getCalibrationMetrics() const override {
        return metrics_;
    }

    std::string name() const override { return "SymbolicGrounder"; }

    void setConfig(const Config& cfg) { config_ = cfg; }

private:
    Config config_;
    mutable std::mt19937 rng_;
    std::uniform_real_distribution<float> dropout_dist_{0.0f, 1.0f};
    std::uniform_real_distribution<float> confusion_dist_{0.0f, 1.0f};
    std::uniform_real_distribution<float> spurious_dist_{0.0f, 1.0f};
    std::normal_distribution<float> noise_dist_{0.0f, 1.0f};
    mutable CalibrationMetrics metrics_;

    std::string corruptSymbol(const std::string& symbol) {
        // Simple corruption: swap predicate or args
        static const std::vector<std::string> predicates = {"on", "clear", "holding", "at"};
        static const std::vector<std::string> blocks = {"block_1", "block_2", "block_3", "block_4", "block_5"};
        
        std::string corrupted = symbol;
        if (uniform_dist_(rng_) < 0.5f) {
            // Change predicate
            corrupted.replace(0, corrupted.find('('), predicates[uniform_int_dist_(rng_)]);
        } else {
            // Change argument
            size_t paren = corrupted.find('(');
            if (paren != std::string::npos) {
                size_t comma = corrupted.find(',', paren);
                if (comma != std::string::npos) {
                    corrupted.replace(paren + 1, comma - paren - 1, blocks[uniform_int_dist_(rng_)]);
                }
            }
        }
        return corrupted;
    }

    void computeECE(const std::vector<GroundingResult>& predictions,
                    const std::vector<std::vector<std::string>>& ground_truth) {
        const int num_bins = 10;
        std::vector<int> bin_counts(num_bins, 0);
        std::vector<float> bin_confidences(num_bins, 0.0f);
        std::vector<float> bin_accuracies(num_bins, 0.0f);
        
        for (size_t i = 0; i < predictions.size() && i < ground_truth.size(); ++i) {
            std::unordered_set<std::string> gt_set(ground_truth[i].begin(), ground_truth[i].end());
            
            for (const auto& pred : predictions[i]) {
                int bin = static_cast<int>(pred.confidence * num_bins);
                bin = std::clamp(bin, 0, num_bins - 1);
                
                bin_counts[bin]++;
                bin_confidences[bin] += pred.confidence;
                
                bool correct = gt_set.count(pred.symbol) > 0;
                bin_accuracies[bin] += correct ? 1.0f : 0.0f;
            }
        }
        
        float ece = 0.0f;
        float mce = 0.0f;
        int total = 0;
        
        for (int b = 0; b < num_bins; ++b) {
            if (bin_counts[b] > 0) {
                float avg_conf = bin_confidences[b] / bin_counts[b];
                float avg_acc = bin_accuracies[b] / bin_counts[b];
                float bin_ece = std::abs(avg_conf - avg_acc);
                ece += bin_ece * bin_counts[b];
                mce = std::max(mce, bin_ece);
                total += bin_counts[b];
            }
        }
        
        if (total > 0) ece /= total;
        
        metrics_.ece = ece;
        metrics_.mce = mce;
    }

    std::uniform_real_distribution<float> uniform_dist_{0.0f, 1.0f};
    std::uniform_int_distribution<int> uniform_int_dist_{0, 4};
};

// EncoderGrounder: Pretrained encoder + prototype classifier + calibration
class EncoderGrounder : public PerceptGrounder {
public:
    struct Config {
        int embedding_dim = 64;
        int num_prototypes = 20;
        float temperature = 1.0f;        // Temperature scaling
        bool use_isotonic = false;       // Use isotonic regression instead
    };

    explicit EncoderGrounder(const Config& cfg = Config{})
        : config_(cfg), rng_(std::random_device{}()) {}

    GroundingResult ground(const test::cogni1::GridworldObservation& obs) override {
        GroundingResult result;
        
        // Encode observation (state + visual features)
        auto embedding = encodeObservation(obs);
        
        // Classify against prototypes
        for (size_t i = 0; i < prototypes_.size(); ++i) {
            float sim = cosineSimilarity(embedding, prototypes_[i]);
            float conf = softmax(sim / config_.temperature);
            
            if (conf > 0.1f) { // Threshold
                result.push_back({prototype_labels_[i], conf, "encoder"});
            }
        }
        
        // Also include symbolic facts with high confidence
        for (const auto& fact : obs.facts) {
            result.push_back({fact, 0.9f, "hybrid"});
        }
        
        return result;
    }

    void calibrate(const std::vector<GroundingResult>& predictions,
                   const std::vector<std::vector<std::string>>& ground_truth) override {
        if (config_.use_isotonic) {
            fitIsotonicRegression(predictions, ground_truth);
        } else {
            fitTemperatureScaling(predictions, ground_truth);
        }
        computeECE(predictions, ground_truth);
    }

    CalibrationMetrics getCalibrationMetrics() const override {
        return metrics_;
    }

    std::string name() const override { return "EncoderGrounder"; }

    // Add prototype for a symbol
    void addPrototype(const std::string& label, const std::vector<float>& embedding) {
        if (prototypes_.size() < config_.num_prototypes) {
            prototypes_.push_back(embedding);
            prototype_labels_.push_back(label);
        }
    }

    // Train prototypes from labeled data
    void trainPrototypes(const std::vector<std::pair<std::vector<float>, std::string>>& labeled_data) {
        // Simple k-means or prototype averaging per class
        std::unordered_map<std::string, std::vector<std::vector<float>>> class_embeddings;
        
        for (const auto& [emb, label] : labeled_data) {
            class_embeddings[label].push_back(emb);
        }
        
        prototypes_.clear();
        prototype_labels_.clear();
        
        for (const auto& [label, embeddings] : class_embeddings) {
            std::vector<float> mean_emb(config_.embedding_dim, 0.0f);
            for (const auto& emb : embeddings) {
                for (int i = 0; i < config_.embedding_dim; ++i) {
                    mean_emb[i] += emb[i];
                }
            }
            for (int i = 0; i < config_.embedding_dim; ++i) {
                mean_emb[i] /= embeddings.size();
            }
            prototypes_.push_back(mean_emb);
            prototype_labels_.push_back(label);
        }
    }

private:
    Config config_;
    mutable std::mt19937 rng_;
    std::vector<std::vector<float>> prototypes_;
    std::vector<std::string> prototype_labels_;
    std::vector<float> isotonic_map_; // For isotonic regression
    float temperature_ = 1.0f;
    mutable CalibrationMetrics metrics_;

    std::vector<float> encodeObservation(const test::cogni1::GridworldObservation& obs) {
        // Encode state as fixed-size vector
        std::vector<float> emb(config_.embedding_dim, 0.0f);
        
        // Encode facts as bag-of-words over predicate-argument pairs
        int idx = 0;
        for (const auto& fact : obs.facts) {
            size_t hash = std::hash<std::string>{}(fact);
            emb[idx % config_.embedding_dim] += 1.0f;
            idx++;
        }
        
        // Encode gripper position
        if (idx < config_.embedding_dim) {
            emb[idx++] = static_cast<float>(obs.gripper_pos.x) / 4.0f; // Normalize to [0,1]
        }
        if (idx < config_.embedding_dim) {
            emb[idx++] = static_cast<float>(obs.gripper_pos.y) / 4.0f;
        }
        
        // Held block
        if (!obs.held_block.empty()) {
            size_t hash = std::hash<std::string>{}(obs.held_block);
            emb[idx % config_.embedding_dim] += 1.0f;
        }
        
        // Normalize
        float norm = 0.0f;
        for (float v : emb) norm += v * v;
        norm = std::sqrt(norm) + 1e-8f;
        for (float& v : emb) v /= norm;
        
        return emb;
    }

    float cosineSimilarity(const std::vector<float>& a, const std::vector<float>& b) {
        float dot = 0.0f, na = 0.0f, nb = 0.0f;
        for (size_t i = 0; i < a.size() && i < b.size(); ++i) {
            dot += a[i] * b[i];
            na += a[i] * a[i];
            nb += b[i] * b[i];
        }
        return dot / (std::sqrt(na) * std::sqrt(nb) + 1e-8f);
    }

    float softmax(float x) {
        return 1.0f / (1.0f + std::exp(-x));
    }

    void fitTemperatureScaling(const std::vector<GroundingResult>& predictions,
                               const std::vector<std::vector<std::string>>& ground_truth) {
        // Simple temperature scaling: find T that minimizes NLL
        // Using gradient-free search for simplicity
        float best_T = 1.0f;
        float best_nll = std::numeric_limits<float>::infinity();
        
        for (float T = 0.1f; T <= 5.0f; T += 0.1f) {
            float nll = 0.0f;
            int count = 0;
            
            for (size_t i = 0; i < predictions.size() && i < ground_truth.size(); ++i) {
                std::unordered_set<std::string> gt_set(ground_truth[i].begin(), ground_truth[i].end());
                
                for (const auto& pred : predictions[i]) {
                    float p = softmax(std::log(pred.confidence + 1e-8f) / T);
                    bool correct = gt_set.count(pred.symbol) > 0;
                    nll += correct ? -std::log(p + 1e-8f) : -std::log(1.0f - p + 1e-8f);
                    count++;
                }
            }
            
            if (count > 0 && nll / count < best_nll) {
                best_nll = nll / count;
                best_T = T;
            }
        }
        
        temperature_ = best_T;
    }

    void fitIsotonicRegression(const std::vector<GroundingResult>& predictions,
                               const std::vector<std::vector<std::string>>& ground_truth) {
        // Collect (confidence, correctness) pairs
        std::vector<std::pair<float, bool>> pairs;
        
        for (size_t i = 0; i < predictions.size() && i < ground_truth.size(); ++i) {
            std::unordered_set<std::string> gt_set(ground_truth[i].begin(), ground_truth[i].end());
            
            for (const auto& pred : predictions[i]) {
                pairs.emplace_back(pred.confidence, gt_set.count(pred.symbol) > 0);
            }
        }
        
        // Sort by confidence
        std::sort(pairs.begin(), pairs.end(),
            [](const auto& a, const auto& b) { return a.first < b.first; });
        
        // Pool Adjacent Violators Algorithm (PAVA)
        const int num_bins = 100;
        isotonic_map_.resize(num_bins);
        
        // Simple binning + PAVA
        std::vector<int> bin_counts(num_bins, 0);
        std::vector<float> bin_sums(num_bins, 0.0f);
        
        for (const auto& [conf, correct] : pairs) {
            int bin = static_cast<int>(conf * num_bins);
            bin = std::clamp(bin, 0, num_bins - 1);
            bin_counts[bin]++;
            bin_sums[bin] += correct ? 1.0f : 0.0f;
        }
        
        // Compute empirical probabilities per bin
        for (int b = 0; b < num_bins; ++b) {
            if (bin_counts[b] > 0) {
                isotonic_map_[b] = bin_sums[b] / bin_counts[b];
            } else {
                isotonic_map_[b] = static_cast<float>(b) / num_bins;
            }
        }
        
        // PAVA: ensure monotonicity
        for (int b = 1; b < num_bins; ++b) {
            if (isotonic_map_[b] < isotonic_map_[b - 1]) {
                isotonic_map_[b] = isotonic_map_[b - 1];
            }
        }
    }

    float applyCalibration(float conf) const {
        if (config_.use_isotonic && !isotonic_map_.empty()) {
            int bin = static_cast<int>(conf * (isotonic_map_.size() - 1));
            bin = std::clamp(bin, 0, static_cast<int>(isotonic_map_.size()) - 1);
            return isotonic_map_[bin];
        } else {
            return softmax(std::log(conf + 1e-8f) / temperature_);
        }
    }

    void computeECE(const std::vector<GroundingResult>& predictions,
                    const std::vector<std::vector<std::string>>& ground_truth) {
        // Same as SymbolicGrounder
        const int num_bins = 10;
        std::vector<int> bin_counts(num_bins, 0);
        std::vector<float> bin_confidences(num_bins, 0.0f);
        std::vector<float> bin_accuracies(num_bins, 0.0f);
        
        for (size_t i = 0; i < predictions.size() && i < ground_truth.size(); ++i) {
            std::unordered_set<std::string> gt_set(ground_truth[i].begin(), ground_truth[i].end());
            
            for (const auto& pred : predictions[i]) {
                float cal_conf = applyCalibration(pred.confidence);
                int bin = static_cast<int>(cal_conf * num_bins);
                bin = std::clamp(bin, 0, num_bins - 1);
                
                bin_counts[bin]++;
                bin_confidences[bin] += cal_conf;
                bin_accuracies[bin] += gt_set.count(pred.symbol) > 0 ? 1.0f : 0.0f;
            }
        }
        
        float ece = 0.0f;
        float mce = 0.0f;
        int total = 0;
        
        for (int b = 0; b < num_bins; ++b) {
            if (bin_counts[b] > 0) {
                float avg_conf = bin_confidences[b] / bin_counts[b];
                float avg_acc = bin_accuracies[b] / bin_counts[b];
                float bin_ece = std::abs(avg_conf - avg_acc);
                ece += bin_ece * bin_counts[b];
                mce = std::max(mce, bin_ece);
                total += bin_counts[b];
            }
        }
        
        if (total > 0) ece /= total;
        
        metrics_.ece = ece;
        metrics_.mce = mce;
    }
};

} // namespace perception
} // namespace brain
} // namespace yuki