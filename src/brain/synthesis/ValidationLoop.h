#pragma once
#include <cstdint>
#include <memory>
#include <vector>
#include "SynthesisResult.h"
#include "brain/selftest/SelfTestHarness.h"
#include "brain/security/SecuritySandbox.h"
#include "brain/metacognition/ImprovementGraph.h"

namespace yuki {
namespace synthesis {

class ValidationLoop {
public:
    ValidationLoop(selftest::SelfTestHarness* harness,
                   security::SecuritySandbox* sandbox,
                   metacognition::ImprovementGraph* graph);

    SynthesisResult validate(SynthesisResult result);
    void processQueue();

    SynthesisResult validateForTesting(const std::string& code);
    std::vector<bool> runTestSuite(const std::vector<std::string>& testFiles);

    struct Stats {
        uint32_t total_attempted = 0;
        uint32_t compile_success = 0;
        uint32_t test_success = 0;
        uint32_t integration_success = 0;
        uint32_t rejected_by_sandbox = 0;
    };
    Stats stats() const noexcept { return stats_; }
    void resetStats() noexcept { stats_ = Stats{}; }

private:
    selftest::SelfTestHarness* harness_;
    security::SecuritySandbox* sandbox_;
    metacognition::ImprovementGraph* graph_;
    std::vector<SynthesisResult> queue_;
    Stats stats_;

    bool compileInSandbox(const SynthesisResult& result, selftest::TestResult* out);
    bool runTests(const selftest::TestResult& compile_result, selftest::TestResult* out);
    float measureCompetenceDelta(uint32_t domain);
    void feedbackToGraph(const SynthesisResult& result, bool success);
};

} // namespace synthesis
} // namespace yuki
