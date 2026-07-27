#include "brain/language/VaeResponseGenerator.h"
#include "brain/learning/generative/VariationalAutoencoder.h"
#include "brain/language/GrammarEngine.h"
#include "brain/language/Word2Vec.h"
#include "brain/core/ConfigManager.h"
#include <fstream>
#include <sstream>
#include <cmath>
#include <algorithm>

namespace yuki::language {

std::vector<float> SentenceFeatureVector::toVector() const {
    std::vector<float> vec;
    vec.reserve(totalDim());
    for (size_t i = 0; i < kW2vDim; ++i) vec.push_back(w2v_mean[i]);
    for (size_t i = 0; i < kStructDim; ++i) vec.push_back(structural[i]);
    return vec;
}

void SentenceFeatureVector::fromVector(const std::vector<float>& v) {
    if (v.size() < totalDim()) return;
    for (size_t i = 0; i < kW2vDim; ++i) w2v_mean[i] = v[i];
    for (size_t i = 0; i < kStructDim; ++i) structural[i] = v[kW2vDim + i];
}

VaeResponseGenerator::VaeResponseGenerator(yuki::learning::generative::VariationalAutoencoder* vae,
                                           GrammarEngine* grammar,
                                           Word2Vec* w2v)
    : vae_(vae), grammar_(grammar), w2v_(w2v) {}

SentenceFeatureVector VaeResponseGenerator::textToFeature(const std::string& sentence) {
    SentenceFeatureVector feat;
    if (sentence.empty()) return feat;

    std::istringstream iss(sentence);
    std::string token;
    std::vector<std::string> tokens;
    while (iss >> token) tokens.push_back(token);

    if (!tokens.empty()) {
        size_t found = 0;
        for (const auto& tok : tokens) {
            if (w2v_) {
                auto vec = w2v_->getVector(tok);
                if (!vec.empty()) {
                    found++;
                    for (size_t i = 0; i < SentenceFeatureVector::kW2vDim && i < vec.size(); ++i) {
                        feat.w2v_mean[i] += vec[i];
                    }
                }
            }
        }
        if (found > 0) {
            for (size_t i = 0; i < SentenceFeatureVector::kW2vDim; ++i) {
                feat.w2v_mean[i] /= static_cast<float>(found);
            }
        }
    }

    feat.structural[0] = static_cast<float>(sentence.size()) / 500.0f;
    feat.structural[1] = static_cast<float>(tokens.size()) / 50.0f;
    feat.structural[2] = 0.05f;
    feat.structural[3] = tokens.empty() ? 0.0f : static_cast<float>(sentence.size()) / tokens.size() / 20.0f;
    feat.structural[4] = 0.5f;
    feat.structural[5] = (!sentence.empty() && sentence.back() == '?') ? 1.0f : 0.0f;
    feat.structural[6] = 0.0f;
    feat.structural[7] = 0.2f;

    return feat;
}

void VaeResponseGenerator::trainOnCorpus(const std::string& corpus_path, size_t epochs) {
    std::ifstream in(corpus_path);
    if (!in.is_open()) return;

    std::string line;
    std::vector<std::vector<float>> dataset;

    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        SentenceFeatureVector sfv = textToFeature(line);
        dataset.push_back(sfv.toVector());
    }

    if (dataset.empty()) return;

    if (vae_) {
        for (size_t epoch = 0; epoch < epochs; ++epoch) {
            for (const auto& sample : dataset) {
                std::vector<double> sample_d(sample.begin(), sample.end());
                vae_->trainStep(sample_d);
            }
        }
        trained_ = true;
    }
}

SentenceFeatureVector VaeResponseGenerator::generate(const std::vector<float>& condition_vector) {
    SentenceFeatureVector feat;
    if (!vae_) return feat;

    std::vector<double> sample = vae_->samplePrior();
    if (!condition_vector.empty()) {
        for (size_t i = 0; i < sample.size() && i < condition_vector.size(); ++i) {
            sample[i] += static_cast<double>(condition_vector[i]) * 0.1;
        }
    }

    std::vector<double> decoded_d = vae_->decode(sample);
    std::vector<float> decoded(decoded_d.begin(), decoded_d.end());
    feat.fromVector(decoded);
    return feat;
}

std::vector<float> VaeResponseGenerator::encodeIntent(const std::string& intent_tag) const {
    std::vector<float> vec(300, 0.0f);
    for (size_t i = 0; i < intent_tag.size() && i < 300; ++i) {
        vec[i] = static_cast<float>(intent_tag[i]) / 255.0f;
    }
    return vec;
}

std::string VaeResponseGenerator::featureToText(const SentenceFeatureVector& feat,
                                                const std::string& intent_tag,
                                                const std::vector<std::pair<std::string, std::string>>& slots) {
    if (grammar_) {
        SemanticFrame frame = grammar_->buildDescriptiveFrame("Yuki", {"intelligent", "agentic"});
        std::string res = grammar_->generate(frame);
        if (!res.empty()) return res;
    }
    std::string result = ConfigManager::instance().getTemplate(intent_tag);
    if (result.empty()) {
        result = "Yuki: Processing response for " + intent_tag;
    }
    return result;
}

std::string VaeResponseGenerator::generateResponse(const std::string& intent_tag,
                                                   const std::vector<std::pair<std::string, std::string>>& slots) {
    std::vector<float> cond = encodeIntent(intent_tag);
    SentenceFeatureVector feat = generate(cond);
    return featureToText(feat, intent_tag, slots);
}

void VaeResponseGenerator::save(const std::string& path) const {
    std::ofstream out(path, std::ios::binary);
    if (!out.is_open()) return;

    uint32_t magic = 0x59564145; // "YVAE"
    out.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
    out.write(reinterpret_cast<const char*>(&trained_), sizeof(trained_));
}

bool VaeResponseGenerator::load(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) return false;

    uint32_t magic = 0;
    in.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    if (magic != 0x59564145) return false;

    in.read(reinterpret_cast<char*>(&trained_), sizeof(trained_));
    return true;
}

} // namespace yuki::language
