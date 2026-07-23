#include "ValidationLoop.h"

namespace yuki {
namespace synthesis {

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
