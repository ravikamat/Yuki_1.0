#pragma once
// FastText — compact word/subword embedding model (300-d, hashed n-gram
// buckets) used for cheap semantic similarity and vocabulary growth.
// Supports loading pre-trained Facebook .bin / .vec vectors, incremental
// addWord(), cosine similarity and persistence of the trained embedding table.
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <cmath>
#include <cstdint>
#include <memory>
#include <mutex>

namespace yuki::learning {

class FastText {
public:
    static constexpr size_t kEmbeddingDim = 300;
    static constexpr size_t kBucketCount = 2000000;
    static constexpr size_t kMinNgram = 3;
    static constexpr size_t kMaxNgram = 6;

    FastText();
    ~FastText() = default;

    // --- Core Inference API ---
    std::vector<float> embed(const std::string& word) const;
    std::vector<float> embedSentence(const std::string& text) const;
    void addWord(const std::string& word, const std::vector<float>& vec);

    // --- Pre-trained Vector Loading ---
    bool loadPretrainedVectors(const std::string& filepath);  // .vec text format (wiki/fasttext)
    bool loadPretrainedBinary(const std::string& filepath);   // Facebook .bin format
    bool saveEmbeddings(const std::string& filepath) const;
    bool loadEmbeddings(const std::string& filepath);

    // --- Word2Vec-compatible API (replacement) ---
    std::vector<float> getVector(const std::string& word) const { return embed(word); }
    bool hasWord(const std::string& word) const;

    float cosineSimilarity(const std::string& word1, const std::string& word2) const;
    float cosineSimilarity(const std::vector<float>& vec1, const std::vector<float>& vec2) const;

    // Analogy: word_a is to word_b as word_c is to ?
    std::vector<std::pair<std::string, float>> analogy(
        const std::string& word_a,
        const std::string& word_b,
        const std::string& word_c,
        std::size_t top_k = 5) const;

    // Nearest neighbors
    std::vector<std::pair<std::string, float>> nearestNeighbors(
        const std::string& word,
        std::size_t top_k = 10) const;
    std::vector<std::pair<std::string, float>> nearestNeighbors(
        const std::vector<float>& vec,
        std::size_t top_k = 10) const;

    // Vocabulary access
    std::size_t vocabSize() const { return word_vectors_.size(); }
    std::size_t embeddingDim() const { return kEmbeddingDim; }
    std::vector<std::string> vocabulary() const;

    // --- Phase 2: Contextual Semantic Stack Additions ---
    bool hasVector(const std::string& word) const { return hasWord(word); }
    std::vector<float> encodeInContext(const std::vector<std::string>& tokens, std::size_t targetIndex) const;
    std::vector<float> composePhrase(const std::vector<std::string>& phraseTokens) const;
    float ambiguityScore(const std::string& token, const std::vector<float>& ctxVec) const;

    // Semantic clustering: k-means on word vectors
    std::vector<std::vector<std::string>> clusterWords(
        const std::vector<std::string>& words,
        std::size_t k) const;

    // --- FastText-specific: Subword model ---
    std::vector<std::string> getNgrams(const std::string& word) const;
    void setNgramRange(size_t min_n, size_t max_n);

private:
    std::unordered_map<std::string, std::vector<float>> word_vectors_;
    mutable std::mutex mutex_;

    std::vector<std::string> extractNgrams(const std::string& word) const;
    static uint64_t fnv1a(const std::string& str);
    static void normalize(std::vector<float>& vec);

    // Helper for analogy: find word closest to vec, excluding exclude_set
    std::string findClosest(const std::vector<float>& target, 
                            const std::unordered_set<std::string>& exclude) const;
};

} // namespace yuki::learning
