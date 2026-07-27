#include "input/encoding/TextEncoder.h"
#include "brain/core/ConfigManager.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
    // Word-boundary keyword match: prevents "run" from matching "brunette"
    bool contains_word(const std::string& haystack, const std::string& needle) {
        if (needle.empty() || haystack.empty()) return false;
        size_t pos = 0;
        while (true) {
            pos = haystack.find(needle, pos);
            if (pos == std::string::npos) return false;
            bool left_boundary = (pos == 0) ||
                !std::isalnum(static_cast<unsigned char>(haystack[pos - 1]));
            bool right_boundary = (pos + needle.length() >= haystack.length()) ||
                !std::isalnum(static_cast<unsigned char>(haystack[pos + needle.length()]));
            if (left_boundary && right_boundary) return true;
            ++pos;
        }
    }
}

namespace yuki::perception {

static inline float randUniform(std::mt19937& rng, float min, float max) {
    std::uniform_real_distribution<float> dist(min, max);
    return dist(rng);
}

static inline float randNormal(std::mt19937& rng, float mean, float stddev) {
    std::normal_distribution<float> dist(mean, stddev);
    return dist(rng);
}

TextEncoder::TextEncoder(const Word2VecConfig& cfg) : cfg_(cfg), rng_(42) {
    word_to_id_["<<OOV>>"] = 0;
    word_counts_.push_back(0);
    initProjectionMatrix();
}

void TextEncoder::initProjectionMatrix() {
    projection_.resize(cfg_.embed_dim * cfg_.project_dim);
    float scale = 1.0f / std::sqrt(static_cast<float>(cfg_.embed_dim));
    for (size_t i = 0; i < projection_.size(); ++i) {
        projection_[i] = randNormal(rng_, 0.0f, scale);
    }
}

std::vector<std::string> TextEncoder::tokenize(const std::string& text) const {
    std::vector<std::string> tokens;
    std::string current;
    for (char ch : text) {
        if (std::isalnum(static_cast<unsigned char>(ch))) {
            current.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
        } else {
            if (!current.empty()) { tokens.push_back(current); current.clear(); }
        }
    }
    if (!current.empty()) tokens.push_back(current);
    return tokens;
}

uint32_t TextEncoder::getIdOrOOV(const std::string& word) const {
    auto it = word_to_id_.find(word);
    return (it != word_to_id_.end()) ? it->second : 0;
}

void TextEncoder::buildVocabulary(const std::vector<std::string>& corpus) {
    std::unordered_map<std::string, uint64_t> freq;
    for (const auto& sentence : corpus) {
        for (const auto& tok : tokenize(sentence)) freq[tok]++;
    }
    std::vector<std::pair<std::string, uint64_t>> sorted_freq(freq.begin(), freq.end());
    std::sort(sorted_freq.begin(), sorted_freq.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });
    for (const auto& p : sorted_freq) {
        if (word_to_id_.size() >= cfg_.max_vocab) break;
        if (word_to_id_.find(p.first) == word_to_id_.end()) {
            word_to_id_[p.first] = static_cast<uint32_t>(word_to_id_.size());
            word_counts_.push_back(p.second);
        }
    }
    size_t vocab_size = word_to_id_.size();
    embeddings_.resize(vocab_size * cfg_.embed_dim);
    float init_range = 0.5f / static_cast<float>(cfg_.embed_dim);
    for (size_t i = 0; i < embeddings_.size(); ++i) {
        embeddings_[i] = randUniform(rng_, -init_range, init_range);
    }
}

void TextEncoder::seedCurriculumVocabulary(const std::vector<std::string>& topic_names) {
    for (const auto& topic : topic_names) {
        for (const auto& tok : tokenize(topic)) {
            if (word_to_id_.find(tok) == word_to_id_.end() && word_to_id_.size() < cfg_.max_vocab) {
                word_to_id_[tok] = static_cast<uint32_t>(word_to_id_.size());
                word_counts_.push_back(10);
            }
        }
    }
    if (embeddings_.empty() && !word_to_id_.empty()) {
        size_t vocab_size = word_to_id_.size();
        embeddings_.resize(vocab_size * cfg_.embed_dim);
        float init_range = 0.5f / static_cast<float>(cfg_.embed_dim);
        for (size_t i = 0; i < embeddings_.size(); ++i) {
            embeddings_[i] = randUniform(rng_, -init_range, init_range);
        }
    }
}

