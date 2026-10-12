#include "FastText.h"
#include <sstream>
#include <algorithm>
#include <numeric>
#include <fstream>
#include <iostream>
#include <queue>
#include <random>
#include <cstdlib>

namespace yuki::learning {

FastText::FastText() {
    // Seed standard base vocabulary embeddings
    std::vector<std::string> base_words = {
        "the", "be", "to", "of", "and", "a", "in", "that", "have", "i",
        "it", "for", "not", "on", "with", "he", "as", "you", "do", "at",
        "this", "but", "his", "by", "from", "they", "we", "say", "her", "she",
        "or", "an", "will", "my", "one", "all", "would", "there", "their", "what",
        "so", "up", "out", "if", "about", "who", "get", "which", "go", "me",
        "think", "yuki", "mind", "learn", "know", "code", "system", "brain", "agent"
    };

    for (const auto& w : base_words) {
        std::vector<float> vec(kEmbeddingDim, 0.0f);
        uint64_t h = fnv1a(w);
        for (size_t d = 0; d < kEmbeddingDim; ++d) {
            h = h * 6364136223846793005ULL + 1442695040888963407ULL;
            vec[d] = static_cast<float>((h >> 32) & 0xFFFF) / 65535.0f - 0.5f;
        }
        normalize(vec);
        word_vectors_[w] = std::move(vec);
    }
}

uint64_t FastText::fnv1a(const std::string& str) {
    uint64_t hash = 14695981039346656037ULL;
    for (char c : str) {
        hash ^= static_cast<uint8_t>(c);
        hash *= 1099511628211ULL;
    }
    return hash;
}

void FastText::normalize(std::vector<float>& vec) {
    float sum_sq = 0.0f;
    for (float v : vec) sum_sq += v * v;
    if (sum_sq > 1e-12f) {
        float inv_norm = 1.0f / std::sqrt(sum_sq);
        for (float& v : vec) v *= inv_norm;
    }
}

std::vector<std::string> FastText::extractNgrams(const std::string& word) const {
    std::string wrapped = "<" + word + ">";
    std::vector<std::string> ngrams;
    size_t len = wrapped.size();

    for (size_t n = kMinNgram; n <= kMaxNgram; ++n) {
        if (len < n) continue;
        for (size_t i = 0; i <= len - n; ++i) {
            ngrams.push_back(wrapped.substr(i, n));
        }
    }
    return ngrams;
}

std::vector<float> FastText::embed(const std::string& word) const {
    std::string clean;
    for (char c : word) {
        if (std::isalnum(static_cast<unsigned char>(c))) {
            clean += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
    }

    if (clean.empty()) {
        return std::vector<float>(kEmbeddingDim, 0.0f);
    }

    // Direct vocabulary hit
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = word_vectors_.find(clean);
        if (it != word_vectors_.end()) {
            return it->second;
        }
    }

    // Subword n-gram aggregation for Out-Of-Vocabulary (OOV) handling
    std::vector<float> out(kEmbeddingDim, 0.0f);
    auto ngrams = extractNgrams(clean);

    if (ngrams.empty()) {
        uint64_t h = fnv1a(clean);
        for (size_t d = 0; d < kEmbeddingDim; ++d) {
            h = h * 6364136223846793005ULL + 1442695040888963407ULL;
            out[d] = static_cast<float>((h >> 32) & 0xFFFF) / 65535.0f - 0.5f;
        }
    } else {
        for (const auto& ng : ngrams) {
            uint64_t h = fnv1a(ng);
            for (size_t d = 0; d < kEmbeddingDim; ++d) {
                h = h * 6364136223846793005ULL + 1442695040888963407ULL;
                out[d] += static_cast<float>((h >> 32) & 0xFFFF) / 65535.0f - 0.5f;
            }
        }
    }

    normalize(out);
    return out;
}

std::vector<float> FastText::embedSentence(const std::string& text) const {
    std::vector<float> sentence_vec(kEmbeddingDim, 0.0f);
    std::stringstream ss(text);
    std::string word;
    size_t count = 0;

    while (ss >> word) {
        auto wvec = embed(word);
        for (size_t d = 0; d < kEmbeddingDim; ++d) {
            sentence_vec[d] += wvec[d];
        }
        ++count;
    }

    if (count > 0) {
        normalize(sentence_vec);
    }
    return sentence_vec;
}

void FastText::addWord(const std::string& word, const std::vector<float>& vec) {
    if (vec.size() == kEmbeddingDim) {
        std::vector<float> norm_vec = vec;
        normalize(norm_vec);
        std::lock_guard<std::mutex> lock(mutex_);
        word_vectors_[word] = std::move(norm_vec);
    }
}

bool FastText::loadPretrainedVectors(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "[FastText] Failed to open: " << filepath << "\n";
        return false;
    }

