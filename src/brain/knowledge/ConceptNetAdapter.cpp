#include "brain/knowledge/ConceptNetAdapter.h"
#include "brain/core/ConfigManager.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cctype>

namespace yuki::knowledge {

ConceptNetAdapter::ConceptNetAdapter(const std::string& config_path) {
    std::unordered_map<std::string, std::string> str_cfg;
    std::unordered_map<std::string, float> float_cfg;

    if (ConfigManager::instance().loadTemplates(config_path, str_cfg)) {
        if (str_cfg.count("target_lang")) target_lang_ = str_cfg["target_lang"];
        if (str_cfg.count("blocked_relations")) {
            std::stringstream ss(str_cfg["blocked_relations"]);
            std::string item;
            while (std::getline(ss, item, ',')) {
                if (!item.empty()) blocked_relations_.push_back(item);
            }
        }
    }
    if (ConfigManager::instance().loadFloatConfig(config_path, float_cfg)) {
        if (float_cfg.count("min_weight")) min_weight_ = float_cfg["min_weight"];
    }

    ConfigManager::instance().loadKeywords("data/knowledge_blocklist.txt", blocked_concepts_);
}

std::string ConceptNetAdapter::normalizeConcept(const std::string& raw) const {
    std::string s = raw;
    // Strip /c/en/ prefix if present
    size_t lang_pos = s.find("/c/en/");
    if (lang_pos != std::string::npos) {
        s = s.substr(lang_pos + 6);
    } else {
        size_t slash = s.rfind('/');
        if (slash != std::string::npos) {
            s = s.substr(slash + 1);
        }
    }
    // Replace underscores with spaces and lowercase
    for (char& c : s) {
        if (c == '_') c = ' ';
        else c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

uint64_t ConceptNetAdapter::computeTripletHash(const ConceptNetAssertion& a) const {
    std::string key = a.relation + "|" + a.start_concept + "|" + a.end_concept;
    uint64_t hash = 14695981039346656037ULL;
    for (char c : key) {
        hash ^= static_cast<uint64_t>(c);
        hash *= 1099511628211ULL;
    }
    return hash;
}

bool ConceptNetAdapter::isValid(const ConceptNetAssertion& a) const {
    if (a.weight < min_weight_) return false;
    if (a.start_concept.empty() || a.end_concept.empty()) return false;
    if (blocked_concepts_.count(a.start_concept) || blocked_concepts_.count(a.end_concept)) return false;

    for (const auto& br : blocked_relations_) {
        if (a.relation == br) return false;
    }
    return true;
}

ConceptNetAdapter::ParseStats ConceptNetAdapter::parseStream(
    const std::string& csv_path,
    std::function<bool(const ConceptNetAssertion&)> callback) {

    ParseStats stats;
    std::ifstream in(csv_path);
    if (!in.is_open()) return stats;

    std::unordered_set<uint64_t> seen_hashes;
    std::string line;
    uint64_t line_num = 0;

    while (std::getline(in, line)) {
        line_num++;
        stats.total++;
        if (line.empty() || line[0] == '#') continue;

        std::stringstream ss(line);
        std::string col;
        std::vector<std::string> cols;
        while (std::getline(ss, col, '\t')) {
            cols.push_back(col);
        }
        if (cols.size() < 5) {
            // Try comma fallback
            std::stringstream ss_comma(line);
            cols.clear();
            while (std::getline(ss_comma, col, ',')) {
                cols.push_back(col);
            }
        }

        if (cols.size() < 5) continue;

        ConceptNetAssertion a;
        a.line_number = line_num;
        a.relation = cols[0];
        // Strip /r/ prefix if present
        size_t rpos = a.relation.find("/r/");
        if (rpos != std::string::npos) a.relation = a.relation.substr(rpos + 3);

        std::string raw_start = cols[1];
        std::string raw_end = cols[2];

        // Language check
        if (raw_start.find("/c/" + target_lang_ + "/") == std::string::npos &&
            raw_start.find("/c/" + target_lang_) == std::string::npos) {
            stats.filtered_lang++;
            continue;
        }

        a.start_concept = normalizeConcept(raw_start);
        a.end_concept = normalizeConcept(raw_end);

        try {
            a.weight = std::stof(cols[4]);
        } catch (...) {
            a.weight = 1.0f;
        }

        if (a.weight < min_weight_) {
            stats.filtered_weight++;
            continue;
        }

        if (!isValid(a)) {
            stats.filtered_trivial++;
            continue;
        }

        uint64_t hash = computeTripletHash(a);
        if (seen_hashes.count(hash)) {
            stats.deduped++;
            continue;
        }

        seen_hashes.insert(hash);
        stats.accepted++;

        if (callback) {
            bool keep_going = callback(a);
            if (!keep_going) break;
        }
    }

    return stats;
}

ConceptNetAdapter::ParseStats ConceptNetAdapter::estimate(const std::string& csv_path) {
    ParseStats stats;
    std::ifstream in(csv_path);
    if (!in.is_open()) return stats;

    std::string line;
    uint64_t line_num = 0;
    uint64_t sampled = 0;

    while (std::getline(in, line)) {
        line_num++;
        if (line_num % 1000 != 0) continue;
        sampled++;

        std::stringstream ss(line);
        std::string col;
        std::vector<std::string> cols;
        while (std::getline(ss, col, '\t')) cols.push_back(col);
        if (cols.size() < 5) continue;

        ConceptNetAssertion a;
        a.relation = cols[0];
        size_t rpos = a.relation.find("/r/");
        if (rpos != std::string::npos) a.relation = a.relation.substr(rpos + 3);

        if (cols[1].find("/c/" + target_lang_ + "/") == std::string::npos) {
            stats.filtered_lang++;
            continue;
        }

        a.start_concept = normalizeConcept(cols[1]);
        a.end_concept = normalizeConcept(cols[2]);
        try { a.weight = std::stof(cols[4]); } catch (...) { a.weight = 1.0f; }

        if (a.weight >= min_weight_ && isValid(a)) {
            stats.accepted++;
        }
    }

    stats.total = line_num;
    if (sampled > 0) {
        double factor = static_cast<double>(line_num) / static_cast<double>(sampled);
        stats.accepted = static_cast<uint64_t>(stats.accepted * factor);
    }
    return stats;
}

} // namespace yuki::knowledge
