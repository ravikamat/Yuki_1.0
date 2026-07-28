#pragma once

#include <string>

namespace yuki::autonomy {

class DynamicPromptDirector {
public:
    std::string buildSystemPrompt(const std::string& mode,
                                  const std::string& ownerIntent,
                                  const std::string& backend,
                                  float risk,
                                  float budget) const;
};

} // namespace yuki::autonomy
