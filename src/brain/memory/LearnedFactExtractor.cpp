#include "LearnedFactExtractor.h"
#include "input/encoding/TextEncoder.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <random>
#include <regex>
#include <iostream>

namespace yuki { namespace memory {

// ─── Classifier Implementation ───

float LearnedFactExtractor::Classifier::predict(const std::vector<float>& embedding) const {
    if (embedding.size() + 1 != weights.size()) return 0.0f;
    
    float sum = weights.back(); // bias
    for (size_t i = 0; i < embedding.size(); ++i) {
        sum += embedding[i] * weights[i];
    }
    // Sigmoid
    return 1.0f / (1.0f + std::exp(-sum));
}

void LearnedFactExtractor::Classifier::update(const std::vector<float>& embedding, float target, float lr) {
    if (embedding.size() + 1 != weights.size()) {
        weights.resize(embedding.size() + 1, 0.01f); // small random init
    }
    
    float pred = predict(embedding);
    float error = target - pred;
    
    // Gradient descent on logistic loss
    float grad = error * pred * (1.0f - pred); // derivative of sigmoid * error
    
    for (size_t i = 0; i < embedding.size(); ++i) {
        weights[i] += lr * grad * embedding[i];
    }
    weights.back() += lr * grad; // bias
}

// ─── LearnedFactExtractor Implementation ───

LearnedFactExtractor::LearnedFactExtractor(yuki::perception::TextEncoder* encoder) : encoder_(encoder) {
    initializeDefaults();
}

void LearnedFactExtractor::initializeDefaults() {
    // Create classifiers for each fact type
    const std::vector<std::pair<std::string, std::string>> factTypes = {
        {"identity", "name"},
        {"identity", "age"},
        {"identity", "location"},
        {"identity", "occupation"},
        {"identity", "employer"},
        {"relationship", "wife"},
        {"relationship", "husband"},
        {"relationship", "mother"},
        {"relationship", "father"},
        {"relationship", "sister"},
        {"relationship", "brother"},
        {"relationship", "son"},
        {"relationship", "daughter"},
        {"relationship", "friend"},
        {"relationship", "girlfriend"},
        {"relationship", "boyfriend"},
        {"relationship", "boss"},
        {"relationship", "dog"},
        {"relationship", "cat"},
        {"preference", "like"},
        {"preference", "dislike"},
        {"goal", "goal"}
    };
    
    for (const auto& [cat, key] : factTypes) {
        auto clf = std::make_unique<Classifier>();
        clf->category = cat;
        clf->key = key;
        clf->weights.resize(EMBEDDING_DIM + 1, 0.01f); // small init
        classifiers_.push_back(std::move(clf));
    }
    
    // Add seed patterns (high-confidence bootstrap rules)
    addSeedPattern("identity", "name", R"(my name is\s+(\w+))", "$1");
    addSeedPattern("identity", "name", R"(i am called\s+(\w+))", "$1");
    addSeedPattern("identity", "name", R"(call me\s+(\w+))", "$1");
    addSeedPattern("identity", "name", R"(people call me\s+(\w+))", "$1");
    addSeedPattern("identity", "name", R"(everyone calls me\s+(\w+))", "$1");
    addSeedPattern("identity", "name", R"(my full name is\s+(\w+))", "$1");
    addSeedPattern("identity", "name", R"(i am\s+([A-Z][a-z]+))", "$1"); // capitalized
    addSeedPattern("identity", "age", R"(i am\s+(\d+)\s*years?\s*old)", "$1");
    addSeedPattern("identity", "occupation", R"(i work as\s+([^.!?]+))", "$1");
    addSeedPattern("identity", "occupation", R"(i am a\s+([^.!?]+))", "$1");
    addSeedPattern("identity", "employer", R"(i work at\s+([^.!?]+))", "$1");
    addSeedPattern("identity", "employer", R"(i work for\s+([^.!?]+))", "$1");
    addSeedPattern("identity", "location", R"(i live in\s+([^.!?]+))", "$1");
    addSeedPattern("identity", "location", R"(i am from\s+([^.!?]+))", "$1");
    addSeedPattern("relationship", "wife", R"(my wife(?:'s name)?\s+is\s+(\w+))", "$1");
    addSeedPattern("relationship", "husband", R"(my husband(?:'s name)?\s+is\s+(\w+))", "$1");
    addSeedPattern("relationship", "mother", R"(my mother(?:'s name)?\s+is\s+(\w+))", "$1");
    addSeedPattern("relationship", "father", R"(my father(?:'s name)?\s+is\s+(\w+))", "$1");
    addSeedPattern("relationship", "dog", R"(my dog(?:'s name)?\s+is\s+(\w+))", "$1");
    addSeedPattern("relationship", "dog", R"(i have a dog named\s+(\w+))", "$1");
    addSeedPattern("relationship", "cat", R"(my cat(?:'s name)?\s+is\s+(\w+))", "$1");
    addSeedPattern("relationship", "cat", R"(i have a cat named\s+(\w+))", "$1");
    addSeedPattern("preference", "like", R"(i like\s+([^.!?]+))", "$1");
    addSeedPattern("preference", "like", R"(i love\s+([^.!?]+))", "$1");
    addSeedPattern("preference", "like", R"(i enjoy\s+([^.!?]+))", "$1");
    addSeedPattern("preference", "dislike", R"(i hate\s+([^.!?]+))", "$1");
    addSeedPattern("preference", "dislike", R"(i don't like\s+([^.!?]+))", "$1");
    addSeedPattern("preference", "dislike", R"(i dislike\s+([^.!?]+))", "$1");
}

void LearnedFactExtractor::addSeedPattern(const std::string& category, const std::string& key,
                                         const std::string& pattern, const std::string& extractionTemplate) {
    SeedPattern sp;
    sp.category = category;
    sp.key = key;
    sp.pattern = pattern;
    sp.extractionTemplate = extractionTemplate;
    seedPatterns_.push_back(std::move(sp));
}

std::vector<float> LearnedFactExtractor::extractFeatures(const std::string& utterance, 
                                                          size_t startPos, size_t endPos) const {
    std::vector<float> features;
    features.reserve(EMBEDDING_DIM + 10);
    
    // 1. TextEncoder embedding of the whole utterance (if available)
    if (encoder_) {
        auto emb = encoder_->encode(utterance);
        if (emb.size() >= EMBEDDING_DIM) {
            emb.resize(EMBEDDING_DIM);
            features.insert(features.end(), emb.begin(), emb.end());
        } else if (!emb.empty()) {
            features.insert(features.end(), emb.begin(), emb.end());
            features.resize(EMBEDDING_DIM, 0.0f);
        } else {
            features.resize(EMBEDDING_DIM, 0.0f);
        }
    } else {
        features.resize(EMBEDDING_DIM, 0.0f);
    }
    
    // 2. Position features (normalized)
    float totalLen = static_cast<float>(utterance.size());
    features.push_back(totalLen > 0 ? static_cast<float>(startPos) / totalLen : 0.0f);
    features.push_back(totalLen > 0 ? static_cast<float>(endPos) / totalLen : 0.0f);
    features.push_back(totalLen > 0 ? static_cast<float>(endPos - startPos) / totalLen : 0.0f);
    
    // 3. Context window features (words before/after)
    size_t ctxStart = (startPos > 20) ? startPos - 20 : 0;
    size_t ctxEnd = std::min(utterance.size(), endPos + 20);
    std::string context = utterance.substr(ctxStart, ctxEnd - ctxStart);
    // Could add more sophisticated context features here
    
    return features;
}

std::vector<std::pair<size_t, size_t>> LearnedFactExtractor::findCandidateSpans(const std::string& utterance) const {
    std::vector<std::pair<size_t, size_t>> candidates;
    std::string lower = utterance;
    std::transform(lower.begin(), lower.end(), lower.begin(), 
                   [](unsigned char c){ return std::tolower(c); });
    
    // 1. Seed pattern matches (high confidence)
    for (const auto& sp : seedPatterns_) {
        std::regex re(sp.pattern, std::regex_constants::icase);
        std::smatch match;
        std::string::const_iterator searchStart(lower.cbegin());
        while (std::regex_search(searchStart, lower.cend(), match, re)) {
            size_t start = match.position(0) + (searchStart - lower.cbegin());
            size_t end = start + match.length(0);
            candidates.emplace_back(start, end);
            searchStart = match.suffix().first;
        }
    }
    
    // 2. If no seed matches, add full utterance as candidate (for learned classifier)
    if (candidates.empty() && !lower.empty()) {
        candidates.emplace_back(0, lower.size());
    }
    
    return candidates;
}

std::vector<yuki::memory::ExtractedFact> LearnedFactExtractor::extract(const std::string& utterance) const {
    std::vector<yuki::memory::ExtractedFact> facts;
    if (utterance.empty()) return facts;
    
    // Find candidate spans
    auto candidates = findCandidateSpans(utterance);
    
    for (const auto& [start, end] : candidates) {
        // Extract features for this span
        auto features = extractFeatures(utterance, start, end);
        
        // Run each classifier
        for (const auto& clf : classifiers_) {
            float confidence = clf->predict(features);
            
            if (confidence > CONFIDENCE_THRESHOLD) {
                // Extract the value using seed patterns or heuristic
                std::string spanText = utterance.substr(start, end - start);
                std::string value = spanText;
                
                // Try to extract more precise value using seed patterns
                for (const auto& sp : seedPatterns_) {
                    if (sp.category == clf->category && sp.key == clf->key) {
                        std::regex re(sp.pattern, std::regex_constants::icase);
                        std::smatch match;
                        if (std::regex_search(utterance, match, re) && match.size() > 1) {
                            value = match[1].str();
                            break;
                        }
                    }
                }
                
                // Trim
                while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) value.pop_back();
                while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) value.erase(0, 1);
                
                if (value.size() > 1 && value.size() < 100) {
                    yuki::memory::ExtractedFact fact;
                    fact.category = clf->category;
                    fact.key = clf->key;
                    fact.value = value;
                    fact.confidence = confidence;
                    fact.rawSource = utterance;
                    fact.span = spanText;
                    facts.push_back(std::move(fact));
                }
            }
        }
    }
    
