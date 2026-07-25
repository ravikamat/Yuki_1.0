#include "brain/language/SentenceMaker.h"
#include <fstream>
#include <sstream>

namespace yuki::language {

bool SentenceMaker::loadTemplates(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file) return false;

    templates_.clear();
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream iss(line);
        std::string id, pattern;
        if (std::getline(iss, id, '\t') && std::getline(iss, pattern, '\t')) {
            templates_[id] = pattern;
        }
    }
    return !templates_.empty();
}

std::string SentenceMaker::compose(const std::string& template_id,
                                    const std::unordered_map<std::string, std::string>& slots) const {
    auto it = templates_.find(template_id);
    if (it == templates_.end()) return "";

    std::string result = it->second;
    for (const auto& [key, val] : slots) {
        std::string tag = "{" + key + "}";
        size_t pos = result.find(tag);
        while (pos != std::string::npos) {
            result.replace(pos, tag.size(), val);
            pos = result.find(tag, pos + val.size());
        }
    }
    return result;
}

size_t SentenceMaker::templateCount() const {
    return templates_.size();
}

} // namespace yuki::language
