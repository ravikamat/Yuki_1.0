#include "CandidateGenerator.h"
#include <algorithm>
#include <cctype>

CandidateGenerator::CandidateGenerator() {}

std::string CandidateGenerator::hinglishPhoneticHash(const std::string& word) const {
    if (word.empty()) return "";
    std::string hash;
    hash.reserve(word.size());
    
    char last = 0;
    for (size_t i = 0; i < word.size(); ++i) {
        char c = static_cast<char>(std::tolower(static_cast<unsigned char>(word[i])));
        if (c == last) continue;
        
        if (c == 'e' && i + 1 < word.size() && std::tolower(word[i+1]) == 'e') {
            hash += 'i'; last = 'e'; ++i; continue;
        }
        if (c == 'o' && i + 1 < word.size() && std::tolower(word[i+1]) == 'o') {
            hash += 'u'; last = 'o'; ++i; continue;
        }
        if (c == 'c' && i + 1 < word.size() && std::tolower(word[i+1]) == 'h') {
            hash += 'x'; last = 'h'; ++i; continue;
        }
        if (c == 's' && i + 1 < word.size() && std::tolower(word[i+1]) == 'h') {
            hash += 'S'; last = 'h'; ++i; continue;
        }
        
        if (c == 'v') c = 'w';
        if (c == 'z') c = 's';
        if (c == 'q') c = 'k';
        if (c == 'c') c = 'k';
        
        hash += c;
        last = c;
    }
    return hash;
}

float CandidateGenerator::computePhoneticScore(const std::string& a, const std::string& b) const {
    std::string ha = hinglishPhoneticHash(a);
    std::string hb = hinglishPhoneticHash(b);
    if (ha == hb) return 1.0f;
    if (ha.empty() || hb.empty()) return 0.0f;

    std::vector<std::vector<int>> d(ha.size() + 1, std::vector<int>(hb.size() + 1));
    for (size_t i = 0; i <= ha.size(); ++i) d[i][0] = i;
    for (size_t j = 0; j <= hb.size(); ++j) d[0][j] = j;

    for (size_t i = 1; i <= ha.size(); ++i) {
        for (size_t j = 1; j <= hb.size(); ++j) {
            int cost = (ha[i - 1] == hb[j - 1]) ? 0 : 1;
            d[i][j] = std::min({ d[i - 1][j] + 1, d[i][j - 1] + 1, d[i - 1][j - 1] + cost });
        }
    }
    
    int dist = d[ha.size()][hb.size()];
    int max_len = std::max(ha.size(), hb.size());
    return 1.0f - (static_cast<float>(dist) / static_cast<float>(max_len));
}

std::vector<CandidateResult> CandidateGenerator::generate(const NormalizedInput& original, const UncertaintyReport& uncertainty) {
    std::vector<CandidateResult> results;
    (void)original;

    for (const auto& flag : uncertainty.token_flags) {
        CandidateResult bestRes;
        bestRes.text = flag.token;
        bestRes.phoneticScore = 1.0f;
        bestRes.editScore = 1.0f;
        bestRes.score = 1.0f;
        results.push_back(bestRes);
    }
    return results;
}