std::vector<float> TextEncoder::encode(const std::string& text) const {
    HeuristicScores scores;
    std::string lower = text;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    
    scores.question = scoreQuestion(lower);
    scores.command  = scoreCommand(lower);
    scores.emotional = scoreEmotional(lower);
    scores.technical = scoreTechnical(lower);
    scores.urgency   = scoreUrgency(lower);
    scores.greeting  = scoreGreeting(lower);
    scores.action    = scoreAction(lower);
    scores.polarity  = scorePolarity(lower);
    scores.phatic    = scorePhatic(lower);

    last_scores_ = scores;


    auto tokens = tokenize(text);
    std::vector<float> vec(cfg_.embed_dim, 0.0f);

    if (!tokens.empty() && !embeddings_.empty()) {
        size_t valid_count = 0;
        for (const auto& tok : tokens) {
            uint32_t id = getIdOrOOV(tok);
            if (id == 0) continue;
            size_t offset = static_cast<size_t>(id) * cfg_.embed_dim;
            if (offset + cfg_.embed_dim > embeddings_.size()) continue;
            for (size_t d = 0; d < cfg_.embed_dim; ++d) vec[d] += embeddings_[offset + d];
            valid_count++;
        }
        if (valid_count > 0) {
            float inv = 1.0f / static_cast<float>(valid_count);
            for (auto& v : vec) v *= inv;
        }
    }

    if (vec.size() >= 9) {
        vec[0] = scores.question;
        vec[1] = scores.command;
        vec[2] = scores.emotional;
        vec[3] = scores.technical;
        vec[4] = scores.urgency;
        vec[5] = scores.greeting;
        vec[6] = scores.action;
        vec[7] = scores.polarity;
        vec[8] = scores.phatic;
    }

    return vec;
}

std::vector<float> TextEncoder::projectTo8(const std::vector<float>& vec64) const {
    if (vec64.size() != cfg_.embed_dim)
        throw std::runtime_error("projectTo8: dimension mismatch");
    std::vector<float> out(cfg_.project_dim, 0.0f);
    for (size_t j = 0; j < cfg_.project_dim; ++j) {
        float sum = 0.0f;
        for (size_t i = 0; i < cfg_.embed_dim; ++i) {
            sum += vec64[i] * projection_[i * cfg_.project_dim + j];
        }
        out[j] = sum;
    }
    l2Normalize(out);
    return out;
}

float TextEncoder::sigmoid(float x) const {
    if (x >= 0.0f) {
        float z = std::exp(-x);
        return 1.0f / (1.0f + z);
    } else {
        float z = std::exp(x);
        return z / (1.0f + z);
    }
}

void TextEncoder::l2Normalize(std::vector<float>& vec) const {
    float sq = 0.0f;
    for (float v : vec) sq += v * v;
    if (sq > 1e-8f) {
        float inv = 1.0f / std::sqrt(sq);
        for (auto& v : vec) v *= inv;
    } else {
        float val = 1.0f / std::sqrt(static_cast<float>(vec.size()));
        for (auto& v : vec) v = val;
    }
}

std::vector<float> TextEncoder::noiseDistribution() const {
    std::vector<float> dist(word_counts_.size());
    float sum = 0.0f;
    for (size_t i = 0; i < word_counts_.size(); ++i) {
        dist[i] = std::pow(static_cast<float>(word_counts_[i]), 0.75f);
        sum += dist[i];
    }
    if (sum > 0.0f) for (auto& d : dist) d /= sum;
    return dist;
}

void TextEncoder::trainStep(const std::string& sentence, float lr) {
    if (embeddings_.empty()) return;
    auto tokens = tokenize(sentence);
    if (tokens.size() < 2) return;

    std::vector<uint32_t> ids;
    ids.reserve(tokens.size());
    for (const auto& tok : tokens) ids.push_back(getIdOrOOV(tok));

    auto neg_dist = noiseDistribution();
    if (neg_dist.empty()) return;
    std::discrete_distribution<uint32_t> neg_sampler(neg_dist.begin(), neg_dist.end());

    for (size_t i = 0; i < ids.size(); ++i) {
        uint32_t center_id = ids[i];
        if (center_id == 0) continue;

        size_t win_start = (i > cfg_.window_size) ? (i - cfg_.window_size) : 0;
        size_t win_end   = std::min(i + cfg_.window_size + 1, ids.size());

        float* center_emb = embeddings_.data() + center_id * cfg_.embed_dim;

        for (size_t j = win_start; j < win_end; ++j) {
            if (j == i) continue;
            uint32_t context_id = ids[j];
            const float* context_emb = embeddings_.data() + context_id * cfg_.embed_dim;

            float dot_pos = 0.0f;
            for (size_t d = 0; d < cfg_.embed_dim; ++d) dot_pos += center_emb[d] * context_emb[d];
            float grad_pos = (1.0f - sigmoid(dot_pos)) * lr;
            grad_pos = std::max(-1.0f, std::min(1.0f, grad_pos));
            for (size_t d = 0; d < cfg_.embed_dim; ++d) center_emb[d] += grad_pos * context_emb[d];

            for (int n = 0; n < cfg_.negative_samples; ++n) {
                uint32_t neg_id = neg_sampler(rng_);
            }
        }
    }
}

TextEncoder::HeuristicScores TextEncoder::getLastScores() const {
    return last_scores_;
}

