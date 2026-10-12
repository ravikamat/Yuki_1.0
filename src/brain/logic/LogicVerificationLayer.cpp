#include "LogicVerificationLayer.h"
#include <algorithm>
#include <sstream>

namespace yuki::logic {

LogicVerificationLayer::LogicVerificationLayer(
    std::shared_ptr<yuki::language::GrammarEngine> grammar,
    std::shared_ptr<yuki::causality::CausalGraph> causal,
    std::shared_ptr<SafetyGovernor> safety,
    std::shared_ptr<yuki::policy::CapabilityIntrospector> capability)
    : grammar_(std::move(grammar))
    , causal_(std::move(causal))
    , safety_(std::move(safety))
    , capability_(std::move(capability))
{
}

bool LogicVerificationLayer::verifyGrammar(const std::string& text, VerificationResult& out) const {
    if (!grammar_) return true;

    auto check = grammar_->verify(text);
    if (!check.well_formed) {
        for (const auto& err : check.errors) {
            out.violations.push_back("GrammarError: " + err.message);
        }
        out.corrected_text = check.corrected;
        return false;
    }
    return true;
}

bool LogicVerificationLayer::verifySafety(const std::string& text, VerificationResult& out) const {
    // 1. Check for prompt injection / safety risk patterns
    static const std::vector<std::string> forbidden = {
        "rm -rf /", ":(){ :|:& };:", "drop database", "format c:", "<script>eval("
    };

    for (const auto& f : forbidden) {
        if (text.find(f) != std::string::npos) {
            out.violations.push_back("SafetyViolation: Forbidden destructive pattern detected: " + f);
            out.retry_hint.forbidden_patterns.push_back(f);
            return false;
        }
    }
    return true;
}

bool LogicVerificationLayer::verifyCausal(const std::string& text, VerificationResult& out) const {
    if (!causal_) return true;

    // Check for logical / causal self-contradictions
    if (text.find("because") != std::string::npos && text.find("therefore not") != std::string::npos) {
        out.violations.push_back("CausalViolation: Circular causal contradiction detected.");
        return false;
    }
    return true;
}

bool LogicVerificationLayer::verifyCapability(const std::string& text, VerificationResult& out) const {
    if (!capability_) return true;

    // Ensure claims align with healthy capabilities
    if (text.find("I cannot") != std::string::npos && text.find("unimplemented") != std::string::npos) {
        out.violations.push_back("CapabilityViolation: Self-invalidating capability claim.");
        return false;
    }
    return true;
}

bool LogicVerificationLayer::verifyContract(const std::string& text,
                                            const yuki::language::PromptContract& contract,
                                            VerificationResult& out) const
{
    // 1. Must include
    for (const auto& req : contract.must_include) {
        if (text.find(req) == std::string::npos) {
            out.violations.push_back("ContractViolation: Missing required phrase: " + req);
            out.retry_hint.required_patterns.push_back(req);
        }
    }

    // 2. Must not include
    for (const auto& f : contract.must_not_include) {
        if (text.find(f) != std::string::npos) {
            out.violations.push_back("ContractViolation: Contains forbidden phrase: " + f);
            out.retry_hint.forbidden_patterns.push_back(f);
        }
    }

    // 3. Prefix lock
    if (!contract.prefix_lock.empty()) {
        if (text.rfind(contract.prefix_lock, 0) != 0) {
            out.violations.push_back("ContractViolation: Missing locked prefix: " + contract.prefix_lock);
            out.retry_hint.corrected_prefix = contract.prefix_lock;
        }
    }

    return out.violations.empty();
}

VerificationResult LogicVerificationLayer::verify(const GenerationCandidate& candidate,
                                                  const VerificationContext& ctx) const
{
    VerificationResult res;
    res.corrected_text = candidate.text;
    res.confidence = candidate.confidence;

    bool g_ok = verifyGrammar(candidate.text, res);
    bool s_ok = verifySafety(candidate.text, res);
    bool c_ok = verifyCausal(candidate.text, res);
    bool cap_ok = verifyCapability(candidate.text, res);
    bool con_ok = verifyContract(candidate.text, ctx.contract, res);

    res.passed = g_ok && s_ok && c_ok && cap_ok && con_ok && res.violations.empty();

    if (!res.passed) {
        res.confidence *= 0.5f;
        std::stringstream ss;
        ss << "Logic verification failed with " << res.violations.size() << " violation(s).";
        res.retry_hint.constraint_description = ss.str();
    }

    return res;
}

std::string LogicVerificationLayer::correct(const std::string& text,
                                            const VerificationResult& result) const
{
    if (result.passed) return text;
    if (!result.corrected_text.empty() && result.corrected_text != text) {
        return result.corrected_text;
    }

    std::string fixed = text;
    // Apply prefix lock if specified
    if (!result.retry_hint.corrected_prefix.empty() && fixed.rfind(result.retry_hint.corrected_prefix, 0) != 0) {
        fixed = result.retry_hint.corrected_prefix + fixed;
    }

    // Ensure terminal punctuation
    if (!fixed.empty() && fixed.back() != '.' && fixed.back() != '!' && fixed.back() != '?') {
        fixed += ".";
    }

    return fixed;
}

} // namespace yuki::logic
