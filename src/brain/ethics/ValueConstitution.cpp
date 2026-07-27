#include "brain/ethics/ValueConstitution.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <iostream>

namespace yuki::ethics {

ValueConstitution::ValueConstitution() = default;

ValueConstitution::ValueConstitution(yuki::language::Word2Vec* w2v)
    : w2v_(w2v) {}

std::string ValueConstitution::extractJsonString(const std::string& json, const std::string& key) const {
    std::string pattern = "\"" + key + "\":";
    size_t pos = json.find(pattern);
    if (pos == std::string::npos) return "";

    pos += pattern.size();
    while (pos < json.size() && (std::isspace(static_cast<unsigned char>(json[pos])) || json[pos] == '"')) pos++;
    size_t end = json.find('"', pos);
    if (end == std::string::npos) return "";
    return json.substr(pos, end - pos);
}

float ValueConstitution::extractJsonFloat(const std::string& json, const std::string& key, float default_val) const {
    std::string pattern = "\"" + key + "\":";
    size_t pos = json.find(pattern);
    if (pos == std::string::npos) return default_val;

    pos += pattern.size();
    while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) pos++;
    try {
        return std::stof(json.substr(pos));
    } catch (...) {
        return default_val;
    }
}

std::vector<std::string> ValueConstitution::extractJsonArray(const std::string& json, const std::string& key) const {
    std::vector<std::string> result;
    std::string pattern = "\"" + key + "\":";
    size_t pos = json.find(pattern);
    if (pos == std::string::npos) return result;

    pos = json.find('[', pos);
    if (pos == std::string::npos) return result;

    size_t end = json.find(']', pos);
    if (end == std::string::npos) return result;

    std::string content = json.substr(pos + 1, end - pos - 1);
    std::stringstream ss(content);
    std::string item;
    while (std::getline(ss, item, ',')) {
        size_t q1 = item.find('"');
        size_t q2 = item.rfind('"');
        if (q1 != std::string::npos && q2 != std::string::npos && q2 > q1) {
            result.push_back(item.substr(q1 + 1, q2 - q1 - 1));
        }
    }
    return result;
}

bool ValueConstitution::load(const std::string& path) {
    std::ifstream in(path);
    if (!in.is_open()) return false;

    std::string line;
    size_t count = 0;

    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;

        EthicalPrinciple p;
        p.id = extractJsonString(line, "id");
        p.name = extractJsonString(line, "name");
        p.source = extractJsonString(line, "source");
        p.teaching = extractJsonString(line, "teaching");
        p.base_weight = extractJsonFloat(line, "weight", 1.0f);
        p.domains = extractJsonArray(line, "domains");
        p.keywords = extractJsonArray(line, "keywords");

        if (!p.id.empty()) {
            principles_[p.id] = p;
            for (const auto& kw : p.keywords) {
                std::string lkw = kw;
                std::transform(lkw.begin(), lkw.end(), lkw.begin(), [](unsigned char c){ return std::tolower(c); });
                keyword_index_[lkw].push_back(p.id);
            }
            count++;
        }
    }

    return count > 0;
}

const EthicalPrinciple* ValueConstitution::getPrinciple(const std::string& id) const {
    auto it = principles_.find(id);
    if (it != principles_.end()) return &it->second;
    return nullptr;
}

float ValueConstitution::computeRelevance(const EthicalPrinciple& p,
                                          const std::string& action,
                                          const std::vector<std::string>& tags) const {
    // 1. Keyword match score
    std::stringstream ss(action);
    std::string token;
    size_t kw_matches = 0;
    while (ss >> token) {
        std::transform(token.begin(), token.end(), token.begin(), [](unsigned char c){ return std::tolower(c); });
        for (const auto& pkw : p.keywords) {
            std::string lpkw = pkw;
            std::transform(lpkw.begin(), lpkw.end(), lpkw.begin(), [](unsigned char c){ return std::tolower(c); });
            if (token == lpkw || token.find(lpkw) != std::string::npos) {
                kw_matches++;
                break;
            }
        }
    }
    float kw_score = p.keywords.empty() ? 0.0f : static_cast<float>(kw_matches) / static_cast<float>(p.keywords.size());

    // 2. Domain match score
    size_t domain_matches = 0;
    for (const auto& tag : tags) {
        for (const auto& pdom : p.domains) {
            if (tag == pdom) {
                domain_matches++;
                break;
            }
        }
    }
    float dom_score = p.domains.empty() ? 0.0f : static_cast<float>(domain_matches) / static_cast<float>(p.domains.size());

    return std::min(1.0f, 0.6f * kw_score + 0.4f * dom_score + 0.1f);
}

