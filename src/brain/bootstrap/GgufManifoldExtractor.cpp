#include "GgufManifoldExtractor.h"
#include "brain/core/Logger.h"
#include "gguf.h"
#include "ggml.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <numeric>
#include <random>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <unordered_set>
#include <thread>
#include <atomic>
#include <mutex>
#include <nlohmann/json.hpp>

namespace yuki { namespace bootstrap {

namespace {

std::string cleanTokenString(const std::string& raw) {
    if (raw.empty()) return "";
    std::string s = raw;
    // Replace GPT-2 / Byte-level BPE space marker \xC4\xA0 (Ġ) or   with regular space or strip it
    if (s.size() >= 2 && static_cast<unsigned char>(s[0]) == 0xC4 && static_cast<unsigned char>(s[1]) == 0xA0) {
        s = s.substr(2);
    } else if (s.size() >= 3 && static_cast<unsigned char>(s[0]) == 0xE2 && static_cast<unsigned char>(s[1]) == 0x96 && static_cast<unsigned char>(s[2]) == 0x81) {
        s = s.substr(3); // SentencePiece   marker
    }
    
    // Trim leading/trailing whitespace
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\r' || s.front() == '\n')) {
        s.erase(s.begin());
    }
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r' || s.back() == '\n')) {
        s.pop_back();
    }
    return s;
}

bool isValidConceptToken(const std::string& token) {
    if (token.size() < 2 || token.size() > 32) return false;
    if (token.front() == '<' && token.back() == '>') return false; // special token <|...|>
    if (token.front() == '[' && token.back() == ']') return false;
    
    // Check printable ASCII
    for (char c : token) {
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-')) {
            return false;
        }
    }
    return true;
}

} // anonymous namespace

class GgufManifoldExtractor::Impl {
public:
    std::string gguf_path_;
    struct gguf_context* ctx_ = nullptr;
    std::vector<std::string> tokens_;
    int64_t tensor_id_ = -1;
    uint32_t vocab_size_ = 0;
    uint32_t embedding_dim_ = 0;
    enum ggml_type tensor_type_ = GGML_TYPE_F32;
    size_t tensor_data_offset_ = 0;
    size_t row_bytes_ = 0;

    ~Impl() {
        if (ctx_) {
            gguf_free(ctx_);
            ctx_ = nullptr;
        }
    }

    bool loadGGUF() {
        if (!std::filesystem::exists(gguf_path_)) {
            yuki::core::Logger::instance().log(yuki::core::LogLevel::ERROR, "GgufManifoldExtractor",
                "File does not exist: " + gguf_path_);
            return false;
        }

        struct gguf_init_params params;
        params.no_alloc = true;
        params.ctx = nullptr;

        ctx_ = gguf_init_from_file(gguf_path_.c_str(), params);
        if (!ctx_) {
            yuki::core::Logger::instance().log(yuki::core::LogLevel::ERROR, "GgufManifoldExtractor",
                "Failed to parse GGUF header from: " + gguf_path_);
            return false;
        }

        // Read tokenizer vocabulary
        int64_t key_tokens = gguf_find_key(ctx_, "tokenizer.ggml.tokens");
        if (key_tokens >= 0) {
            vocab_size_ = static_cast<uint32_t>(gguf_get_arr_n(ctx_, key_tokens));
            tokens_.reserve(vocab_size_);
            for (size_t i = 0; i < vocab_size_; ++i) {
                const char* tok = gguf_get_arr_str(ctx_, key_tokens, i);
                tokens_.push_back(tok ? std::string(tok) : "");
            }
        }

        // Find embedding tensor: token_embd.weight or output.weight
        tensor_id_ = gguf_find_tensor(ctx_, "token_embd.weight");
        if (tensor_id_ < 0) {
            tensor_id_ = gguf_find_tensor(ctx_, "model.embed_tokens.weight");
        }
        if (tensor_id_ < 0) {
            tensor_id_ = gguf_find_tensor(ctx_, "output.weight");
        }

        if (tensor_id_ < 0) {
            yuki::core::Logger::instance().log(yuki::core::LogLevel::ERROR, "GgufManifoldExtractor",
                "Embedding tensor (token_embd.weight / output.weight) not found in GGUF");
            return false;
        }

        const int64_t* ne = gguf_get_tensor_ne(ctx_, tensor_id_);
        embedding_dim_ = static_cast<uint32_t>(ne[0]);
        uint32_t tensor_rows = static_cast<uint32_t>(ne[1]);
        if (vocab_size_ == 0) {
            vocab_size_ = tensor_rows;
            tokens_.resize(vocab_size_);
            for (uint32_t i = 0; i < vocab_size_; ++i) {
                tokens_[i] = "token_" + std::to_string(i);
            }
        }

        tensor_type_ = gguf_get_tensor_type(ctx_, tensor_id_);
        tensor_data_offset_ = gguf_get_data_offset(ctx_) + gguf_get_tensor_offset(ctx_, tensor_id_);
        row_bytes_ = ggml_row_size(tensor_type_, embedding_dim_);

        yuki::core::Logger::instance().log(yuki::core::LogLevel::INFO, "GgufManifoldExtractor",
            "GGUF loaded: vocab_size=" + std::to_string(vocab_size_) +
            ", dim=" + std::to_string(embedding_dim_) +
            ", type=" + std::to_string(tensor_type_) +
            ", row_bytes=" + std::to_string(row_bytes_));

        return vocab_size_ > 0 && embedding_dim_ > 0 && row_bytes_ > 0;
    }

