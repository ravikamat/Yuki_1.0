#include "LanguageLayer.h"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <vector>

// Helper to check for Devanagari Unicode range in UTF-8
static bool containsDevanagari(const std::string& str) {
    for (size_t i = 0; i < str.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(str[i]);
        if (c == 0xE0 && i + 2 < str.size()) {
            unsigned char c1 = static_cast<unsigned char>(str[i+1]);
            unsigned char c2 = static_cast<unsigned char>(str[i+2]);
            if ((c1 == 0xA4 || c1 == 0xA5) && (c2 >= 0x80 && c2 <= 0xBF)) {
                return true;
            }
        }
    }
    return false;
}

// Tokenize helper splitting by whitespace and punctuation
static std::vector<std::string> tokenizeWords(const std::string& str) {
    std::vector<std::string> words;
    std::string current;
    for (char c : str) {
        if (std::isspace(static_cast<unsigned char>(c)) || std::ispunct(static_cast<unsigned char>(c))) {
            if (!current.empty()) {
                words.push_back(current);
                current.clear();
            }
        } else {
            current += c;
        }
    }
    if (!current.empty()) {
        words.push_back(current);
    }
    return words;
}

// Case-insensitive Hinglish keyword checker
static bool isHinglishWord(const std::string& word) {
    std::string w = word;
    std::transform(w.begin(), w.end(), w.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return (w == "aaj" || w == "batao" || w == "kholo" || w == "karo" ||
            w == "ko" || w == "pe" || w == "aur" || w == "koi");
}

// Helper to translate single Hinglish word
static std::string translateWord(const std::string& word) {
    std::string w = word;
    size_t start = 0;
    while (start < w.size() && std::ispunct(static_cast<unsigned char>(w[start]))) {
        start++;
    }
    size_t end = w.size();
    while (end > start && std::ispunct(static_cast<unsigned char>(w[end - 1]))) {
        end--;
    }
    
    std::string clean = w.substr(start, end - start);
    std::string lower = clean;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    
    std::string translated = clean;
    bool found = false;
    
    if (lower == "aaj") { translated = "today"; found = true; }
    else if (lower == "batao") { translated = "tell"; found = true; }
    else if (lower == "kholo") { translated = "open"; found = true; }
    else if (lower == "karo") { translated = "do"; found = true; }
    else if (lower == "ko") { translated = "to"; found = true; }
    else if (lower == "pe") { translated = "on"; found = true; }
    else if (lower == "aur") { translated = "and"; found = true; }
    else if (lower == "koi") { translated = "any"; found = true; }
    else if (lower == "\xE0\xA4\x86\xE0\xA4\x9C") { translated = "today"; found = true; }
    
    if (found) {
        if (!clean.empty() && std::isupper(static_cast<unsigned char>(clean[0]))) {
            translated[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(translated[0])));
        }
        return w.substr(0, start) + translated + w.substr(end);
    }
    return word;
}

// Translates full text word-by-word while preserving whitespace
static std::string translateText(const std::string& input) {
    std::string result;
    std::string current;
    for (char c : input) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            if (!current.empty()) {
                result += translateWord(current);
                current.clear();
            }
            result += c;
        } else {
            current += c;
        }
    }
    if (!current.empty()) {
        result += translateWord(current);
    }
    return result;
}

LanguageLayer::LanguageLayer() {}

LanguageResult LanguageLayer::detect(const std::string& input) const {
    LanguageResult res;
    res.detected_text = input;
    
    // Check Devanagari first
    if (containsDevanagari(input)) {
        res.detected = DetectedLanguage::HINDI_DEVANAGARI;
        res.code = "hi";
        res.languageCode = "hi";
        res.responseStyle = "hindi";
        res.needsTranslation = true;
    } else {
        // Tokenize and check Hinglish words
        std::vector<std::string> words = tokenizeWords(input);
        bool hasHinglish = false;
        for (const auto& w : words) {
            if (isHinglishWord(w)) {
                hasHinglish = true;
                break;
            }
        }
        if (hasHinglish) {
            res.detected = DetectedLanguage::HINGLISH;
            res.code = "hi-en";
            res.languageCode = "hi-en";
            res.responseStyle = "hinglish";
            res.needsTranslation = true;
        } else {
            res.detected = DetectedLanguage::ENGLISH;
            res.code = "en";
            res.languageCode = "en";
            res.responseStyle = "english";
            res.needsTranslation = false;
        }
    }
    
    if (res.needsTranslation) {
        res.translated_english = translateText(input);
        res.normalizedEnglish = res.translated_english;
    } else {
        res.translated_english = input;
        res.normalizedEnglish = input;
    }
    
    return res;
}

LanguageResult LanguageLayer::analyse(const std::string& input) const {
    return detect(input);
}

std::string LanguageLayer::adaptResponse(const std::string& englishResponse, const LanguageResult& lr) const {
    return englishResponse;
}
