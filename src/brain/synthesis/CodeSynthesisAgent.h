#pragma once
#include <cstdint>
#include <memory>
#include <vector>
#include "SynthesisSpec.h"
#include "SynthesisResult.h"
#include "brain/metacognition/HypothesisConsumer.h"
#include "brain/security/SecuritySandbox.h"
#include "brain/selftest/SelfTestHarness.h"

namespace yuki {
namespace synthesis {

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
