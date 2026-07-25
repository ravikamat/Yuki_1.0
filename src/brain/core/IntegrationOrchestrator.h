#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include <memory>
#include <functional>
#include <unordered_map>
#include <unordered_set>

namespace yuki { namespace core {

enum class ModuleHealth { OK, DEGRADED, FAILED, UNKNOWN };

struct ModuleStatus {
    std::string name;
    ModuleHealth health = ModuleHealth::UNKNOWN;
    double latencyMs = 0.0;
    double memoryMb = 0.0;
    std::string lastError;
    uint64_t lastHeartbeat = 0;
};

struct HealthReport {
    double overallScore = 1.0;
    size_t okCount = 0;
    size_t degradedCount = 0;
    size_t failedCount = 0;
    std::vector<ModuleStatus> modules;
    std::vector<std::string> cyclesDetected;
    std::vector<std::string> warnings;
};

class IntegrationOrchestrator {
public:
    IntegrationOrchestrator();
    ~IntegrationOrchestrator();
    IntegrationOrchestrator(const IntegrationOrchestrator&) = delete;
    IntegrationOrchestrator& operator=(const IntegrationOrchestrator&) = delete;
    IntegrationOrchestrator(IntegrationOrchestrator&&) noexcept;
    IntegrationOrchestrator& operator=(IntegrationOrchestrator&&) noexcept;

    using HealthCallback = std::function<ModuleStatus()>;

    void registerModule(const std::string& name,
                        const std::vector<std::string>& dependencies,
                        HealthCallback healthCheck);

    void unregisterModule(const std::string& name);

    bool validateCoherence();
    HealthReport getSystemHealth();
    std::vector<std::string> detectCycles();
    bool hasCycles() const;

    bool validatePath(const std::vector<std::string>& modulePath);
    std::vector<std::string> identifyFaultyModules();

    // Binary serialization: magic = 0x494E544F ('INTO')
    std::vector<uint8_t> serialize() const;
    bool deserialize(const std::vector<uint8_t>& data);

    size_t getModuleCount() const;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

}} // namespace yuki::core
