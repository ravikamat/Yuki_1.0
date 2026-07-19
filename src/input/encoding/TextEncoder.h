#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <random>
#include <cmath>
#include <sstream>
#include <cctype>
#include <algorithm>
#include <numeric>
#include <stdexcept>

namespace yuki::perception {

struct Word2VecConfig {
    size_t embed_dim = 64;
    size_t project_dim = 8;
    size_t max_vocab = 50000;
    size_t window_size = 5;
    int negative_samples = 5;
    float initial_lr = 0.025f;
    float min_lr = 0.0001f;
};

class TextEncoder {
public:
    enum class Heuristic {
        Question = 0,
        Command,
        Emotional,
        Technical,
        Urgency,
        Greeting,
        Action,
        Polarity,
        Phatic,        // ADD: social lubricants (yes, no, ok, thanks, bye, I am...)
        COUNT
    };

    struct HeuristicScores {
        float question = 0.0f;
        float command = 0.0f;
        float emotional = 0.0f;
        float technical = 0.0f;
        float urgency = 0.0f;
        float greeting = 0.0f;
        float action = 0.0f;
        float polarity = 0.0f;
        float phatic = 0.0f;        // ADD
    };

    explicit TextEncoder(const Word2VecConfig& cfg = Word2VecConfig{});
    std::vector<float> encode(const std::string& text) const;
    std::vector<float> projectTo8(const std::vector<float>& vec64) const;
    void trainStep(const std::string& sentence, float lr);
    void buildVocabulary(const std::vector<std::string>& corpus);
    size_t vocabSize() const { return word_to_id_.size(); }
    void seedCurriculumVocabulary(const std::vector<std::string>& topic_names);

    HeuristicScores getLastScores() const;  // ADD

private:
    Word2VecConfig cfg_;
    std::unordered_map<std::string, uint32_t> word_to_id_;
    std::vector<float> embeddings_;
    std::vector<float> projection_;
    std::vector<uint64_t> word_counts_;
    mutable std::mt19937 rng_;
    mutable HeuristicScores last_scores_;  // ADD: cache from last encode() call (mutable because encode is const)

    std::vector<std::string> tokenize(const std::string& text) const;
    uint32_t getIdOrOOV(const std::string& word) const;
    void initProjectionMatrix();
    std::vector<float> noiseDistribution() const;
    float sigmoid(float x) const;
    void l2Normalize(std::vector<float>& vec) const;

    float scoreQuestion(const std::string& lower) const;
    float scoreCommand(const std::string& lower) const;
    float scoreEmotional(const std::string& lower) const;
    float scoreTechnical(const std::string& lower) const;
    float scoreUrgency(const std::string& lower) const;
    float scoreGreeting(const std::string& lower) const;
    float scoreAction(const std::string& lower) const;
    float scorePolarity(const std::string& lower) const;
    float scorePhatic(const std::string& lower) const;
};

} // namespace yuki::perception