    bool readEmbeddingRow(std::ifstream& file, uint32_t row_idx, std::vector<float>& out_vec) {
        if (row_idx >= vocab_size_) return false;
        out_vec.resize(embedding_dim_);

        uint64_t file_pos = tensor_data_offset_ + static_cast<uint64_t>(row_idx) * row_bytes_;
        file.seekg(static_cast<std::streamoff>(file_pos));
        if (!file) return false;

        std::vector<uint8_t> raw_buf(row_bytes_);
        file.read(reinterpret_cast<char*>(raw_buf.data()), static_cast<std::streamsize>(row_bytes_));
        if (!file) return false;

        // Dequantize row
        if (tensor_type_ == GGML_TYPE_F32) {
            std::memcpy(out_vec.data(), raw_buf.data(), embedding_dim_ * sizeof(float));
        } else if (tensor_type_ == GGML_TYPE_F16) {
            ggml_fp16_to_fp32_row(reinterpret_cast<const ggml_fp16_t*>(raw_buf.data()), out_vec.data(), embedding_dim_);
        } else if (tensor_type_ == GGML_TYPE_BF16) {
            ggml_bf16_to_fp32_row(reinterpret_cast<const ggml_bf16_t*>(raw_buf.data()), out_vec.data(), embedding_dim_);
        } else {
            auto traits = ggml_get_type_traits(tensor_type_);
            if (traits && traits->to_float) {
                traits->to_float(raw_buf.data(), out_vec.data(), embedding_dim_);
            } else {
                return false;
            }
        }

        // Normalize embedding
        float sum_sq = 0.0f;
        for (float v : out_vec) sum_sq += v * v;
        float norm = std::sqrt(sum_sq);
        if (norm > 1e-8f) {
            float inv_norm = 1.0f / norm;
            for (float& v : out_vec) v *= inv_norm;
        }

        return true;
    }

    float computeEntropy(const std::vector<float>& similarities) const {
        float sum = 0.0f;
        for (float s : similarities) {
            if (s > 0.0f) sum += s;
        }
        if (sum < 1e-6f) return 0.0f;
        float entropy = 0.0f;
        for (float s : similarities) {
            if (s > 0.0f) {
                float p = s / sum;
                if (p > 1e-8f) {
                    entropy -= p * std::log(p);
                }
            }
        }
        return entropy;
    }

