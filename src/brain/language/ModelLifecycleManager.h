#pragma once

#include <string>
#include <vector>
#include <memory>
#include "src/brain/language/GenerationBackend.h"

namespace yuki::brain::language {

struct LocalModelInfo {
    std::string modelId;
    std::string family;
    std::string filePath;
    std::string checksum;
    int contextLength{2048};
    bool verified{false};
};

class ModelLifecycleManager {
public:
    ModelLifecycleManager() = default;

    bool registerModel(const LocalModelInfo& modelInfo);
    bool validateChecksum(const std::string& modelId) const;
    bool promoteModel(const std::string& modelId);
    bool rollbackModel();
    std::string activeModelId() const { return activeModelId_; }
    LocalModelInfo activeModelInfo() const;

private:
    std::vector<LocalModelInfo> registeredModels_;
    std::string activeModelId_;
    std::string previousModelId_;
};

} // namespace yuki::brain::language
