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
#include <memory>

// Forward declarations
namespace yuki { namespace swarm { class ISemanticStore; } }
namespace yuki { namespace brain { namespace memory { class SemanticHeuristicAccess; struct LearnedKeyword; } } }

namespace yuki { namespace perception {

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
    ~TextEncoder(); // out-of-line: unique_ptr<SemanticHeuristicAccess> needs complete type
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

    /// Default constructor — identical to previous behaviour (hardcoded keyword lists).
    explicit TextEncoder(const Word2VecConfig& cfg = Word2VecConfig{});

    /// Memory-backed constructor: loads keyword lists from ISemanticStore.
    /// All callers MUST use this overload once the semantic store is available.
    TextEncoder(const Word2VecConfig& cfg, yuki::swarm::ISemanticStore& store);

    std::vector<float> encode(const std::string& text) const;
    std::vector<float> projectTo8(const std::vector<float>& vec64) const;
    void trainStep(const std::string& sentence, float lr);
    void buildVocabulary(const std::vector<std::string>& corpus);
    size_t vocabSize() const { return word_to_id_.size(); }
    void seedCurriculumVocabulary(const std::vector<std::string>& topic_names);

    HeuristicScores getLastScores() const;  // cache from last encode()

    /// Reinforce or penalize keyword lists after a turn.
    /// channel: 0=question, 1=command, 2=emotional, 3=technical, 4=urgency,
    ///          5=greeting, 6=action, 7=polarity
    void onTurnOutcome(int channel, bool wasCorrect, const std::string& topWord = "");

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

    // Heuristic keyword caches (populated from store or fallback to inline literals)
    std::unique_ptr<yuki::brain::memory::SemanticHeuristicAccess> heuristic_;
    // Each of the 8 channels has a vector of learned keywords
    // Indices: 0=question, 1=command, 2=emotional, 3=technical, 4=urgency, 5=greeting, 6=action, 7=polarity
    mutable std::vector<std::vector<std::string>> channelKeywords_;

    void loadHeuristicKeywords();
};

} } // namespace yuki::perception