    std::string line;
    // Check if first line is a header like "111051 300"
    if (std::getline(file, line)) {
        if (!line.empty()) {
            std::stringstream ss(line);
            size_t count = 0, dim = 0;
            if ((ss >> count >> dim) && dim == kEmbeddingDim) {
                std::lock_guard<std::mutex> lock(mutex_);
                word_vectors_.reserve(word_vectors_.size() + count);
            } else {
                // Not a header line, rewind
                file.clear();
                file.seekg(0, std::ios::beg);
            }
        }
    }

    size_t loaded = 0;
    std::vector<float> vec(kEmbeddingDim);
    std::vector<std::pair<std::string, std::vector<float>>> batch;
    batch.reserve(4096);

    while (std::getline(file, line)) {
        if (line.empty()) continue;
        const char* p = line.c_str();
        while (*p && (*p == ' ' || *p == '\t')) ++p;
        if (!*p) continue;

        const char* word_start = p;
        while (*p && *p != ' ' && *p != '\t') ++p;
        std::string word(word_start, p - word_start);
        if (word.empty()) continue;

        size_t d = 0;
        char* endp = nullptr;
        while (*p && d < kEmbeddingDim) {
            float val = std::strtof(p, &endp);
            if (p == endp) break;
            vec[d++] = val;
            p = endp;
        }
        if (d != kEmbeddingDim) continue;

        normalize(vec);
        batch.emplace_back(std::move(word), vec);

        if (batch.size() >= 4096) {
            std::lock_guard<std::mutex> lock(mutex_);
            for (auto& item : batch) {
                word_vectors_[std::move(item.first)] = std::move(item.second);
            }
            batch.clear();
        }
        ++loaded;
    }

    if (!batch.empty()) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& item : batch) {
            word_vectors_[std::move(item.first)] = std::move(item.second);
        }
    }

    std::cout << "[FastText] Loaded " << loaded << " vectors from " << filepath << "\n";
    return true;
}

bool FastText::loadPretrainedBinary(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[FastText] Failed to open binary: " << filepath << "\n";
        return false;
    }

    // Read header: vocab_size dim
    int32_t vocab_size, dim;
    file.read(reinterpret_cast<char*>(&vocab_size), sizeof(int32_t));
    file.read(reinterpret_cast<char*>(&dim), sizeof(int32_t));

    if (dim != static_cast<int32_t>(kEmbeddingDim)) {
        std::cerr << "[FastText] Dimension mismatch: expected " << kEmbeddingDim << ", got " << dim << "\n";
        return false;
    }

    size_t loaded = 0;
    for (int32_t i = 0; i < vocab_size; ++i) {
        // Read word (null-terminated)
        std::string word;
        char c;
        while (file.get(c) && c != ' ' && c != '\n') {
            word += c;
        }

        std::vector<float> vec(kEmbeddingDim);
        file.read(reinterpret_cast<char*>(vec.data()), kEmbeddingDim * sizeof(float));

        if (!word.empty()) {
            normalize(vec);
            std::lock_guard<std::mutex> lock(mutex_);
            word_vectors_[word] = std::move(vec);
            ++loaded;
        }
    }

    std::cout << "[FastText] Loaded " << loaded << " vectors from binary " << filepath << "\n";
    return true;
}

bool FastText::saveEmbeddings(const std::string& filepath) const {
    std::ofstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "[FastText] Failed to open for writing: " << filepath << "\n";
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& [word, vec] : word_vectors_) {
        file << word;
        for (float v : vec) file << ' ' << v;
        file << '\n';
    }
    return true;
}

bool FastText::loadEmbeddings(const std::string& filepath) {
    return loadPretrainedVectors(filepath);
}

