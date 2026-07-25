#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <optional>

namespace yuki::capability {

struct CapabilityProfile {
    std::string tool_id;
    std::vector<std::string> inputs;
    std::vector<std::string> outputs;
    float avg_duration_ms{0.0f};
    float avg_ram_mb{0.0f};
    float avg_cpu_percent{0.0f};
    float base_risk{0.0f};
    float required_competence{0.0f};
    std::vector<std::string> platform_tags;
    bool produces_artifacts{false};
    bool is_destructive{false};

    std::vector<uint8_t> serialize() const;
    static std::optional<CapabilityProfile> deserialize(const std::vector<uint8_t>& data);
};

} // namespace yuki::capability
