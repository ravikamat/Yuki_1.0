#include "input/InputAnalyzer.h"
#include <fstream>
#include <algorithm>
#include <cctype>
#include <sstream>

namespace yuki::input {

InputAnalyzer::InputAnalyzer() {
    // Default fallback prefixes
    command_prefixes_ = {"open", "run", "build", "find", "show", "search", "clean", "exit"};
}

bool InputAnalyzer::loadPrefixesFromFile(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file) return false;

    command_prefixes_.clear();
    std::string line;
    while (std::getline(file, line)) {
        stripWhitespace(line);
        if (!line.empty() && line[0] != '#') {
            std::transform(line.begin(), line.end(), line.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            command_prefixes_.insert(line);
        }
    }
    return !command_prefixes_.empty();
}

void InputAnalyzer::normalizeUnicode(std::string& text) {
    // Strip UTF-8 BOM if present (\xEF\xBB\xBF)
    if (text.size() >= 3 &&
        static_cast<unsigned char>(text[0]) == 0xEF &&
        static_cast<unsigned char>(text[1]) == 0xBB &&
        static_cast<unsigned char>(text[2]) == 0xBF) {
        text.erase(0, 3);
    }
}

void InputAnalyzer::stripWhitespace(std::string& text) {
    // Trim leading/trailing whitespace and collapse multiple spaces
    auto start = text.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        text.clear();
        return;
    }
    auto end = text.find_last_not_of(" \t\r\n");
    std::string trimmed = text.substr(start, end - start + 1);

    std::string result;
    result.reserve(trimmed.size());
    bool last_was_space = false;
    for (char c : trimmed) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            if (!last_was_space) {
                result.push_back(' ');
                last_was_space = true;
            }
        } else {
            result.push_back(c);
            last_was_space = false;
        }
    }
    text = result;
}

std::string InputAnalyzer::detectCommandPrefix(const std::string& text) const {
    if (text.empty()) return "";
    std::istringstream iss(text);
    std::string first_word;
    iss >> first_word;

    std::transform(first_word.begin(), first_word.end(), first_word.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    if (command_prefixes_.count(first_word)) {
        return first_word;
    }
    return "";
}

std::vector<size_t> InputAnalyzer::detectEmoticons(const std::string& text) const {
    std::vector<size_t> pos;
    static const std::vector<std::string> emoticons = {":)", ":(", ":D", ";)", ":P", "<3"};
    for (const auto& emo : emoticons) {
        size_t p = text.find(emo);
        while (p != std::string::npos) {
            pos.push_back(p);
            p = text.find(emo, p + emo.size());
        }
    }
    std::sort(pos.begin(), pos.end());
    return pos;
}

InputType InputAnalyzer::classifyInputType(const std::string& text) const {
    if (text.empty()) return InputType::STATEMENT;

    if (text.back() == '?') {
        return InputType::QUESTION;
    }

    if (!detectCommandPrefix(text).empty()) {
        return InputType::COMMAND;
    }

    return InputType::STATEMENT;
}

} // namespace yuki::input
