#include "MemoryEncoder.h"
#include <cmath>
#include <chrono>
#include <algorithm>

namespace yuki {
namespace memory {

std::vector<float> MemoryEncoder::encodeScores(
    float question, float command, float emotional,
    float technical, float urgency, float greeting,
    float action, float polarity,
    const std::string& source_tag,
    uint64_t /*timestamp_ms*/
) const {
    std::vector<float> vec(OUTPUT_DIM, 0.0f);

    // Dims 0-7: raw heuristic scores (amplified for separability)
    vec[0]  = question  * 2.0f;
    vec[1]  = command   * 2.0f;
    vec[2]  = emotional * 2.0f;
    vec[3]  = technical * 2.0f;
    vec[4]  = urgency   * 2.0f;
    vec[5]  = greeting  * 2.0f;
    vec[6]  = action    * 2.0f;
    vec[7]  = polarity  * 2.0f;

    // Dims 8-15: pairwise interactions (quadratic features)
    vec[8]  = question * command;
    vec[9]  = emotional * polarity;
    vec[10] = technical * urgency;
    vec[11] = greeting * action;
    vec[12] = question * urgency;
    vec[13] = command * action;
    vec[14] = emotional * greeting;
    vec[15] = technical * polarity;

    // Dims 16-19: source one-hot (stt, chat, web, system)
    if (source_tag == "stt")    vec[16] = 1.0f;
    else if (source_tag == "chat")   vec[17] = 1.0f;
    else if (source_tag == "web")    vec[18] = 1.0f;
    else if (source_tag == "system") vec[19] = 1.0f;
    else vec[19] = 0.5f; // unknown

    // Dims 20-23: normalization / entropy hints
    float sum = vec[0] + vec[1] + vec[2] + vec[3] + vec[4] + vec[5] + vec[6] + vec[7];
    float max_val = std::max({question, command, emotional, technical, urgency, greeting, action, polarity});
    vec[20] = sum / 8.0f;           // mean activation
    vec[21] = max_val;             // peak confidence
    vec[22] = (max_val > 0.7f) ? 1.0f : 0.0f; // dominant flag
    vec[23] = 1.0f;                // bias term

    return vec;
}

std::vector<float> MemoryEncoder::encodeText(const std::string& text) const {
    // Heuristic TF-like encoding for KnowledgeDaemon facts
    // Counts normalized by length, projected into 24 dims
    std::vector<float> vec(OUTPUT_DIM, 0.0f);
    if (text.empty()) return vec;

    float len = static_cast<float>(text.length());
    float upper = 0.0f, digit = 0.0f, punct = 0.0f, space = 0.0f;
    for (char c : text) {
        if (std::isupper(c)) upper += 1.0f;
        if (std::isdigit(c)) digit += 1.0f;
        if (std::ispunct(c)) punct += 1.0f;
        if (std::isspace(c)) space += 1.0f;
    }

    vec[0] = upper / len;
    vec[1] = digit / len;
    vec[2] = punct / len;
    vec[3] = space / len;
    vec[4] = std::log1p(len); // length feature

    // Simple keyword bins projected to remaining dims
    static const char* tech_words[] = {"code", "function", "class", "algorithm", "data", "model", "vector", "matrix"};
    static const char* social_words[] = {"hello", "hi", "please", "thanks", "sorry", "welcome", "goodbye"};
    static const char* urgent_words[] = {"now", "urgent", "asap", "broken", "error", "fail", "crash", "fix"};
    static const char* question_words[] = {"what", "how", "why", "when", "where", "which", "?"};

    float tech_score = 0.0f, social_score = 0.0f, urgent_score = 0.0f, question_score = 0.0f;
    std::string lower;
    lower.reserve(text.size());
    for (char c : text) lower.push_back(std::tolower(c));

    for (auto w : tech_words)    if (lower.find(w) != std::string::npos) tech_score += 1.0f;
    for (auto w : social_words)  if (lower.find(w) != std::string::npos) social_score += 1.0f;
    for (auto w : urgent_words)  if (lower.find(w) != std::string::npos) urgent_score += 1.0f;
    for (auto w : question_words) if (lower.find(w) != std::string::npos) question_score += 1.0f;

    vec[5] = tech_score;
    vec[6] = social_score;
    vec[7] = urgent_score;
    vec[8] = question_score;

    // Pad remainder with derived features
    for (size_t i = 9; i < OUTPUT_DIM; ++i) {
        vec[i] = vec[i % 9] * 0.1f;
    }
    return vec;
}

} // namespace memory
} // namespace yuki