float ValueConstitution::computeAlignment(const EthicalPrinciple& p, const std::string& action) const {
    if (w2v_) {
        auto getSentenceVector = [&](const std::string& text) {
            std::vector<float> vec(300, 0.0f);
            std::stringstream ss(text);
            std::string token;
            size_t count = 0;
            while (ss >> token) {
                auto v = w2v_->getVector(token);
                for (size_t i = 0; i < 300 && i < v.size(); ++i) vec[i] += v[i];
                count++;
            }
            if (count > 1) {
                for (float& f : vec) f /= static_cast<float>(count);
            }
            return vec;
        };

        auto vec_act = getSentenceVector(action);
        auto vec_teach = getSentenceVector(p.teaching);
        float cosine = w2v_->cosineSimilarity(vec_act, vec_teach);
        // Sigmoid scale cosine to [-1, +1]
        return (2.0f / (1.0f + std::exp(-5.0f * cosine))) - 1.0f;
    }

    // Keyword overlap fallback if Word2Vec pointer is null
    std::stringstream ss(action);
    std::string token;
    bool has_negative_kw = (action.find("destroy") != std::string::npos ||
                            action.find("delete") != std::string::npos ||
                            action.find("harm") != std::string::npos ||
                            action.find("cheat") != std::string::npos);

    return has_negative_kw ? -0.6f : 0.4f;
}

ValueAlignmentReport ValueConstitution::evaluate(const std::string& action_description,
                                                 const std::vector<std::string>& context_tags) const {
    ValueAlignmentReport report;
    if (principles_.empty()) return report;

    float weighted_sum = 0.0f;
    float weight_total = 0.0f;

    for (const auto& [id, p] : principles_) {
        float rel = computeRelevance(p, action_description, context_tags);
        if (rel > 0.05f) {
            float align = computeAlignment(p, action_description);
            float eff_weight = rel * p.base_weight;

            weighted_sum += eff_weight * align;
            weight_total += eff_weight;

            report.top_principles.push_back({p.id, rel});

            if (align < -0.3f) {
                report.warnings.push_back("Warning: Action misaligned with " + p.id + " (" + p.name + ")");
            }
        }
    }

    if (weight_total > 0.0f) {
        report.alignment_score = weighted_sum / weight_total;
    }

    std::sort(report.top_principles.begin(), report.top_principles.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });

    return report;
}

void ValueConstitution::save(const std::string& path) const {
    std::ofstream out(path, std::ios::binary);
    if (!out.is_open()) return;

    uint32_t count = static_cast<uint32_t>(principles_.size());
    out.write(reinterpret_cast<const char*>(&count), sizeof(count));

    for (const auto& [id, p] : principles_) {
        uint32_t len = static_cast<uint32_t>(p.id.size());
        out.write(reinterpret_cast<const char*>(&len), sizeof(len));
        out.write(p.id.data(), len);

        len = static_cast<uint32_t>(p.name.size());
        out.write(reinterpret_cast<const char*>(&len), sizeof(len));
        out.write(p.name.data(), len);

        out.write(reinterpret_cast<const char*>(&p.base_weight), sizeof(p.base_weight));
    }
}

bool ValueConstitution::loadBinary(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) return false;

    uint32_t count = 0;
    in.read(reinterpret_cast<char*>(&count), sizeof(count));

    for (uint32_t i = 0; i < count; ++i) {
        EthicalPrinciple p;
        uint32_t len = 0;
        in.read(reinterpret_cast<char*>(&len), sizeof(len));
        p.id.resize(len);
        in.read(&p.id[0], len);

        in.read(reinterpret_cast<char*>(&len), sizeof(len));
        p.name.resize(len);
        in.read(&p.name[0], len);

        in.read(reinterpret_cast<char*>(&p.base_weight), sizeof(p.base_weight));

        principles_[p.id] = p;
    }
    return true;
}

} // namespace yuki::ethics
