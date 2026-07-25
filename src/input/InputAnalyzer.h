#pragma once
#include <string>
#include <vector>
#include <unordered_set>

namespace yuki::input {

enum class InputType : uint8_t {
    STATEMENT = 0,
    QUESTION,
    COMMAND
};

class InputAnalyzer {
public:
    InputAnalyzer();

    bool loadPrefixesFromFile(const std::string& filepath);
    void normalizeUnicode(std::string& text);
    void stripWhitespace(std::string& text);
    std::string detectCommandPrefix(const std::string& text) const;
    std::vector<size_t> detectEmoticons(const std::string& text) const;
    InputType classifyInputType(const std::string& text) const;

private:
    std::unordered_set<std::string> command_prefixes_;
};

} // namespace yuki::input