    std::optional<BootstrapPackage> extractPackage(uint32_t max_manifolds, float entropy_threshold) {
        BootstrapPackage pkg;
        pkg.model_name = std::filesystem::path(gguf_path_).filename().string();
        pkg.extraction_timestamp = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
        pkg.vocab_size = vocab_size_;
        pkg.embedding_dim = embedding_dim_;

        // 1. Select candidate concept tokens
        struct ConceptCandidate {
            uint32_t vocab_index = 0;
            std::string clean_name;
            std::vector<float> embedding;
        };

        std::vector<ConceptCandidate> candidates;
        candidates.reserve(std::min(max_manifolds * 2, vocab_size_));

        std::ifstream file(gguf_path_, std::ios::binary);
        if (!file) {
            yuki::core::Logger::instance().log(yuki::core::LogLevel::ERROR, "GgufManifoldExtractor",
                "Cannot open GGUF file for reading: " + gguf_path_);
            return std::nullopt;
        }

        std::unordered_set<std::string> seen_names;

        std::cout << "[GgufManifoldExtractor] Filtering candidate vocabulary tokens...\n";
        for (uint32_t i = 0; i < vocab_size_ && candidates.size() < max_manifolds * 2; ++i) {
            std::string cleaned = cleanTokenString(tokens_[i]);
            if (isValidConceptToken(cleaned)) {
                std::string lower = cleaned;
                std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                if (seen_names.find(lower) == seen_names.end()) {
                    seen_names.insert(lower);
                    ConceptCandidate cand;
                    cand.vocab_index = i;
                    cand.clean_name = lower;
                    if (readEmbeddingRow(file, i, cand.embedding)) {
                        candidates.push_back(std::move(cand));
                    }
                }
            }
        }

        if (candidates.empty()) {
            // Fallback to sequential first N rows if token filtering was overly strict
            for (uint32_t i = 0; i < std::min(max_manifolds, vocab_size_); ++i) {
                ConceptCandidate cand;
                cand.vocab_index = i;
                cand.clean_name = tokens_[i].empty() ? ("token_" + std::to_string(i)) : cleanTokenString(tokens_[i]);
                if (cand.clean_name.empty()) cand.clean_name = "token_" + std::to_string(i);
                if (readEmbeddingRow(file, i, cand.embedding)) {
                    candidates.push_back(std::move(cand));
                }
            }
        }

        size_t n_concepts = std::min<size_t>(max_manifolds, candidates.size());
        std::cout << "[GgufManifoldExtractor] Selected " << candidates.size() << " concept candidates. Computing manifolds for top " << n_concepts << "...\n";

        pkg.manifolds.reserve(n_concepts);

        // Precompute candidate similarities with multi-threading
        const size_t total_cand = candidates.size();
        const size_t num_threads = std::max<size_t>(1, std::thread::hardware_concurrency());
        std::vector<std::vector<SemanticManifold>> thread_results(num_threads);
        for (auto& r : thread_results) {
            r.reserve(n_concepts / num_threads + 1);
        }

        std::atomic<size_t> next_index{0};
        std::atomic<size_t> completed{0};
        std::mutex log_mtx;

        std::vector<std::thread> workers;
        workers.reserve(num_threads);

        for (size_t t = 0; t < num_threads; ++t) {
            workers.emplace_back([&, t]() {
                while (true) {
                    size_t i = next_index.fetch_add(1);
                    if (i >= n_concepts) break;

                    const auto& src = candidates[i];
                    std::vector<std::pair<float, size_t>> sim_scores;
                    sim_scores.reserve(total_cand);

                    std::vector<float> sim_values;
                    sim_values.reserve(total_cand);

                    for (size_t j = 0; j < total_cand; ++j) {
                        float dot = 0.0f;
                        const float* a = src.embedding.data();
                        const float* b = candidates[j].embedding.data();
                        for (uint32_t d = 0; d < embedding_dim_; ++d) {
                            dot += a[d] * b[d];
                        }
                        sim_values.push_back(dot);
                        if (i != j) {
                            sim_scores.emplace_back(dot, j);
                        }
                    }

                    // Top-50 nearest neighbors
                    size_t top_k = std::min<size_t>(50, sim_scores.size());
                    std::partial_sort(sim_scores.begin(), sim_scores.begin() + top_k, sim_scores.end(),
                        [](const auto& x, const auto& y) { return x.first > y.first; });

                    SemanticManifold manifold;
                    manifold.concept_name = src.clean_name;
                    manifold.vocab_index = src.vocab_index;
                    manifold.centroid = src.embedding;
                    manifold.entropy = computeEntropy(sim_values);

                    for (size_t k = 0; k < top_k; ++k) {
                        float sim = sim_scores[k].first;
                        if (sim > 0.15f) {
                            manifold.neighbors.emplace_back(candidates[sim_scores[k].second].clean_name, sim);
                        }
                    }

                    if (manifold.entropy >= entropy_threshold || manifold.neighbors.size() >= 3) {
                        thread_results[t].push_back(std::move(manifold));
                    }

                    size_t c = completed.fetch_add(1) + 1;
                    if (c % 1000 == 0 || c == n_concepts) {
                        std::lock_guard<std::mutex> lock(log_mtx);
                        std::cout << "[GgufManifoldExtractor] Progress: " 
                                  << (c * 100 / n_concepts) << "% (" << c << "/" << n_concepts << " concepts computed)...\n";
                    }
                }
            });
        }

        for (auto& w : workers) {
            if (w.joinable()) w.join();
        }

        for (auto& r : thread_results) {
            for (auto& m : r) {
                pkg.manifolds.push_back(std::move(m));
            }
        }

        std::cout << "[GgufManifoldExtractor] Extracted " << pkg.manifolds.size() << " semantic manifolds.\n";

        // Domain clusters
        pkg.domain_clusters = extractDomainClustersInternal(pkg.manifolds, 32);

        // Syntactic patterns
        pkg.syntactic_patterns = extractSyntacticPatternsInternal();

        return pkg;
    }

