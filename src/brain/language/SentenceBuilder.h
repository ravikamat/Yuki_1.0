#pragma once
#include <string>
#include <vector>

namespace yuki::language {

class SentenceBuilder {
public:
    SentenceBuilder() = default;

    std::string buildResponse(const std::vector<std::string>& clauses) const;
    std::string addEmotionalColoring(const std::string& base, float valence, float arousal) const;
};

} // namespace yuki::language
