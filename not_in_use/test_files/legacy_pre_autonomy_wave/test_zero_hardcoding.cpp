#include "brain/core/ConfigManager.h"
#include <iostream>
#include <cassert>
#include <unordered_map>
#include <unordered_set>
#include <vector>

int main() {
    std::cout << "[TEST] ConfigManager zero-hardcoding verification..." << std::endl;

    auto& cfg = yuki::ConfigManager::instance();

    // 1. Templates
    std::unordered_map<std::string, std::string> tmpls;
    cfg.loadTemplates("data/identity_templates.txt", tmpls);
    assert(!tmpls.empty());
    assert(tmpls.count("yuki_identity") > 0);
    assert(tmpls["yuki_identity"].find("RahulRavi") != std::string::npos);

    // 2. Keywords / Patterns
    std::unordered_set<std::string> kwSet;
    cfg.loadKeywords("data/self_detection_patterns.txt", kwSet);
    assert(!kwSet.empty());

    // 3. Float config
    std::unordered_map<std::string, float> floats;
    cfg.loadFloatConfig("data/llm_config.txt", floats);
    assert(floats.count("temperature") > 0);
    assert(floats["temperature"] == 0.7f);

    // 4. VSE Features
    std::unordered_map<std::string, std::vector<float>> vse;
    cfg.loadVseFeatures("data/vse_training_features.txt", vse);
    assert(vse.count("TUTORIAL") > 0);
    assert(vse["TUTORIAL"].size() == 12);

    // 5. Bootstrap Knowledge
    std::vector<std::tuple<std::string, std::string, float>> kb;
    cfg.loadBootstrapKnowledge("data/bootstrap_knowledge.txt", kb);
    assert(!kb.empty());

    std::cout << "[TEST] ConfigManager verification PASSED! All data files loaded cleanly." << std::endl;
    return 0;
}
