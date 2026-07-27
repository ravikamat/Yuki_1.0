#pragma once
#include <string>
#include <vector>
#include <memory>
#include <cstdint>
#include <utility>

namespace yuki::learning::generative { class VariationalAutoencoder; }
namespace yuki::language { class GrammarEngine; class Word2Vec; }

namespace yuki::language {

struct SentenceFeatureVector {
    static constexpr size_t kW2vDim = 300;
    static constexpr size_t kStructDim = 8;
    float w2v_mean[kW2vDim] = {0};
    float structural[kStructDim] = {0}; // len, words, punct_density, avg_word_len, entropy, question_flag, imperative_flag, complexity_score
    uint32_t grammar_frame_id = 0;

    static size_t totalDim() { return kW2vDim + kStructDim; }
    std::vector<float> toVector() const;
    void fromVector(const std::vector<float>& v);
};

class VaeResponseGenerator {
public:
    VaeResponseGenerator(yuki::learning::generative::VariationalAutoencoder* vae,
                         GrammarEngine* grammar,
                         Word2Vec* w2v);

    // Train VAE on response corpus. Each line in corpus_path is one sentence.
    void trainOnCorpus(const std::string& corpus_path, size_t epochs);

    // Generate a SentenceFeatureVector from a latent sample conditioned on intent.
    SentenceFeatureVector generate(const std::vector<float>& condition_vector);

    // Full pipeline: condition → VAE sample → GrammarEngine frame → slots → string
    std::string generateResponse(const std::string& intent_tag,
                                 const std::vector<std::pair<std::string, std::string>>& slots);

    bool isTrained() const { return trained_; }
    void save(const std::string& path) const;
    bool load(const std::string& path);

private:
    yuki::learning::generative::VariationalAutoencoder* vae_;
    GrammarEngine* grammar_;
    Word2Vec* w2v_;
    bool trained_ = false;
    uint64_t training_steps_ = 0;

    SentenceFeatureVector textToFeature(const std::string& sentence);
    std::string featureToText(const SentenceFeatureVector& feat,
                              const std::string& intent_tag,
                              const std::vector<std::pair<std::string, std::string>>& slots);
    std::vector<float> encodeIntent(const std::string& intent_tag) const;
};

} // namespace yuki::language
