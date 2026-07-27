#include "brain/language/EnglishLanguageEngine.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace yuki::language {

uint64_t EnglishLanguageEngine::fnv1a(const std::string& s) const {
    uint64_t h = 0xcbf29ce484222325ULL;
    for (unsigned char c : s) {
        h ^= std::tolower(c);
        h *= 0x100000001b3ULL;
    }
    return h;
}

bool EnglishLanguageEngine::loadFromDataDirectory(const std::string& data_dir) {
    std::string dict_path = data_dir + "/vocabulary_base.txt";
    std::string exc_path = data_dir + "/spell_exceptions.txt";

    std::ifstream fdict(dict_path);
    if (fdict) {
        std::string word;
        while (fdict >> word) {
            std::transform(word.begin(), word.end(), word.begin(), [](unsigned char c){ return std::tolower(c); });
            dictionary_hashes_.insert(fnv1a(word));
            known_words_.insert(word);
        }
    }

    std::ifstream fexc(exc_path);
    if (fexc) {
        std::string word;
        while (fexc >> word) {
            std::transform(word.begin(), word.end(), word.begin(), [](unsigned char c){ return std::tolower(c); });
            spell_exceptions_.insert(word);
        }
    }

    return (!known_words_.empty() || !spell_exceptions_.empty());
}

bool EnglishLanguageEngine::isWordKnown(const std::string& word) const {
    std::string lower = word;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c){ return std::tolower(c); });

    if (spell_exceptions_.count(lower)) return true;
    if (dictionary_hashes_.count(fnv1a(lower))) return true;
    return false;
}

size_t EnglishLanguageEngine::editDistance(const std::string& s1, const std::string& s2) const {
    const size_t m = s1.size();
    const size_t n = s2.size();
    std::vector<std::vector<size_t>> dp(m + 1, std::vector<size_t>(n + 1));

    for (size_t i = 0; i <= m; ++i) dp[i][0] = i;
    for (size_t j = 0; j <= n; ++j) dp[0][j] = j;

    for (size_t i = 1; i <= m; ++i) {
        for (size_t j = 1; j <= n; ++j) {
            if (std::tolower(s1[i - 1]) == std::tolower(s2[j - 1])) {
                dp[i][j] = dp[i - 1][j - 1];
            } else {
                dp[i][j] = 1 + std::min({dp[i - 1][j], dp[i][j - 1], dp[i - 1][j - 1]});
            }
        }
    }
    return dp[m][n];
}

std::string EnglishLanguageEngine::suggestCorrection(const std::string& word) const {
    if (isWordKnown(word)) return word;

    std::string best_match = word;
    size_t min_dist = 999;

    for (const auto& w : known_words_) {
        size_t dist = editDistance(word, w);
        if (dist < min_dist) {
            min_dist = dist;
            best_match = w;
        }
    }
    return best_match;
}

bool EnglishLanguageEngine::checkGrammar(const std::string& sentence, std::vector<std::string>& errors) const {
    errors.clear();
    if (sentence.empty()) return true;

    // Simple structural grammar check: double spaces, missing capitalization
    if (sentence.find("  ") != std::string::npos) {
        errors.push_back("Double spaces detected");
    }
    if (std::islower(static_cast<unsigned char>(sentence[0]))) {
        errors.push_back("Sentence should start with a capital letter");
    }
    return errors.empty();
}

} // namespace yuki::language
