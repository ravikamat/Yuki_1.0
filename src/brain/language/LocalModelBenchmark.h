#pragma once

#include "src/brain/platform/LocalModelRuntimeConfig.h"
#include "src/brain/platform/IntelOneApiRuntime.h"
#include <string>

namespace yuki::brain::language {

struct LocalModelBenchmarkResult {
    bool success{false};
    bool syclVerified{false};
    std::string modelId;
    std::string deviceName;
    float promptTokensPerSecond{0.0f};
    float decodeTokensPerSecond{0.0f};
    std::string rawOutput;
    std::string diagnostic;
};

class LocalModelBenchmark {
public:
    LocalModelBenchmarkResult run(
        const yuki::brain::platform::LocalModelRuntimeConfig& config,
        const yuki::brain::platform::IntelOneApiRuntimeStatus& runtimeStatus) const;

    bool persist(const LocalModelBenchmarkResult& result,
                 const std::string& outputPath) const;
};

} // namespace yuki::brain::language