bool FastText::hasWord(const std::string& word) const {
    std::string clean;
    for (char c : word) {
        if (std::isalnum(static_cast<unsigned char>(c))) {
            clean += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
    }
    if (clean.empty()) return false;
    std::lock_guard<std::mutex> lock(mutex_);
    return word_vectors_.find(clean) != word_vectors_.end();
}

float FastText::cosineSimilarity(const std::string& word1, const std::string& word2) const {
    auto v1 = embed(word1);
    auto v2 = embed(word2);
    return cosineSimilarity(v1, v2);
}

float FastText::cosineSimilarity(const std::vector<float>& vec1, const std::vector<float>& vec2) const {
    if (vec1.size() != vec2.size() || vec1.empty()) return 0.0f;
    float dot = 0.0f, norm1 = 0.0f, norm2 = 0.0f;
    for (size_t i = 0; i < vec1.size(); ++i) {
        dot += vec1[i] * vec2[i];
        norm1 += vec1[i] * vec1[i];
        norm2 += vec2[i] * vec2[i];
    }
    if (norm1 < 1e-12f || norm2 < 1e-12f) return 0.0f;
    return dot / (std::sqrt(norm1) * std::sqrt(norm2));
}

std::vector<std::pair<std::string, float>> FastText::nearestNeighbors(
    const std::string& word, std::size_t top_k) const {
    auto target = embed(word);
    return nearestNeighbors(target, top_k);
}

std::vector<std::pair<std::string, float>> FastText::nearestNeighbors(
    const std::vector<float>& vec, std::size_t top_k) const {

    std::vector<std::pair<std::string, float>> results;
    results.reserve(word_vectors_.size());

    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& [word, wvec] : word_vectors_) {
        float sim = cosineSimilarity(vec, wvec);
        results.emplace_back(word, sim);
    }

    std::partial_sort(results.begin(),
                      results.begin() + std::min(top_k, results.size()),
                      results.end(),
                      [](const auto& a, const auto& b) { return a.second > b.second; });

    if (results.size() > top_k) results.resize(top_k);
    return results;
}

std::vector<std::string> FastText::vocabulary() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> vocab;
    vocab.reserve(word_vectors_.size());
    for (const auto& [word, _] : word_vectors_) vocab.push_back(word);
    return vocab;
}

std::vector<std::pair<std::string, float>> FastText::analogy(
    const std::string& word_a, const std::string& word_b,
    const std::string& word_c, std::size_t top_k) const {

    auto va = embed(word_a);
    auto vb = embed(word_b);
    auto vc = embed(word_c);

    // target = vb - va + vc
    std::vector<float> target(kEmbeddingDim);
    for (size_t i = 0; i < kEmbeddingDim; ++i) {
        target[i] = vb[i] - va[i] + vc[i];
    }
    normalize(target);

    std::unordered_set<std::string> exclude = {word_a, word_b, word_c};
    auto neighbors = nearestNeighbors(target, top_k + exclude.size());

    std::vector<std::pair<std::string, float>> filtered;
    for (const auto& [word, score] : neighbors) {
        if (exclude.find(word) == exclude.end()) {
            filtered.emplace_back(word, score);
            if (filtered.size() >= top_k) break;
        }
    }
    return filtered;
}

std::vector<float> FastText::encodeInContext(const std::vector<std::string>& tokens, std::size_t targetIndex) const {
    if (tokens.empty() || targetIndex >= tokens.size()) {
        return std::vector<float>(kEmbeddingDim, 0.0f);
    }

    // Context window: ±2 tokens
    const size_t window = 2;
    size_t start = (targetIndex > window) ? targetIndex - window : 0;
    size_t end = std::min(targetIndex + window + 1, tokens.size());

    std::vector<float> context_vec(kEmbeddingDim, 0.0f);
    size_t count = 0;

    for (size_t i = start; i < end; ++i) {
        if (i == targetIndex) continue;
        auto wvec = embed(tokens[i]);
        for (size_t d = 0; d < kEmbeddingDim; ++d) {
            context_vec[d] += wvec[d];
        }
        ++count;
    }

    // Combine target word vector with context
    auto target_vec = embed(tokens[targetIndex]);
    float alpha = 0.7f;  // weight for target word
    float beta = (count > 0) ? 0.3f / count : 0.0f;

    std::vector<float> result(kEmbeddingDim);
    for (size_t d = 0; d < kEmbeddingDim; ++d) {
        result[d] = alpha * target_vec[d] + beta * context_vec[d];
    }
    normalize(result);
    return result;
}

std::vector<float> FastText::composePhrase(const std::vector<std::string>& phraseTokens) const {
    if (phraseTokens.empty()) return std::vector<float>(kEmbeddingDim, 0.0f);

    std::vector<float> result(kEmbeddingDim, 0.0f);
    for (const auto& token : phraseTokens) {
        auto wvec = embed(token);
        for (size_t d = 0; d < kEmbeddingDim; ++d) {
            result[d] += wvec[d];
        }
    }
    normalize(result);
    return result;
}

