#include "brain/infra/CognitiveConfig.h"
#include "brain/infra/CognitiveConfigLoader.h"

namespace yuki {
namespace brain {
namespace infra {

CognitiveConfig CognitiveConfig::loadFromJson(const std::string& json_path) {
    CognitiveConfig config;
    config.vse = CognitiveConfigLoader::loadVSEConfig(json_path);
    return config;
}

} // namespace infra
} // namespace brain
} // namespace yuki