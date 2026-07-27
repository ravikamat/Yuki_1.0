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

// ══════════════════════════════════════════════════════════════════════════════
// ValidationLoop Implementation
// ══════════════════════════════════════════════════════════════════════════════

ValidationLoop::ValidationLoop(selftest::SelfTestHarness* harness,
                                security::SecuritySandbox* sandbox,
                                metacognition::ImprovementGraph* graph)
    : harness_(harness), sandbox_(sandbox), graph_(graph) {}

SynthesisResult ValidationLoop::validate(SynthesisResult result) {
    ++stats_.total_attempted;

    if (!harness_ || !sandbox_) {
        result.status = SynthesisResult::Status::FAILED_COMPILATION;
        result.error_code = 10;
        return result;
    }

    auto h_verdict = sandbox_->validateWrite(result.output_header_path);
    auto s_verdict = sandbox_->validateWrite(result.output_source_path);
    if (!h_verdict.allowed() || !s_verdict.allowed()) {
        result.status = SynthesisResult::Status::REJECTED_BY_SANDBOX;
        ++stats_.rejected_by_sandbox;
        result.error_code = 11;
        feedbackToGraph(result, false);
        return result;
    }

    selftest::TestResult compile_result;
    bool compiled = compileInSandbox(result, &compile_result);
    result.compiled = compiled;
    result.compile_exit_code = compile_result.exitCode;
    result.compile_stdout = compile_result.stdoutCapture;
    result.compile_stderr = compile_result.stderrCapture;

    if (!compiled) {
        result.status = SynthesisResult::Status::FAILED_COMPILATION;
        result.error_code = 12;
        feedbackToGraph(result, false);
        return result;
    }

    ++stats_.compile_success;

    selftest::TestResult test_result;
    bool tests_pass = runTests(compile_result, &test_result);
    result.tests_passed = tests_pass;
    result.test_exit_code = test_result.exitCode;

    if (!tests_pass) {
        result.status = SynthesisResult::Status::FAILED_TESTS;
        result.error_code = 13;
        feedbackToGraph(result, false);
        return result;
    }

    ++stats_.test_success;
    result.status = SynthesisResult::Status::INTEGRATED;
    ++stats_.integration_success;
    feedbackToGraph(result, true);

    return result;
}

void ValidationLoop::processQueue() {
    for (auto& r : queue_) {
        validate(r);
    }
    queue_.clear();
}

bool ValidationLoop::compileInSandbox(const SynthesisResult& result, selftest::TestResult* out) {
    if (!harness_) return false;

    selftest::TestConfig config;
    config.sourceCode = result.generated_source;
    *out = harness_->runTest(config);
    return out->compiled;
}

bool ValidationLoop::runTests(const selftest::TestResult& compile_result, selftest::TestResult* out) {
    *out = compile_result;
    return compile_result.passed;
}

float ValidationLoop::measureCompetenceDelta(uint32_t /*domain*/) {
    return 0.1f;
}

void ValidationLoop::feedbackToGraph(const SynthesisResult& result, bool success) {
    if (!graph_) return;
    graph_->feedback(metacognition::SymptomCode::PRECISION_TOO_HIGH,
                    metacognition::ExperimentType::REWIRE_FEATURES,
                    success);
}

SynthesisResult ValidationLoop::validateForTesting(const std::string& code) {
    SynthesisResult res;
    res.generated_source = code;
    res.output_header_path = "temp_test.h";
    res.output_source_path = "temp_test.cpp";
    return validate(res);
}

std::vector<bool> ValidationLoop::runTestSuite(const std::vector<std::string>& testFiles) {
    std::vector<bool> results;
    for (const auto& file : testFiles) {
        auto val = validateForTesting(file);
        results.push_back(val.status == SynthesisResult::Status::INTEGRATED);
    }
    return results;
}

} // namespace synthesis
} // namespace yuki
