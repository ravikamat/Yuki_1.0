#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <random>
#include <mutex>
#include "brain/learning/neural/Matrix.h"

namespace yuki::language {

struct VocabWord {
    std::string word;
    std::size_t count = 0;
    std::size_t index = 0;      // row index in embedding matrix
    float sample_prob = 1.0f;   // subsampling probability
};

class Word2Vec {
public:
    struct Config {
        std::size_t embedding_dim = 300;
        std::size_t window_size = 5;
        std::size_t negative_samples = 5;
        float subsample_threshold = 1e-3f;
        float learning_rate = 0.025f;
        std::size_t min_count = 5;
        std::size_t epochs = 5;
        bool use_hierarchical_softmax = false;
        std::size_t seed = 42;
    };

    explicit Word2Vec(const Config& cfg = Config{});
    ~Word2Vec();  // Rule #26: out-of-line destructor

    // --- Vocabulary & Training ---
    void buildVocabulary(const std::vector<std::string>& sentences);
    void train(const std::vector<std::string>& sentences);

    // --- Persistence ---
    bool saveEmbeddings(const std::string& filepath) const;
    bool loadEmbeddings(const std::string& filepath);
    bool saveVocabulary(const std::string& filepath) const;
    bool loadVocabulary(const std::string& filepath);

    // --- Inference API ---
    std::vector<float> getVector(const std::string& word) const;
    bool hasWord(const std::string& word) const;

    float cosineSimilarity(const std::string& word1, const std::string& word2) const;
    float cosineSimilarity(const std::vector<float>& vec1, const std::vector<float>& vec2) const;

    // Analogy: word_a is to word_b as word_c is to ?
    std::vector<std::pair<std::string, float>> analogy(
        const std::string& word_a,
        const std::string& word_b,
        const std::string& word_c,
        std::size_t top_k = 5) const;

    // --- Phase 2: Contextual Semantic Stack Additions ---
    bool hasVector(const std::string& word) const { return hasWord(word); }
    std::vector<float> encodeInContext(const std::vector<std::string>& tokens, std::size_t targetIndex) const;
    std::vector<float> composePhrase(const std::vector<std::string>& phraseTokens) const;
    float ambiguityScore(const std::string& token, const std::vector<float>& ctxVec) const;

    // Find nearest neighbors in vector space
    std::vector<std::pair<std::string, float>> nearestNeighbors(
        const std::string& word,
        std::size_t top_k = 10) const;
    std::vector<std::pair<std::string, float>> nearestNeighbors(
        const std::vector<float>& vec,
        std::size_t top_k = 10) const;

    // Semantic clustering: k-means on word vectors
    std::vector<std::vector<std::string>> clusterWords(
        const std::vector<std::string>& words,
        std::size_t k) const;

    // Vocabulary accessors
    std::size_t vocabSize() const { return vocab_.size(); }
    std::size_t embeddingDim() const { return cfg_.embedding_dim; }
    std::vector<std::string> vocabulary() const;

private:
    Config cfg_;
    std::unordered_map<std::string, VocabWord> vocab_;
    std::vector<std::string> index_to_word_;

    // Embedding matrices: input (target) and output (context) weights
    // Shape: [vocab_size x embedding_dim]
    std::unique_ptr<yuki::learning::neural::Matrix> input_weights_;   // W_in
    std::unique_ptr<yuki::learning::neural::Matrix> output_weights_;  // W_out

    // Hierarchical softmax structures (optional)
    std::vector<std::vector<int>> huffman_codes_;
    std::vector<std::vector<int>> huffman_points_;

    // Negative sampling table
    std::vector<std::size_t> neg_sampling_table_;

    // RNG
    mutable std::mt19937 rng_;
    mutable std::mutex inference_mutex_;

    // Internal methods
    void computeSubsampleProbs();
    void buildHuffmanTree();
    void buildNegativeSamplingTable();

    std::vector<std::string> tokenize(const std::string& sentence) const;
    void trainSentence(const std::vector<std::string>& tokens, float alpha);
    void trainPair(const std::string& target, const std::string& context, float alpha);
    void trainPairNegativeSampling(
        std::size_t target_idx,
        std::size_t context_idx,
        float alpha);
    void trainPairHierarchicalSoftmax(
        std::size_t target_idx,
        std::size_t context_idx,
        float alpha);

    std::size_t sampleNegative() const;
    std::vector<float> getVectorByIndex(std::size_t idx) const;
};

} // namespace yuki::language
