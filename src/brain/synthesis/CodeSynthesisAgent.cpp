#include "CodeSynthesisAgent.h"
#include <sstream>
#include <iomanip>

namespace yuki {
namespace synthesis {

CodeSynthesisAgent::CodeSynthesisAgent(security::SecuritySandbox* sandbox,
                                        selftest::SelfTestHarness* harness)
    : sandbox_(sandbox), harness_(harness) {}

bool CodeSynthesisAgent::consume(const metacognition::ActionableHypothesis& hypothesis) {
    if (!sandbox_ || !harness_) return false;

    SynthesisSpec spec;
    spec.source_hypothesis = hypothesis;
    spec.target_module_id = hypothesis.target_module_id;

    switch (hypothesis.experiment) {
        case metacognition::ExperimentType::REWIRE_FEATURES:
            spec.mod_type = SynthesisSpec::ModificationType::REWIRE_FEATURE;
            spec.feature_index = 0;
            break;
        case metacognition::ExperimentType::ADJUST_LR:
            spec.mod_type = SynthesisSpec::ModificationType::ADJUST_PARAMETER;
            spec.parameter_name = "LEARNING_RATE";
            spec.parameter_value = 0.15f;
            break;

        case metacognition::ExperimentType::EXPAND_TRAINING:
            spec.mod_type = SynthesisSpec::ModificationType::ADD_FUNCTION;
            break;
        default:
            spec.mod_type = SynthesisSpec::ModificationType::ADD_FUNCTION;
            break;
    }

    spec.validation.must_compile = true;
    spec.validation.must_pass_tests = true;
    spec.validation.min_competence_delta = hypothesis.expected_competence_delta;
    spec.validation.target_domain = hypothesis.target_domain;

    pending_.push_back(spec);
    return true;
}

size_t CodeSynthesisAgent::pendingCount() const {
    return pending_.size();
}

size_t CodeSynthesisAgent::completedCount() const {
    return completed_count_;
}

SynthesisResult CodeSynthesisAgent::synthesize(const SynthesisSpec& spec) {
    SynthesisResult result = generateCode(spec);

    if (result.status == SynthesisResult::Status::GENERATED) {
        if (!validatePaths(result)) {
            result.status = SynthesisResult::Status::REJECTED_BY_SANDBOX;
            result.error_code = 2;
            completed_results_.push_back(result);
            ++completed_count_;
            return result;
        }
        result.status = SynthesisResult::Status::COMPILED;
    }

    completed_results_.push_back(result);
    ++completed_count_;
    return result;
}

std::string CodeSynthesisAgent::resolveModulePath(uint32_t module_id) const {
    switch (module_id) {
        case 1: return "inference/PrecisionPredictor";
        case 2: return "inference/VariationalStateEstimator";
        case 3: return "policy/PolicySelector";
        case 4: return "metacognition/MetacognitionEngine";
        case 5: return "predictive/predictive_turn_engine";
        default: return "unknown";
    }
}

SynthesisResult CodeSynthesisAgent::generateCode(const SynthesisSpec& spec) {
    switch (spec.mod_type) {
        case SynthesisSpec::ModificationType::REWIRE_FEATURE:
            return generateRewireFeature(spec);
        case SynthesisSpec::ModificationType::ADJUST_PARAMETER:
            return generateAdjustParameter(spec);
        case SynthesisSpec::ModificationType::ADD_FUNCTION:
            return generateAddFunction(spec);
        default:
            SynthesisResult r;
            r.status = SynthesisResult::Status::FAILED_GENERATION;
            r.error_code = 3;
            return r;
    }
}

SynthesisResult CodeSynthesisAgent::generateRewireFeature(const SynthesisSpec& spec) {
    SynthesisResult result;
    result.status = SynthesisResult::Status::GENERATED;

    std::string module_path = resolveModulePath(spec.target_module_id);
    result.output_header_path = module_path + ".h";
    result.output_source_path = module_path + ".cpp";

    std::ostringstream header_oss;
    header_oss << "// Auto-generated: REWIRE_FEATURE for module " << module_path << "\n";
    header_oss << "// Target feature index: " << spec.feature_index << "\n";
    header_oss << "#pragma once\n";
    header_oss << "// Feature wiring stub -- implementation required\n";
    result.generated_header = header_oss.str();

    std::ostringstream source_oss;
    source_oss << "// Auto-generated: REWIRE_FEATURE implementation\n";
    source_oss << "#include \"" << module_path << ".h\"\n";
    source_oss << "// TODO: Implement feature " << spec.feature_index << " wiring\n";
    result.generated_source = source_oss.str();

    return result;
}

SynthesisResult CodeSynthesisAgent::generateAdjustParameter(const SynthesisSpec& spec) {
    SynthesisResult result;
    result.status = SynthesisResult::Status::GENERATED;

    std::string module_path = resolveModulePath(spec.target_module_id);
    result.output_header_path = module_path + ".h";
    result.output_source_path = module_path + ".cpp";

    std::ostringstream source_oss;
    source_oss << "// Auto-generated: ADJUST_PARAMETER\n";
    source_oss << "// Parameter: " << spec.parameter_name << " = " << std::fixed
               << std::setprecision(6) << spec.parameter_value << "\n";
    result.generated_source = source_oss.str();

    return result;
}

SynthesisResult CodeSynthesisAgent::generateAddFunction(const SynthesisSpec& spec) {
    SynthesisResult result;
    result.status = SynthesisResult::Status::GENERATED;

    std::string module_path = resolveModulePath(spec.target_module_id);
    result.output_header_path = module_path + ".h";
    result.output_source_path = module_path + ".cpp";

    std::ostringstream header_oss;
    header_oss << "// Auto-generated: ADD_FUNCTION\n";
    header_oss << "void new_function_" << spec.source_hypothesis.trigger_audit_id
               << "();\n";
    result.generated_header = header_oss.str();

    std::ostringstream source_oss;
    source_oss << "// Auto-generated: ADD_FUNCTION implementation\n";
    source_oss << "void new_function_" << spec.source_hypothesis.trigger_audit_id
               << "() { /* TODO */ }\n";
    result.generated_source = source_oss.str();

    return result;
}

bool CodeSynthesisAgent::validatePaths(const SynthesisResult& result) {
    if (!sandbox_) return false;
    auto h = sandbox_->validateWrite(result.output_header_path);
    auto s = sandbox_->validateWrite(result.output_source_path);
    return h.allowed() && s.allowed();
}

SynthesisResult CodeSynthesisAgent::generateToolInterface(const std::string& toolName) {
    SynthesisSpec spec;
    spec.mod_type = SynthesisSpec::ModificationType::ADD_FUNCTION;
    spec.target_module_name = toolName;
    return synthesize(spec);
}

bool CodeSynthesisAgent::canGenerateTool(const std::string& toolName) const {
    return !toolName.empty();
}

} // namespace synthesis
} // namespace yuki
