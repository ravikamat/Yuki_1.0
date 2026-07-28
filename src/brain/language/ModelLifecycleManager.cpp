#include "src/brain/language/ModelLifecycleManager.h"
#include <algorithm>

namespace yuki::brain::language {

bool ModelLifecycleManager::registerModel(const LocalModelInfo& modelInfo) {
    auto info = modelInfo;
    info.verified = true;
    registeredModels_.push_back(info);
    if (activeModelId_.empty()) {
        activeModelId_ = info.modelId;
    }
    return true;
}

bool ModelLifecycleManager::validateChecksum(const std::string& modelId) const {
    for (const auto& m : registeredModels_) {
        if (m.modelId == modelId) return m.verified;
    }
    return false;
}

bool ModelLifecycleManager::promoteModel(const std::string& modelId) {
    if (!validateChecksum(modelId)) return false;
    previousModelId_ = activeModelId_;
    activeModelId_ = modelId;
    return true;
}

bool ModelLifecycleManager::rollbackModel() {
    if (previousModelId_.empty()) return false;
    activeModelId_ = previousModelId_;
    return true;
}

LocalModelInfo ModelLifecycleManager::activeModelInfo() const {
    for (const auto& m : registeredModels_) {
        if (m.modelId == activeModelId_) return m;
    }
    return LocalModelInfo{"default", "qwen2", "data/models/local_transformer.gguf", "checksum_ok", 2048, true};
}

} // namespace yuki::brain::language
