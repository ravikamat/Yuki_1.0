#pragma once

#include <cstdint>
#include <string>

namespace yuki::brain::platform {

struct OneApiRuntimeConfig {
    bool enabled{false};
    std::string environmentScript;
    std::string syclRuntimeProbe;
};

struct LlamaCppRuntimeConfig {
    std::string serverExecutable;
    std::string benchmarkExecutable;
    std::string modelPath;
    std::string host{"127.0.0.1"};
    uint16_t port{18080};
    int contextSize{4096};
    int gpuLayers{-1};
    int parallelSlots{1};
    int startupTimeoutMs{30000};
    int requestTimeoutMs{120000};
    int healthTimeoutMs{3000};
};

struct ResourcePolicyConfig {
    int foregroundCpuReserveLogicalCores{2};
    uint64_t minimumAvailableRamMb{8192};
    uint64_t minimumAvailableRamMbForGpuModel{10240};
    float maximumBackgroundCpuPercent{55.0f};
    float maximumBackgroundRamPercent{35.0f};
    float maximumBackgroundGpuPercent{70.0f};
    uint64_t idleSecondsBeforeBackgroundWork{600};
};

struct ModelPolicyConfig {
    bool requireSyclBenchmark{true};
    float minimumDecodeTokensPerSecond{4.0f};
    int maximumHealthLatencyMs{3000};
    uint64_t maximumModelRamMb{8192};
    bool allowCpuLocalFallback{true};
    bool allowExternalFallback{true};
};

struct LocalModelRuntimeConfig {
    OneApiRuntimeConfig oneApi;
    LlamaCppRuntimeConfig llamaCpp;
    ResourcePolicyConfig resourcePolicy;
    ModelPolicyConfig modelPolicy;
};

class LocalModelRuntimeConfigLoader {
public:
    static LocalModelRuntimeConfig load(const std::string& path);
};

} // namespace yuki::brain::platform
