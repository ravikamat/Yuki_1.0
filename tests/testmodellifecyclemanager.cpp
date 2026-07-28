#include <iostream>
#include <cassert>
#include "src/brain/language/ModelLifecycleManager.h"

int main() {
    using yuki::brain::language::ModelLifecycleManager;
    using yuki::brain::language::LocalModelInfo;

    ModelLifecycleManager manager;
    LocalModelInfo m1;
    m1.modelId = "model_qwen2_0.5b";
    m1.family = "qwen2";
    m1.filePath = "data/models/qwen2.gguf";

    manager.registerModel(m1);
    if (!manager.validateChecksum("model_qwen2_0.5b")) {
        std::cerr << "[FAIL] testmodellifecyclemanager: checksum validation failed\n";
        return 1;
    }

    std::cout << "[PASS] testmodellifecyclemanager\n";
    return 0;
}