float FastText::ambiguityScore(const std::string& token, const std::vector<float>& ctxVec) const {
    auto token_vec = embed(token);
    float sim = cosineSimilarity(token_vec, ctxVec);
    // High similarity = low ambiguity (well-disambiguated in context)
    // Low similarity = high ambiguity (word meaning shifts in context)
    return 1.0f - sim;
}

std::vector<std::vector<std::string>> FastText::clusterWords(
    const std::vector<std::string>& words, std::size_t k) const {

    if (words.empty() || k == 0) return {};

    std::vector<std::vector<float>> vecs;
    std::vector<std::string> valid_words;
    for (const auto& w : words) {
        auto v = embed(w);
        bool is_zero = true;
        for (float x : v) if (std::abs(x) > 1e-6f) { is_zero = false; break; }
        if (!is_zero) {
            vecs.push_back(v);
            valid_words.push_back(w);
        }
    }

    if (vecs.empty() || k >= vecs.size()) {
        return {valid_words};
    }

    // Simple k-means++ initialization
    std::vector<std::vector<float>> centroids(k, std::vector<float>(kEmbeddingDim));
    std::mt19937 rng(42);
    std::uniform_int_distribution<size_t> dist(0, vecs.size() - 1);
    centroids[0] = vecs[dist(rng)];

    for (size_t c = 1; c < k; ++c) {
        std::vector<float> probs(vecs.size());
        for (size_t i = 0; i < vecs.size(); ++i) {
            float min_dist = 1e9f;
            for (size_t j = 0; j < c; ++j) {
                float d = 1.0f - cosineSimilarity(vecs[i], centroids[j]);
                min_dist = std::min(min_dist, d);
            }
            probs[i] = min_dist * min_dist;
        }
        float sum = std::accumulate(probs.begin(), probs.end(), 0.0f);
        if (sum > 0) {
            for (auto& p : probs) p /= sum;
        } else {
            probs[dist(rng)] = 1.0f;
        }

        std::discrete_distribution<size_t> ddist(probs.begin(), probs.end());
        centroids[c] = vecs[ddist(rng)];
    }

    // K-means iterations
    std::vector<std::vector<std::size_t>> clusters(k);
    for (int iter = 0; iter < 10; ++iter) {
        // Assign
        clusters.assign(k, {});
        for (size_t i = 0; i < vecs.size(); ++i) {
            float best_sim = -1.0f;
            size_t best_c = 0;
            for (size_t c = 0; c < k; ++c) {
                float sim = cosineSimilarity(vecs[i], centroids[c]);
                if (sim > best_sim) {
                    best_sim = sim;
                    best_c = c;
                }
            }
            clusters[best_c].push_back(i);
        }

        // Update centroids
        for (size_t c = 0; c < k; ++c) {
            if (clusters[c].empty()) continue;
            std::fill(centroids[c].begin(), centroids[c].end(), 0.0f);
            for (size_t idx : clusters[c]) {
                for (size_t d = 0; d < kEmbeddingDim; ++d) {
                    centroids[c][d] += vecs[idx][d];
                }
            }
            for (size_t d = 0; d < kEmbeddingDim; ++d) {
                centroids[c][d] /= clusters[c].size();
            }
            normalize(centroids[c]);
        }
    }

    // Build result
    std::vector<std::vector<std::string>> result(k);
    for (size_t c = 0; c < k; ++c) {
        for (size_t idx : clusters[c]) {
            result[c].push_back(valid_words[idx]);
        }
    }
    return result;
}

std::vector<std::string> FastText::getNgrams(const std::string& word) const {
    return extractNgrams(word);
}

void FastText::setNgramRange(size_t min_n, size_t max_n) {
    // Note: This requires rebuilding ngrams for existing words
    // For now, just update constants (would need const_cast or mutable members for runtime change)
    // In practice, ngram range is set at compile-time via static constexpr
}

std::string FastText::findClosest(const std::vector<float>& target,
                                   const std::unordered_set<std::string>& exclude) const {
    float best_sim = -1.0f;
    std::string best_word;

    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& [word, vec] : word_vectors_) {
        if (exclude.find(word) != exclude.end()) continue;
        float sim = cosineSimilarity(target, vec);
        if (sim > best_sim) {
            best_sim = sim;
            best_word = word;
        }
    }
    return best_word;
}

} // namespace yuki::learning