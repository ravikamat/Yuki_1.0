#pragma once
#include "brain/language/GrammarEngine.h"
#include "brain/causality/CausalGraph.h"
#include "brain/policy/CapabilityIntrospector.h"
#include "brain/SafetyGovernor.h"
#include "brain/language/PromptContract.h"
#include "brain/language/GenerationBackend.h"
#include <string>
#include <vector>
#include <memory>

namespace yuki::logic {

struct GenerationCandidate {
    std::string text;
    yuki::brain::language::BackendKind backend;
    float confidence = 1.0f;
};

struct VerificationContext {
    yuki::brain::language::GenerationRequest request;
    yuki::language::PromptContract contract;
};

struct VerificationResult {
    bool passed = false;
    std::string corrected_text;
    std::vector<std::string> violations;
    std::vector<yuki::language::PromptConstraint> required_constraints;
    float confidence = 0.0f;

    struct RetryHint {
        std::string constraint_description;
        std::vector<std::string> forbidden_patterns;
        std::vector<std::string> required_patterns;
        std::string corrected_prefix;
    } retry_hint;
};

class LogicVerificationLayer {
public:
    LogicVerificationLayer(
        std::shared_ptr<yuki::language::GrammarEngine> grammar = nullptr,
        std::shared_ptr<yuki::causality::CausalGraph> causal = nullptr,
        std::shared_ptr<SafetyGovernor> safety = nullptr,
        std::shared_ptr<yuki::policy::CapabilityIntrospector> capability = nullptr);

    ~LogicVerificationLayer() = default;

    VerificationResult verify(const GenerationCandidate& candidate,
                              const VerificationContext& ctx) const;

    std::string correct(const std::string& text,
                        const VerificationResult& result) const;

private:
    std::shared_ptr<yuki::language::GrammarEngine> grammar_;
    std::shared_ptr<yuki::causality::CausalGraph> causal_;
    std::shared_ptr<SafetyGovernor> safety_;
    std::shared_ptr<yuki::policy::CapabilityIntrospector> capability_;

    bool verifyGrammar(const std::string& text, VerificationResult& out) const;
    bool verifySafety(const std::string& text, VerificationResult& out) const;
    bool verifyCausal(const std::string& text, VerificationResult& out) const;
    bool verifyCapability(const std::string& text, VerificationResult& out) const;
    bool verifyContract(const std::string& text, const yuki::language::PromptContract& contract, VerificationResult& out) const;
};

} // namespace yuki::logic
