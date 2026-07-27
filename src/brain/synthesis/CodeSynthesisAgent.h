#pragma once
#include <cstdint>
#include <memory>
#include <vector>
#include <string>
#include "SynthesisSpec.h"
#include "SynthesisResult.h"
#include "brain/metacognition/HypothesisConsumer.h"
#include "brain/security/SecuritySandbox.h"
#include "brain/selftest/SelfTestHarness.h"
#include "brain/metacognition/ImprovementGraph.h"

namespace yuki {
namespace synthesis {

// ══════════════════════════════════════════════════════════════════════════════
// ValidationLoop Class
// ══════════════════════════════════════════════════════════════════════════════

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

// ══════════════════════════════════════════════════════════════════════════════
// CodeSynthesisAgent Class
// ══════════════════════════════════════════════════════════════════════════════

class CodeSynthesisAgent : public metacognition::HypothesisConsumer {
public:
    CodeSynthesisAgent(security::SecuritySandbox* sandbox,
                       selftest::SelfTestHarness* harness);

    bool consume(const metacognition::ActionableHypothesis& hypothesis) override;
    size_t pendingCount() const override;
    size_t completedCount() const override;

    SynthesisResult synthesize(const SynthesisSpec& spec);

    SynthesisResult generateToolInterface(const std::string& toolName);
    bool canGenerateTool(const std::string& toolName) const;

    const std::vector<SynthesisResult>& completedResults() const noexcept {
        return completed_results_;
    }
    void clearCompleted() noexcept { completed_results_.clear(); completed_count_ = 0; }

private:
    security::SecuritySandbox* sandbox_;
    selftest::SelfTestHarness* harness_;
    std::vector<SynthesisSpec> pending_;
    std::vector<SynthesisResult> completed_results_;
    size_t completed_count_ = 0;

    std::string resolveModulePath(uint32_t module_id) const;
    SynthesisResult generateCode(const SynthesisSpec& spec);
    SynthesisResult generateRewireFeature(const SynthesisSpec& spec);
    SynthesisResult generateAdjustParameter(const SynthesisSpec& spec);
    SynthesisResult generateAddFunction(const SynthesisSpec& spec);
    bool validatePaths(const SynthesisResult& result);
};

} // namespace synthesis
} // namespace yuki