    std::unordered_map<std::string, std::vector<std::string>> extractDomainClustersInternal(
        const std::vector<SemanticManifold>& manifolds, uint32_t k) {
        std::unordered_map<std::string, std::vector<std::string>> clusters;
        if (manifolds.empty()) return clusters;
        k = std::min<uint32_t>(k, static_cast<uint32_t>(manifolds.size()));

        std::vector<std::vector<float>> centroids(k, std::vector<float>(embedding_dim_, 0.0f));
        std::mt19937 rng(42);
        std::uniform_int_distribution<size_t> dist(0, manifolds.size() - 1);

        for (uint32_t c = 0; c < k; ++c) {
            centroids[c] = manifolds[dist(rng)].centroid;
        }

        std::vector<uint32_t> assignments(manifolds.size(), 0);

        // Run 8 iterations of k-means
        for (int iter = 0; iter < 8; ++iter) {
            for (size_t i = 0; i < manifolds.size(); ++i) {
                float best_sim = -1e9f;
                uint32_t best_c = 0;
                for (uint32_t c = 0; c < k; ++c) {
                    float dot = 0.0f;
                    for (uint32_t d = 0; d < embedding_dim_; ++d) {
                        dot += manifolds[i].centroid[d] * centroids[c][d];
                    }
                    if (dot > best_sim) {
                        best_sim = dot;
                        best_c = c;
                    }
                }
                assignments[i] = best_c;
            }

            // Update centroids
            std::vector<uint32_t> counts(k, 0);
            std::fill(centroids.begin(), centroids.end(), std::vector<float>(embedding_dim_, 0.0f));

            for (size_t i = 0; i < manifolds.size(); ++i) {
                uint32_t c = assignments[i];
                counts[c]++;
                for (uint32_t d = 0; d < embedding_dim_; ++d) {
                    centroids[c][d] += manifolds[i].centroid[d];
                }
            }

            for (uint32_t c = 0; c < k; ++c) {
                if (counts[c] > 0) {
                    float norm = 0.0f;
                    for (uint32_t d = 0; d < embedding_dim_; ++d) {
                        centroids[c][d] /= counts[c];
                        norm += centroids[c][d] * centroids[c][d];
                    }
                    norm = std::sqrt(norm);
                    if (norm > 1e-8f) {
                        for (uint32_t d = 0; d < embedding_dim_; ++d) centroids[c][d] /= norm;
                    }
                }
            }
        }

        // Domain names
        const std::vector<std::string> domain_names = {
            "core_logic", "mathematics", "computer_science", "physics", "linguistics",
            "metacognition", "perception", "action_planning", "memory_fabric", "autonomous_learning",
            "code_synthesis", "natural_science", "system_control", "creativity", "ethics_governance",
            "dialogue_interaction", "causal_inference", "knowledge_representation", "tool_execution", "pattern_recognition",
            "spatial_reasoning", "temporal_dynamics", "probabilistic_modeling", "graph_topology", "signal_processing",
            "decision_theory", "neuro_symbolic", "continuous_learning", "self_reflection", "domain_ontology",
            "semantic_relations", "symbolic_computation"
        };

        for (uint32_t c = 0; c < k; ++c) {
            std::string domain_tag = (c < domain_names.size()) ? domain_names[c] : ("domain_" + std::to_string(c));
            for (size_t i = 0; i < manifolds.size() && clusters[domain_tag].size() < 100; ++i) {
                if (assignments[i] == c) {
                    clusters[domain_tag].push_back(manifolds[i].concept_name);
                }
            }
        }

        return clusters;
    }

