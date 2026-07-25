#include "brain/language/SentenceBuilder.h"
#include <sstream>

namespace yuki::language {

std::string SentenceBuilder::buildResponse(const std::vector<std::string>& clauses) const {
    if (clauses.empty()) return "";
    std::ostringstream oss;
    for (size_t i = 0; i < clauses.size(); ++i) {
        if (i > 0) oss << " ";
        oss << clauses[i];
    }
    return oss.str();
}

std::string SentenceBuilder::addEmotionalColoring(const std::string& base, float valence, float arousal) const {
    if (base.empty()) return base;
    std::string result = base;

    if (arousal > 0.7f && base.back() == '.') {
        result.back() = '!';
    }

    if (valence > 0.5f) {
        result += " :)";
    } else if (valence < -0.5f) {
        result += " (noted)";
    }

    return result;
}

} // namespace yuki::language
