#pragma once
// LearnedFactExtractor.h — Learned personal fact extractor
// Replaces hardcoded string patterns with learned embeddings + classifier

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <array>
#include <optional>
#include "input/encoding/TextEncoder.h"

namespace yuki { namespace memory {

struct ExtractedFact {
    std::string category;   // "identity", "relationship", "preference", "goal"
    std::string key;        // "name", "age", "location", "occupation", "like", "dislike"
    std::string value;      // extracted value
    float confidence = 0.0f;
    std::string rawSource;  // original utterance
    std::string span;       // the exact span in the utterance
};

struct TrainingExample {
    std::string utterance;
    std::vector<ExtractedFact> facts;
};

class LearnedFactExtractor {
public:
    // Default constructor - uses TextEncoder's Word2Vec for embeddings
    explicit LearnedFactExtractor(yuki::perception::TextEncoder* encoder = nullptr);
    
    // Extract facts from an utterance using learned patterns
    std::vector<ExtractedFact> extract(const std::string& utterance) const;
    
    // Add a training example (for online learning)
    void addTrainingExample(const TrainingExample& example);
    
    // Train the classifier on accumulated examples
    void train();
    
    // Save/load model
    bool save(const std::string& path) const;
    bool load(const std::string& path);
    
    // Get training stats
    size_t trainingExampleCount() const { return trainingExamples_.size(); }
    
    // Add seed patterns (high-confidence rules) for bootstrap
    void addSeedPattern(const std::string& category, const std::string& key, 
                       const std::string& pattern, const std::string& extractionTemplate);

private:
    struct Classifier {
        // Simple logistic regression per fact type
        std::vector<float> weights;  // size = embedding_dim + 1 (bias)
        std::string category;
        std::string key;
        
        float predict(const std::vector<float>& embedding) const;
        void update(const std::vector<float>& embedding, float target, float lr);
    };
    
    // Feature extraction: TextEncoder embedding + position features + pattern features
    std::vector<float> extractFeatures(const std::string& utterance, size_t startPos, size_t endPos) const;
    
    // Find candidate spans using seed patterns + learned patterns
    std::vector<std::pair<size_t, size_t>> findCandidateSpans(const std::string& utterance) const;
    
    // Classifiers for each fact type
    std::vector<std::unique_ptr<Classifier>> classifiers_;
    
    // Training data for online learning
    std::vector<TrainingExample> trainingExamples_;
    
    // TextEncoder for embeddings (non-owning)
    yuki::perception::TextEncoder* encoder_ = nullptr;
    
    // Seed patterns for bootstrap (high-confidence rules)
    struct SeedPattern {
        std::string category;
        std::string key;
        std::string pattern;           // regex-like pattern
        std::string extractionTemplate; // how to extract value from match
    };
    std::vector<SeedPattern> seedPatterns_;
    
    // Config
    static constexpr float CONFIDENCE_THRESHOLD = 0.5f;
    static constexpr size_t EMBEDDING_DIM = 64; // TextEncoder's 64-dim
    
    // Initialize default classifiers and seed patterns
    void initializeDefaults();
    
    // Helper: train a single classifier
    void trainClassifier(size_t idx, const std::vector<std::pair<std::vector<float>, float>>& data);
};

}} // namespace yuki::memory