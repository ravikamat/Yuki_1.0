#pragma once

#include <string>
#include <optional>
#include "CognitiveConfig.h"
#include <nlohmann/json.hpp>

namespace yuki {
namespace brain {
namespace infra {

struct CognitiveConfigLoader {
    static CognitiveConfig::VSEConfig loadVSEConfig(const std::string& json_path);
    static ConfigSchema::HPCConfigT loadHPCConfig(const std::string& json_path);
    static ConfigSchema::YNCConfigT loadYNCConfig(const std::string& json_path);
    static ConfigSchema::DriveConfigT loadDriveConfig(const std::string& json_path);
    static ConfigSchema::SelfModelConfigT loadSelfModelConfig(const std::string& json_path);
    static ConfigSchema::ToMConfigT loadToMConfig(const std::string& json_path);
    static ConfigSchema::MetacognitionConfigT loadMetacognitionConfig(const std::string& json_path);
    static ConfigSchema::IdentityConfigT loadIdentityConfig(const std::string& json_path);
    static ConfigSchema::PersonaConfigT loadPersonaConfig(const std::string& json_path);
    static ConfigSchema::PolicyConfigT loadPolicyConfig(const std::string& json_path);
    static ConfigSchema::ResourcePolicyConfigT loadResourcePolicyConfig(const std::string& json_path);
    static ConfigSchema::MarketplaceConfigT loadMarketplaceConfig(const std::string& json_path);
    static ConfigSchema::UpgradeConfigT loadUpgradeConfig(const std::string& json_path);
    static ConfigSchema::KnowledgeSyncConfigT loadKnowledgeSyncConfig(const std::string& json_path);
    static ConfigSchema::SkillAcquisitionConfigT loadSkillAcquisitionConfig(const std::string& json_path);
    static ConfigSchema::UpgradeSchedulerConfigT loadUpgradeSchedulerConfig(const std::string& json_path);
    static ConfigSchema::CheckpointConfigT loadCheckpointConfig(const std::string& json_path);
    static ConfigSchema::EventLogConfigT loadEventLogConfig(const std::string& json_path);
    static ConfigSchema::MetricsConfigT loadMetricsConfig(const std::string& json_path);
    static ConfigSchema::SensorimotorConfigT loadSensorimotorConfig(const std::string& json_path);
    static ConfigSchema::GUIAutomationConfigT loadGUIAutomationConfig(const std::string& json_path);
    static ConfigSchema::WorldModelConfigT loadWorldModelConfig(const std::string& json_path);
    static ConfigSchema::SimulatorConfigT loadSimulatorConfig(const std::string& json_path);

    static std::optional<nlohmann::json> loadJsonFile(const std::string& path);
    static nlohmann::json getSection(const nlohmann::json& root, const std::string& section);
};

} // namespace infra
} // namespace brain
} // namespace yuki