    std::vector<std::string> extractSyntacticPatternsInternal() {
        return {
            "verb + me + noun",
            "generate + noun",
            "create + noun",
            "build + noun",
            "learn + noun",
            "how + to + verb",
            "what + is + noun",
            "explain + noun",
            "execute + action + on + target",
            "analyze + dataset + for + pattern",
            "solve + problem + using + method",
            "verify + hypothesis + with + evidence"
        };
    }

    bool savePackage(const std::string& path, const BootstrapPackage& pkg) {
        nlohmann::json j;
        j["model_name"] = pkg.model_name;
        j["extraction_timestamp"] = pkg.extraction_timestamp;
        j["vocab_size"] = pkg.vocab_size;
        j["embedding_dim"] = pkg.embedding_dim;
        j["manifold_count"] = pkg.manifolds.size();

        j["manifolds"] = nlohmann::json::array();
        for (const auto& m : pkg.manifolds) {
            nlohmann::json mf;
            mf["concept_name"] = m.concept_name;
            mf["vocab_index"] = m.vocab_index;
            mf["centroid"] = m.centroid;
            mf["entropy"] = m.entropy;
            mf["domain_tag"] = m.domain_tag;
            mf["neighbors"] = nlohmann::json::array();
            for (const auto& n : m.neighbors) {
                mf["neighbors"].push_back({{"token", n.first}, {"similarity", n.second}});
            }
            j["manifolds"].push_back(mf);
        }

        j["domain_clusters"] = pkg.domain_clusters;
        j["syntactic_patterns"] = pkg.syntactic_patterns;

        // Ensure parent directory exists
        auto parent = std::filesystem::path(path).parent_path();
        if (!parent.empty()) {
            std::filesystem::create_directories(parent);
        }

        std::ofstream out(path);
        if (!out) {
            yuki::core::Logger::instance().log(yuki::core::LogLevel::ERROR, "GgufManifoldExtractor",
                "Cannot open output file for writing: " + path);
            return false;
        }
        out << j.dump(2);
        return out.good();
    }

