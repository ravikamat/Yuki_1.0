#pragma once

#include <string>
#include <vector>
#include "src/brain/language/GenerationBackend.h"

namespace yuki::brain::learning {

struct BenchmarkTask {
    std::string taskId;
    std::string category;
    std::string prompt;
    std::string expectedKeyword;
};

struct BenchmarkReport {
    std::size_t totalTasks{0};
    std::size_t passedTasks{0};
    float passRate{0.0f};
    float avgLatencyMs{0.0f};
    bool meetsThreshold{false};
};

class BenchmarkSuite {
public:
    BenchmarkSuite() = default;

    bool loadSeedDataset(const std::string& seedPath);
    BenchmarkReport evaluateBackend(yuki::brain::language::IGenerationBackend& backend);

private:
    std::vector<BenchmarkTask> tasks_;
};

} // namespace yuki::brain::learning