float TextEncoder::scorePhatic(const std::string& lower) const {
    std::unordered_set<std::string> phaticKeywords;
    yuki::ConfigManager::instance().loadKeywords("data/social_keywords.txt", phaticKeywords);
    if (phaticKeywords.find(lower) != phaticKeywords.end()) return 1.0f;

    std::vector<std::pair<std::string, float>> patterns;
    yuki::ConfigManager::instance().loadPatterns("data/self_detection_patterns.txt", patterns);
    for (const auto& [prefix, score] : patterns) {
        if (lower.size() >= prefix.size() && lower.compare(0, prefix.size(), prefix) == 0) {
            return score;
        }
    }
    return 0.0f;
}

float TextEncoder::scoreQuestion(const std::string& lower) const {
    float score = 0.0f;
    static const std::vector<std::string> wh = {
        "what", "how", "why", "when", "where", "who", "which"
    };
    for (const auto& w : wh) {
        if (contains_word(lower, w)) score += 0.25f;
    }
    if (contains_word(lower, "tell me")) score += 0.65f;
    if (contains_word(lower, "explain")) score += 0.65f;
    if (contains_word(lower, "what is")) score += 0.2f;
    if (contains_word(lower, "how to")) score += 0.2f;
    if (!lower.empty() && lower.back() == '?') score += 0.35f;
    return std::min(score, 1.0f);
}

float TextEncoder::scoreCommand(const std::string& lower) const {
    static const std::vector<std::string> keywords = {
        "run", "execute", "delete", "remove", "wipe", "open", "close",
        "enable", "disable", "turn", "write", "start", "show", "get",
        "set", "install", "send", "launch", "save", "load", "move",
        "copy", "print", "list", "find", "search", "stop", "restart"
    };
    for (const auto& w : keywords) {
        if (contains_word(lower, w)) return 0.8f;
    }
    return 0.0f;
}

float TextEncoder::scoreEmotional(const std::string& lower) const {
    static const std::vector<std::string> keywords = {
        "feel", "sad", "happy", "frustrated", "angry", "stressed", "hate", "love",
        "amazing", "awesome", "incredible", "excited", "terrible", "horrible",
        "wonderful", "fantastic", "great job", "proud", "disappointed", "upset"
    };
    for (const auto& w : keywords) {
        if (contains_word(lower, w)) return 0.8f;
    }
    return 0.0f;
}

float TextEncoder::scoreTechnical(const std::string& lower) const {
    static const std::vector<std::string> keywords = {
        // Programming languages and technical CS concepts only
        // Removing subject-matter words (photosynthesis, quantum) that are
        // QUERY topics, not TUTORIAL explanations.
        // Removing "api" — too common in clarification contexts.
        "python", "cpp", "c++", "java", "code", "class", "pointer",
        "memory", "algorithm", "function", "variable", "loop", "array",
        "recursion", "database", "sql", "framework", "library"
    };
    for (const auto& w : keywords) {
        if (contains_word(lower, w)) return 0.8f;
    }
    return 0.0f;
}

float TextEncoder::scoreUrgency(const std::string& lower) const {
    static const std::vector<std::string> keywords = {
        "urgent", "now", "immediately", "quick", "fast", "abort", "stop",
        "nevermind", "never mind", "cancel", "forget it", "quit", "no more"
    };
    for (const auto& w : keywords) {
        if (contains_word(lower, w)) return 0.8f;
    }
    if (lower.find('!') != std::string::npos) return 0.6f;
    return 0.0f;
}

float TextEncoder::scoreGreeting(const std::string& lower) const {
    static const std::vector<std::string> keywords = {
        "hello", "hi", "hey", "good morning", "good night"
    };
    for (const auto& w : keywords) {
        if (contains_word(lower, w)) return 0.8f;
    }
    // Identity/social questions also map to greeting (META_QUESTION intent)
    // "what is your name", "who are you", "are you yuki", "your name is"
    if (contains_word(lower, "your name")) return 0.8f;
    if (contains_word(lower, "who are you")) return 0.8f;
    if (contains_word(lower, "are you")) return 0.6f;
    if (contains_word(lower, "introduce yourself")) return 0.8f;
    return 0.0f;
}

float TextEncoder::scoreAction(const std::string& lower) const {
    static const std::vector<std::string> keywords = {
        "do", "make", "build", "create", "organize", "run",
        "open", "write", "start", "show", "get", "set", "install",
        "send", "launch", "save", "load", "move", "copy", "print"
    };
    for (const auto& w : keywords) {
        if (contains_word(lower, w)) return 0.8f;
    }
    return 0.0f;
}

float TextEncoder::scorePolarity(const std::string& lower) const {
    static const std::vector<std::string> keywords = {
        "good", "bad", "yes", "no", "wrong", "correct", "right"
    };
    for (const auto& w : keywords) {
        if (contains_word(lower, w)) return 0.8f;
    }
    return 0.0f;
}

} // namespace yuki::perception