    std::optional<SemanticManifold> extractSingleManifold(const std::string& concept_token) {
        if (!ctx_) return std::nullopt;

        std::string target = concept_token;
        std::transform(target.begin(), target.end(), target.begin(), ::tolower);

        for (uint32_t i = 0; i < vocab_size_; ++i) {
            std::string cleaned = cleanTokenString(tokens_[i]);
            std::transform(cleaned.begin(), cleaned.end(), cleaned.begin(), ::tolower);
            if (cleaned == target) {
                std::vector<float> embedding;
                std::ifstream file(gguf_path_, std::ios::binary);
                if (!file || !readEmbeddingRow(file, i, embedding)) {
                    return std::nullopt;
                }

                SemanticManifold manifold;
                manifold.concept_name = cleaned;
                manifold.vocab_index = i;
                manifold.centroid = embedding;

                std::vector<std::pair<float, size_t>> sim_scores;
                sim_scores.reserve(vocab_size_);

                std::vector<float> sim_values;
                sim_values.reserve(vocab_size_);

                for (uint32_t j = 0; j < vocab_size_; ++j) {
                    float dot = 0.0f;
                    const float* a = embedding.data();
                    std::vector<float> b_vec;
                    if (!readEmbeddingRow(file, j, b_vec)) continue;
                    const float* b = b_vec.data();
                    for (uint32_t d = 0; d < embedding_dim_; ++d) {
                        dot += a[d] * b[d];
                    }
                    sim_values.push_back(dot);
                    if (i != j) {
                        sim_scores.emplace_back(dot, j);
                    }
                }

                size_t top_k = std::min<size_t>(50, sim_scores.size());
                std::partial_sort(sim_scores.begin(), sim_scores.begin() + top_k, sim_scores.end(),
                    [](const auto& x, const auto& y) { return x.first > y.first; });

                manifold.entropy = computeEntropy(sim_values);

                for (size_t k = 0; k < top_k; ++k) {
                    float sim = sim_scores[k].first;
                    if (sim > 0.15f) {
                        std::string neighbor_name = cleanTokenString(tokens_[sim_scores[k].second]);
                        std::transform(neighbor_name.begin(), neighbor_name.end(), neighbor_name.begin(), ::tolower);
                        manifold.neighbors.emplace_back(neighbor_name, sim);
                    }
                }

                if (!manifold.neighbors.empty()) {
                    manifold.domain_tag = "related_to_" + manifold.neighbors[0].first;
                }

                return manifold;
            }
        }

        return std::nullopt;
    }
};

GgufManifoldExtractor::GgufManifoldExtractor(const std::string& gguf_path)
    : pimpl_(std::make_unique<Impl>()) {
    pimpl_->gguf_path_ = gguf_path;
    if (!pimpl_->loadGGUF()) {
        valid_ = false;
        error_ = "Failed to load GGUF file: " + gguf_path;
        return;
    }
    valid_ = true;
}

GgufManifoldExtractor::~GgufManifoldExtractor() = default;

std::optional<BootstrapPackage> GgufManifoldExtractor::extractAll(uint32_t max_manifolds, float entropy_threshold) {
    if (!valid_ || !pimpl_) return std::nullopt;
    return pimpl_->extractPackage(max_manifolds, entropy_threshold);
}

std::optional<SemanticManifold> GgufManifoldExtractor::extractManifold(const std::string& concept_token) {
        if (!valid_ || !pimpl_) return std::nullopt;
        return pimpl_->extractSingleManifold(concept_token);
    }

std::vector<std::string> GgufManifoldExtractor::extractSyntacticPatterns(int) {
    if (!valid_ || !pimpl_) return {};
    return pimpl_->extractSyntacticPatternsInternal();
}

std::unordered_map<std::string, std::vector<std::string>> GgufManifoldExtractor::extractDomainClusters(uint32_t k) {
        if (!valid_ || !pimpl_) return {};
        // Extract all manifolds first, then cluster them
        auto pkg_opt = extractAll(50000, 0.1f);
        if (!pkg_opt.has_value()) return {};
        return pimpl_->extractDomainClustersInternal(pkg_opt->manifolds, k);
    }

bool GgufManifoldExtractor::saveToFile(const std::string& path, const BootstrapPackage& package) {
    if (!valid_ || !pimpl_) return false;
    return pimpl_->savePackage(path, package);
}

}} // namespace yuki::bootstrap