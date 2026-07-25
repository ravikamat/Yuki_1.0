#pragma once
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <vector>

namespace yuki::language {

class EnglishLanguageEngine {
public:
    EnglishLanguageEngine() = default;

    bool loadFromDataDirectory(const std::string& data_dir);
    bool isWordKnown(const std::string& word) const;
    std::string suggestCorrection(const std::string& word) const;
    bool checkGrammar(const std::string& sentence, std::vector<std::string>& errors) const;

private:
    std::unordered_set<uint64_t> dictionary_hashes_;
    std::unordered_set<std::string> known_words_;
    std::unordered_set<std::string> spell_exceptions_;
    std::unordered_map<std::string, std::string> grammar_rules_;

    uint64_t fnv1a(const std::string& s) const;
    size_t editDistance(const std::string& s1, const std::string& s2) const;
};

} // namespace yuki::language
