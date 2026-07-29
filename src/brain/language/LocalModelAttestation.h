#pragma once

#include "src/brain/platform/LocalModelRuntimeConfig.h"
#include "src/brain/platform/IntelOneApiRuntime.h"
#include <cstdint>
#include <string>

namespace yuki::brain::language {

struct LocalModelAttestationRecord {
    uint32_t schemaVersion{2};
    std::string backend{"llama_cpp_sycl"};
    std::string status{"not_measured"};
    bool verified{false};
    uint64_t measuredAtUnixMs{0};
    uint64_t expiresAtUnixMs{0};
    std::string modelPath;
    std::string modelFingerprintSha256;
    std::string llamaServerPath;
    std::string llamaServerFingerprintSha256;
    std::string llamaBenchPath;
    std::string llamaVersion;
    std::string oneApiRuntimeVersion;
    std::string deviceLuid;
    std::string deviceName;
    std::string driverVersion;
    int gpuLayers{0};
    int contextSize{0};
    float promptTokensPerSecond{0.0f};
    float decodeTokensPerSecond{0.0f};
    std::string rawOutputHashSha256;
    std::string diagnostic;

    bool isExpired(uint64_t nowUnixMs) const;
    bool matchesRuntime(const std::string& currentModelFingerprint,
                        const std::string& currentServerFingerprint,
                        const std::string& currentDeviceLuid) const;
};

class LocalModelAttestation {
public:
    static std::string calculateFileFingerprint(const std::string& filePath);
    static LocalModelAttestationRecord load(const std::string& path);
    static bool save(const LocalModelAttestationRecord& record, const std::string& path);
};

} // namespace yuki::brain::language
