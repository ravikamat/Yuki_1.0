#include "Word2Vec.h"
#include "brain/core/Logger.h"
#include <fstream>
#include <sstream>
#include <cmath>
#include <algorithm>
#include <queue>
#include <limits>
#include <cctype>

namespace yuki::language {

using Matrix = yuki::learning::neural::Matrix;

static float sigmoidFast(float x) {
    if (x <= -6.0f) return 0.0f;
    if (x >= 6.0f) return 1.0f;
    return 1.0f / (1.0f + std::exp(-x));
}

Word2Vec::Word2Vec(const Config& cfg)
    : cfg_(cfg), rng_(cfg.seed)
{
}

Word2Vec::~Word2Vec() = default;

std::vector<std::string> Word2Vec::tokenize(const std::string& sentence) const {
    std::vector<std::string> tokens;
    std::istringstream iss(sentence);
    std::string word;
    while (iss >> word) {
        std::string cleaned;
        for (char c : word) {
            if (std::isalnum(static_cast<unsigned char>(c))) {
                cleaned.push_back(static_cast<char>(std::tolower(c)));
            }
        }
        if (!cleaned.empty()) {
            tokens.push_back(cleaned);
        }
    }
    return tokens;
}

void Word2Vec::buildVocabulary(const std::vector<std::string>& sentences) {
    std::lock_guard<std::mutex> lock(inference_mutex_);
    vocab_.clear();
    index_to_word_.clear();

    std::unordered_map<std::string, std::size_t> counts;
    for (const auto& sentence : sentences) {
        auto tokens = tokenize(sentence);
        for (const auto& tok : tokens) {
            counts[tok]++;
        }
    }

    std::size_t idx = 0;
    for (const auto& kv : counts) {
        if (kv.second >= cfg_.min_count) {
            VocabWord vw;
            vw.word = kv.first;
            vw.count = kv.second;
            vw.index = idx++;
            vocab_[kv.first] = vw;
            index_to_word_.push_back(kv.first);
        }
    }

    if (vocab_.empty()) {
        yuki::core::Logger::instance().log(yuki::core::LogLevel::WARN,
            "Word2Vec::buildVocabulary - Vocabulary is empty after min_count filtering.");
        return;
    }

    computeSubsampleProbs();
    buildNegativeSamplingTable();
    if (cfg_.use_hierarchical_softmax) {
        buildHuffmanTree();
    }

    // Xavier initialization for W_in: uniform(-scale, scale)
    float scale = std::sqrt(6.0f / static_cast<float>(cfg_.embedding_dim + cfg_.embedding_dim));
    input_weights_ = std::make_unique<Matrix>(vocab_.size(), cfg_.embedding_dim);
    std::uniform_real_distribution<float> dist(-scale, scale);
    for (std::size_t i = 0; i < input_weights_->rows; ++i) {
        for (std::size_t j = 0; j < input_weights_->cols; ++j) {
            (*input_weights_)(i, j) = dist(rng_);
        }
    }

    // W_out initialized to zeros
    output_weights_ = std::make_unique<Matrix>(vocab_.size(), cfg_.embedding_dim, 0.0f);

    yuki::core::Logger::instance().log(yuki::core::LogLevel::INFO,
        "Word2Vec vocabulary built with " + std::to_string(vocab_.size()) + " words.");
}

void Word2Vec::computeSubsampleProbs() {
    std::size_t total_count = 0;
    for (const auto& kv : vocab_) {
        total_count += kv.second.count;
    }
    if (total_count == 0) return;

    for (auto& kv : vocab_) {
        float f = static_cast<float>(kv.second.count) / static_cast<float>(total_count);
        float p = (std::sqrt(f / cfg_.subsample_threshold) + 1.0f) * (cfg_.subsample_threshold / f);
        kv.second.sample_prob = std::min(1.0f, p);
    }
}

void Word2Vec::buildNegativeSamplingTable() {
    neg_sampling_table_.clear();
    std::size_t table_size = std::max(static_cast<std::size_t>(1e7), vocab_.size() * 10);
    neg_sampling_table_.reserve(table_size);

    double total_pow = 0.0;
    for (const auto& w : index_to_word_) {
        total_pow += std::pow(vocab_[w].count, 0.75);
    }

    for (std::size_t i = 0; i < index_to_word_.size(); ++i) {
        double d1 = std::pow(vocab_[index_to_word_[i]].count, 0.75) / total_pow;
        std::size_t count = static_cast<std::size_t>(d1 * table_size);
        for (std::size_t c = 0; c < count && neg_sampling_table_.size() < table_size; ++c) {
            neg_sampling_table_.push_back(i);
        }
    }
    while (neg_sampling_table_.size() < table_size && !index_to_word_.empty()) {
        neg_sampling_table_.push_back(neg_sampling_table_.size() % index_to_word_.size());
    }
}

struct HuffmanNode {
    std::size_t count;
    int point; // index in internal nodes
    int word_idx; // index in vocab, -1 if internal
    std::shared_ptr<HuffmanNode> left;
    std::shared_ptr<HuffmanNode> right;
};

struct CompareHuffman {
    bool operator()(const std::shared_ptr<HuffmanNode>& a, const std::shared_ptr<HuffmanNode>& b) const {
        return a->count > b->count;
    }
};

void Word2Vec::buildHuffmanTree() {
    huffman_codes_.assign(vocab_.size(), std::vector<int>());
    huffman_points_.assign(vocab_.size(), std::vector<int>());

    std::priority_queue<std::shared_ptr<HuffmanNode>,
                         std::vector<std::shared_ptr<HuffmanNode>>,
                         CompareHuffman> pq;

    for (const auto& w : index_to_word_) {
        auto node = std::make_shared<HuffmanNode>();
        node->count = vocab_[w].count;
        node->point = -1;
        node->word_idx = static_cast<int>(vocab_[w].index);
        pq.push(node);
    }

    int internal_counter = 0;
    while (pq.size() > 1) {
        auto node1 = pq.top(); pq.pop();
        auto node2 = pq.top(); pq.pop();

        auto parent = std::make_shared<HuffmanNode>();
        parent->count = node1->count + node2->count;
        parent->point = internal_counter++;
        parent->word_idx = -1;
        parent->left = node1;
        parent->right = node2;
        pq.push(parent);
    }

    if (pq.empty()) return;
    auto root = pq.top();

    std::function<void(std::shared_ptr<HuffmanNode>, std::vector<int>, std::vector<int>)> traverse =
        [&](std::shared_ptr<HuffmanNode> node, std::vector<int> code, std::vector<int> points) {
            if (!node) return;
            if (node->word_idx != -1) {
                huffman_codes_[node->word_idx] = code;
                huffman_points_[node->word_idx] = points;
                return;
            }
            points.push_back(node->point);
            auto code_left = code;
            code_left.push_back(0);
            traverse(node->left, code_left, points);

            auto code_right = code;
            code_right.push_back(1);
            traverse(node->right, code_right, points);
        };

    traverse(root, {}, {});
}

std::size_t Word2Vec::sampleNegative() const {
    if (neg_sampling_table_.empty()) return 0;
    std::uniform_int_distribution<std::size_t> dist(0, neg_sampling_table_.size() - 1);
    return neg_sampling_table_[dist(rng_)];
}

void Word2Vec::train(const std::vector<std::string>& sentences) {
    if (vocab_.empty()) {
        buildVocabulary(sentences);
    }
    if (vocab_.empty() || !input_weights_ || !output_weights_) return;

    std::vector<std::vector<std::string>> tokenized_sentences;
    tokenized_sentences.reserve(sentences.size());
    std::size_t total_words = 0;

    for (const auto& s : sentences) {
        auto toks = tokenize(s);
        std::vector<std::string> valid_toks;
        for (const auto& t : toks) {
            if (vocab_.find(t) != vocab_.end()) {
                valid_toks.push_back(t);
            }
        }
        if (!valid_toks.empty()) {
            total_words += valid_toks.size();
            tokenized_sentences.push_back(std::move(valid_toks));
        }
    }

    if (total_words == 0) return;

    std::size_t words_processed = 0;
    float alpha = cfg_.learning_rate;
    float min_alpha = cfg_.learning_rate * 0.0001f;

    for (std::size_t epoch = 0; epoch < cfg_.epochs; ++epoch) {
        for (const auto& tokens : tokenized_sentences) {
            float progress = static_cast<float>(words_processed) / static_cast<float>(total_words * cfg_.epochs + 1);
            alpha = std::max(min_alpha, cfg_.learning_rate * (1.0f - progress));

            trainSentence(tokens, alpha);
            words_processed += tokens.size();
        }
    }

    yuki::core::Logger::instance().log(yuki::core::LogLevel::INFO,
        "Word2Vec training complete over " + std::to_string(cfg_.epochs) + " epochs.");
}

void Word2Vec::trainSentence(const std::vector<std::string>& tokens, float alpha) {
    std::uniform_real_distribution<float> rand_subsample(0.0f, 1.0f);
    std::size_t n = tokens.size();

    for (std::size_t i = 0; i < n; ++i) {
        const std::string& target_word = tokens[i];
        auto it_target = vocab_.find(target_word);
        if (it_target == vocab_.end()) continue;

        // Subsampling filter
        if (rand_subsample(rng_) > it_target->second.sample_prob) {
            continue;
        }

        int b = std::uniform_int_distribution<int>(0, static_cast<int>(cfg_.window_size) - 1)(rng_);
        std::size_t start = (i >= static_cast<std::size_t>(cfg_.window_size - b)) ? i - (cfg_.window_size - b) : 0;
        std::size_t end = std::min(n, i + cfg_.window_size - b + 1);

        for (std::size_t j = start; j < end; ++j) {
            if (i == j) continue;
            trainPair(target_word, tokens[j], alpha);
        }
    }
}

void Word2Vec::trainPair(const std::string& target, const std::string& context, float alpha) {
    auto it_t = vocab_.find(target);
    auto it_c = vocab_.find(context);
    if (it_t == vocab_.end() || it_c == vocab_.end()) return;

    if (cfg_.use_hierarchical_softmax) {
        trainPairHierarchicalSoftmax(it_t->second.index, it_c->second.index, alpha);
    } else {
        trainPairNegativeSampling(it_t->second.index, it_c->second.index, alpha);
    }
}

void Word2Vec::trainPairNegativeSampling(std::size_t target_idx, std::size_t context_idx, float alpha) {
    std::size_t dim = cfg_.embedding_dim;
    std::vector<float> target_gradient(dim, 0.0f);

    // Positive sample
    {
        float dot = 0.0f;
        for (std::size_t d = 0; d < dim; ++d) {
            dot += (*input_weights_)(target_idx, d) * (*output_weights_)(context_idx, d);
        }
        float sig = sigmoidFast(dot);
        float g = (1.0f - sig) * alpha;

        for (std::size_t d = 0; d < dim; ++d) {
            target_gradient[d] += g * (*output_weights_)(context_idx, d);
            (*output_weights_)(context_idx, d) += g * (*input_weights_)(target_idx, d);
        }
    }

    // Negative samples
    for (std::size_t k = 0; k < cfg_.negative_samples; ++k) {
        std::size_t neg_idx = sampleNegative();
        if (neg_idx == context_idx) continue;

        float dot = 0.0f;
        for (std::size_t d = 0; d < dim; ++d) {
            dot += (*input_weights_)(target_idx, d) * (*output_weights_)(neg_idx, d);
        }
        float sig = sigmoidFast(dot);
        float g = (0.0f - sig) * alpha;

        for (std::size_t d = 0; d < dim; ++d) {
            target_gradient[d] += g * (*output_weights_)(neg_idx, d);
            (*output_weights_)(neg_idx, d) += g * (*input_weights_)(target_idx, d);
        }
    }

    // Update target input weights
    for (std::size_t d = 0; d < dim; ++d) {
        (*input_weights_)(target_idx, d) += target_gradient[d];
    }
}

void Word2Vec::trainPairHierarchicalSoftmax(std::size_t target_idx, std::size_t context_idx, float alpha) {
    if (context_idx >= huffman_codes_.size()) return;
    const auto& code = huffman_codes_[context_idx];
    const auto& points = huffman_points_[context_idx];

    std::size_t dim = cfg_.embedding_dim;
    std::vector<float> target_gradient(dim, 0.0f);

    for (std::size_t i = 0; i < code.size(); ++i) {
        std::size_t point = static_cast<std::size_t>(points[i]);
        if (point >= output_weights_->rows) continue;

        float dot = 0.0f;
        for (std::size_t d = 0; d < dim; ++d) {
            dot += (*input_weights_)(target_idx, d) * (*output_weights_)(point, d);
        }
        float sig = sigmoidFast(dot);
        float label = static_cast<float>(1 - code[i]);
        float g = (label - sig) * alpha;

        for (std::size_t d = 0; d < dim; ++d) {
            target_gradient[d] += g * (*output_weights_)(point, d);
            (*output_weights_)(point, d) += g * (*input_weights_)(target_idx, d);
        }
    }

    for (std::size_t d = 0; d < dim; ++d) {
        (*input_weights_)(target_idx, d) += target_gradient[d];
    }
}

std::vector<float> Word2Vec::getVector(const std::string& word) const {
    std::lock_guard<std::mutex> lock(inference_mutex_);
    auto it = vocab_.find(word);
    if (it == vocab_.end() || !input_weights_) {
        return std::vector<float>(cfg_.embedding_dim, 0.0f);
    }
    return getVectorByIndex(it->second.index);
}

std::vector<float> Word2Vec::getVectorByIndex(std::size_t idx) const {
    std::vector<float> vec(cfg_.embedding_dim, 0.0f);
    if (!input_weights_ || idx >= input_weights_->rows) return vec;
    for (std::size_t d = 0; d < cfg_.embedding_dim; ++d) {
        vec[d] = (*input_weights_)(idx, d);
    }
    return vec;
}

bool Word2Vec::hasWord(const std::string& word) const {
    std::lock_guard<std::mutex> lock(inference_mutex_);
    return vocab_.find(word) != vocab_.end();
}

float Word2Vec::cosineSimilarity(const std::string& word1, const std::string& word2) const {
    std::lock_guard<std::mutex> lock(inference_mutex_);
    auto v1 = getVector(word1);
    auto v2 = getVector(word2);
    return cosineSimilarity(v1, v2);
}

float Word2Vec::cosineSimilarity(const std::vector<float>& vec1, const std::vector<float>& vec2) const {
    if (vec1.size() != vec2.size() || vec1.empty()) return 0.0f;
    float dot = 0.0f;
    float norm1 = 0.0f;
    float norm2 = 0.0f;

    for (std::size_t i = 0; i < vec1.size(); ++i) {
        dot += vec1[i] * vec2[i];
        norm1 += vec1[i] * vec1[i];
        norm2 += vec2[i] * vec2[i];
    }
    if (norm1 <= 0.0f || norm2 <= 0.0f) return 0.0f;
    return dot / (std::sqrt(norm1) * std::sqrt(norm2));
}

std::vector<std::pair<std::string, float>> Word2Vec::analogy(
    const std::string& word_a,
    const std::string& word_b,
    const std::string& word_c,
    std::size_t top_k) const
{
    std::lock_guard<std::mutex> lock(inference_mutex_);
    auto va = getVector(word_a);
    auto vb = getVector(word_b);
    auto vc = getVector(word_c);

    std::vector<float> target_vec(cfg_.embedding_dim, 0.0f);
    for (std::size_t d = 0; d < cfg_.embedding_dim; ++d) {
        target_vec[d] = vb[d] - va[d] + vc[d];
    }

    auto candidates = nearestNeighbors(target_vec, top_k + 3);
    std::vector<std::pair<std::string, float>> results;

    for (const auto& p : candidates) {
        if (p.first != word_a && p.first != word_b && p.first != word_c) {
            results.push_back(p);
            if (results.size() >= top_k) break;
        }
    }
    return results;
}

std::vector<std::pair<std::string, float>> Word2Vec::nearestNeighbors(
    const std::string& word,
    std::size_t top_k) const
{
    auto vec = getVector(word);
    return nearestNeighbors(vec, top_k);
}

std::vector<std::pair<std::string, float>> Word2Vec::nearestNeighbors(
    const std::vector<float>& vec,
    std::size_t top_k) const
{
    std::lock_guard<std::mutex> lock(inference_mutex_);
    std::vector<std::pair<std::string, float>> scores;
    if (!input_weights_ || vocab_.empty()) return scores;

    scores.reserve(vocab_.size());
    for (const auto& kv : vocab_) {
        auto v_cand = getVectorByIndex(kv.second.index);
        float sim = cosineSimilarity(vec, v_cand);
        scores.push_back({kv.first, sim});
    }

    std::partial_sort(scores.begin(),
                      scores.begin() + std::min(top_k, scores.size()),
                      scores.end(),
                      [](const auto& a, const auto& b) { return a.second > b.second; });

    if (scores.size() > top_k) {
        scores.resize(top_k);
    }
    return scores;
}

std::vector<std::vector<std::string>> Word2Vec::clusterWords(
    const std::vector<std::string>& words,
    std::size_t k) const
{
    std::lock_guard<std::mutex> lock(inference_mutex_);
    std::vector<std::vector<std::string>> clusters(k);
    if (words.empty() || k == 0) return clusters;

    std::vector<std::vector<float>> vecs;
    std::vector<std::string> valid_words;
    for (const auto& w : words) {
        if (hasWord(w)) {
            valid_words.push_back(w);
            vecs.push_back(getVector(w));
        }
    }

    if (valid_words.empty()) return clusters;
    if (k > valid_words.size()) k = valid_words.size();
    clusters.resize(k);

    std::vector<std::vector<float>> centroids(k, std::vector<float>(cfg_.embedding_dim, 0.0f));
    for (std::size_t i = 0; i < k; ++i) {
        centroids[i] = vecs[i];
    }

    std::vector<std::size_t> assignments(valid_words.size(), 0);

    for (std::size_t iter = 0; iter < 20; ++iter) {
        // Assign step
        for (std::size_t i = 0; i < valid_words.size(); ++i) {
            float max_sim = -1.0f;
            std::size_t best_c = 0;
            for (std::size_t c = 0; c < k; ++c) {
                float sim = cosineSimilarity(vecs[i], centroids[c]);
                if (sim > max_sim) {
                    max_sim = sim;
                    best_c = c;
                }
            }
            assignments[i] = best_c;
        }

        // Update step
        std::vector<std::vector<float>> new_centroids(k, std::vector<float>(cfg_.embedding_dim, 0.0f));
        std::vector<std::size_t> counts(k, 0);

        for (std::size_t i = 0; i < valid_words.size(); ++i) {
            std::size_t c = assignments[i];
            counts[c]++;
            for (std::size_t d = 0; d < cfg_.embedding_dim; ++d) {
                new_centroids[c][d] += vecs[i][d];
            }
        }

        for (std::size_t c = 0; c < k; ++c) {
            if (counts[c] > 0) {
                for (std::size_t d = 0; d < cfg_.embedding_dim; ++d) {
                    centroids[c][d] = new_centroids[c][d] / static_cast<float>(counts[c]);
                }
            }
        }
    }

    for (std::size_t i = 0; i < valid_words.size(); ++i) {
        clusters[assignments[i]].push_back(valid_words[i]);
    }

    return clusters;
}

std::vector<std::string> Word2Vec::vocabulary() const {
    std::lock_guard<std::mutex> lock(inference_mutex_);
    return index_to_word_;
}

bool Word2Vec::saveEmbeddings(const std::string& filepath) const {
    std::lock_guard<std::mutex> lock(inference_mutex_);
    std::ofstream ofs(filepath, std::ios::binary);
    if (!ofs.is_open()) return false;

    std::size_t vsize = vocab_.size();
    std::size_t dim = cfg_.embedding_dim;

    ofs.write(reinterpret_cast<const char*>(&vsize), sizeof(vsize));
    ofs.write(reinterpret_cast<const char*>(&dim), sizeof(dim));

    for (const auto& w : index_to_word_) {
        std::uint16_t len = static_cast<std::uint16_t>(w.size());
        ofs.write(reinterpret_cast<const char*>(&len), sizeof(len));
        ofs.write(w.data(), len);

        auto vec = getVectorByIndex(vocab_.at(w).index);
        ofs.write(reinterpret_cast<const char*>(vec.data()), dim * sizeof(float));
    }
    return true;
}

bool Word2Vec::loadEmbeddings(const std::string& filepath) {
    std::lock_guard<std::mutex> lock(inference_mutex_);
    std::ifstream ifs(filepath, std::ios::binary);
    if (!ifs.is_open()) return false;

    std::size_t vsize = 0;
    std::size_t dim = 0;

    ifs.read(reinterpret_cast<char*>(&vsize), sizeof(vsize));
    ifs.read(reinterpret_cast<char*>(&dim), sizeof(dim));

    if (vsize == 0 || dim == 0) return false;

    cfg_.embedding_dim = dim;
    vocab_.clear();
    index_to_word_.clear();
    index_to_word_.reserve(vsize);

    input_weights_ = std::make_unique<Matrix>(vsize, dim);
    output_weights_ = std::make_unique<Matrix>(vsize, dim, 0.0f);

    for (std::size_t i = 0; i < vsize; ++i) {
        std::uint16_t len = 0;
        ifs.read(reinterpret_cast<char*>(&len), sizeof(len));
        std::string w(len, '\0');
        ifs.read(&w[0], len);

        VocabWord vw;
        vw.word = w;
        vw.count = 100; // default count for loaded vocab
        vw.index = i;
        vw.sample_prob = 1.0f;
        vocab_[w] = vw;
        index_to_word_.push_back(w);

        std::vector<float> vec(dim, 0.0f);
        ifs.read(reinterpret_cast<char*>(vec.data()), dim * sizeof(float));
        for (std::size_t d = 0; d < dim; ++d) {
            (*input_weights_)(i, d) = vec[d];
        }
    }

    computeSubsampleProbs();
    buildNegativeSamplingTable();
    return true;
}

bool Word2Vec::saveVocabulary(const std::string& filepath) const {
    std::lock_guard<std::mutex> lock(inference_mutex_);
    std::ofstream ofs(filepath);
    if (!ofs.is_open()) return false;

    for (const auto& w : index_to_word_) {
        const auto& vw = vocab_.at(w);
        ofs << vw.word << " " << vw.count << "\n";
    }
    return true;
}

bool Word2Vec::loadVocabulary(const std::string& filepath) {
    std::ifstream ifs(filepath);
    if (!ifs.is_open()) return false;

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(ifs, line)) {
        if (!line.empty()) lines.push_back(line);
    }
    buildVocabulary(lines);
    return true;
}

} // namespace yuki::language