    // Deduplicate: keep highest confidence per (category, key)
    std::sort(facts.begin(), facts.end(), [](const auto& a, const auto& b) {
        if (a.category != b.category) return a.category < b.category;
        if (a.key != b.key) return a.key < b.key;
        return a.confidence > b.confidence;
    });
    
    std::vector<yuki::memory::ExtractedFact> unique;
    for (const auto& f : facts) {
        if (unique.empty() || unique.back().category != f.category || unique.back().key != f.key) {
            unique.push_back(f);
        }
    }
    
    return unique;
}

void LearnedFactExtractor::addTrainingExample(const TrainingExample& example) {
    trainingExamples_.push_back(example);
}

void LearnedFactExtractor::train() {
    if (trainingExamples_.empty()) {
        std::cout << "[LearnedFactExtractor] No training examples, skipping train\n";
        return;
    }
    
    std::cout << "[LearnedFactExtractor] Training on " << trainingExamples_.size() << " examples\n";
    
    // Collect training data for each classifier
    for (size_t clfIdx = 0; clfIdx < classifiers_.size(); ++clfIdx) {
        const auto& clf = classifiers_[clfIdx];
        std::vector<std::pair<std::vector<float>, float>> clfData;
        
        for (const auto& example : trainingExamples_) {
            auto candidates = findCandidateSpans(example.utterance);
            
            for (const auto& [start, end] : candidates) {
                auto features = extractFeatures(example.utterance, start, end);
                
                // Check if this example has a fact matching this classifier
                float target = 0.0f;
                for (const auto& fact : example.facts) {
                    if (fact.category == clf->category && fact.key == clf->key) {
                        // Check if the span overlaps
                        if (example.utterance.find(fact.span) != std::string::npos) {
                            target = 1.0f;
                            break;
                        }
                    }
                }
                
                clfData.emplace_back(features, target);
            }
        }
        
        if (!clfData.empty()) {
            trainClassifier(clfIdx, clfData);
        }
    }
    
    std::cout << "[LearnedFactExtractor] Training complete\n";
}

