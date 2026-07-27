#pragma once
#include <string>
#include <unordered_map>
#include <vector>

namespace yuki::language {

class SentenceMaker {
public:
    SentenceMaker() = default;

    bool loadTemplates(const std::string& filepath);
    std::string compose(const std::string& template_id,
                        const std::unordered_map<std::string, std::string>& slots) const;
    size_t templateCount() const;

private:
    std::unordered_map<std::string, std::string> templates_;
};

} // namespace yuki::language