void LearnedFactExtractor::trainClassifier(size_t idx, 
    const std::vector<std::pair<std::vector<float>, float>>& data) {
    
    const float lr = 0.01f;
    const size_t epochs = 10;
    
    for (size_t epoch = 0; epoch < epochs; ++epoch) {
        // Shuffle
        std::vector<size_t> indices(data.size());
        std::iota(indices.begin(), indices.end(), 0);
        std::shuffle(indices.begin(), indices.end(), std::mt19937{42});
        
        for (size_t idx : indices) {
            const auto& [features, target] = data[idx];
            classifiers_[idx]->update(features, target, lr);
        }
    }
}

bool LearnedFactExtractor::save(const std::string& path) const {
    std::ofstream f(path);
    if (!f.is_open()) return false;
    
    // Save classifiers
    f << classifiers_.size() << "\n";
    for (const auto& clf : classifiers_) {
        f << clf->category << "\n" << clf->key << "\n";
        f << clf->weights.size() << "\n";
        for (float w : clf->weights) f << w << " ";
        f << "\n";
    }
    
    // Save seed patterns
    f << seedPatterns_.size() << "\n";
    for (const auto& sp : seedPatterns_) {
        f << sp.category << "\n" << sp.key << "\n" << sp.pattern << "\n" << sp.extractionTemplate << "\n";
    }
    
    return f.good();
}

bool LearnedFactExtractor::load(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return false;
    
    size_t numClassifiers;
    f >> numClassifiers;
    f.ignore();
    
    classifiers_.clear();
    for (size_t i = 0; i < numClassifiers; ++i) {
        auto clf = std::make_unique<Classifier>();
        std::getline(f, clf->category);
        std::getline(f, clf->key);
        size_t weightSize;
        f >> weightSize;
        clf->weights.resize(weightSize);
        for (size_t i = 0; i < weightSize; ++i) {
            f >> clf->weights[i];
        }
        f.ignore();
        classifiers_.push_back(std::move(clf));
    }
    
    size_t numPatterns;
    f >> numPatterns;
    f.ignore();
    
    seedPatterns_.clear();
    for (size_t i = 0; i < numPatterns; ++i) {
        SeedPattern sp;
        std::getline(f, sp.category);
        std::getline(f, sp.key);
        std::getline(f, sp.pattern);
        std::getline(f, sp.extractionTemplate);
        seedPatterns_.push_back(std::move(sp));
    }
    
    return f.good();
}

}} // namespace yuki::memory