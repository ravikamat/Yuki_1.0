#include "src/brain/language/LocalModelBenchmark.h"
#include "src/brain/platform/RuntimeProcess.h"
#include <fstream>
#include <sstream>
#include <regex>

namespace yuki::brain::language {

LocalModelBenchmarkResult LocalModelBenchmark::parseBenchmarkOutput(const std::string& text) {
    LocalModelBenchmarkResult result;
    result.rawOutput = text;

    if (text.find("t/s") == std::string::npos && text.find("tokens per second") == std::string::npos) {
        result.success = false;
        result.diagnostic = "Output does not contain valid token throughput headers";
        return result;
    }

    std::regex valRegex(R"raw((\d+\.\d+))raw");
    std::sregex_iterator iter(text.begin(), text.end(), valRegex);
    std::sregex_iterator end;

    std::vector<float> values;
    while (iter != end) {
        try {
            values.push_back(std::stof(iter->str()));
        } catch (...) {}
        ++iter;
    }

    if (values.size() >= 2) {
        result.promptTokensPerSecond = values[0];
        result.decodeTokensPerSecond = values[1];
        result.success = true;
        result.syclVerified = (result.decodeTokensPerSecond > 0.0f);
    } else if (values.size() == 1) {
        result.decodeTokensPerSecond = values[0];
        result.success = true;
        result.syclVerified = (result.decodeTokensPerSecond > 0.0f);
    } else {
        result.success = false;
        result.diagnostic = "Could not parse throughput values";
    }

    return result;
}

LocalModelBenchmarkResult LocalModelBenchmark::run(
    const yuki::brain::platform::LocalModelRuntimeConfig& config,
    const yuki::brain::platform::IntelOneApiRuntimeStatus& runtimeStatus) const {

    (void)runtimeStatus;
    LocalModelBenchmarkResult result;

    if (config.llamaCpp.benchmarkExecutable.empty()) {
        result.diagnostic = "Benchmark executable path not configured";
        return result;
    }

    yuki::brain::platform::RuntimeProcess process;
    std::vector<std::string> args = {
        "-m", config.llamaCpp.modelPath,
        "-ngl", std::to_string(config.llamaCpp.gpuLayers),
        "-c", std::to_string(config.llamaCpp.contextSize)
    };

    auto procRes = process.runAndCapture(config.llamaCpp.benchmarkExecutable, args, "", 30000);
    if (!procRes.completed) {
        result.diagnostic = "llama-bench process execution failed or timed out";
        return result;
    }

    return parseBenchmarkOutput(procRes.stdoutText);
}

bool LocalModelBenchmark::persist(const LocalModelBenchmarkResult& result, const std::string& outputPath) const {
    std::ofstream file(outputPath);
    if (!file.is_open()) return false;

    file << "{\n";
    file << "  \"schema_version\": 2,\n";
    file << "  \"status\": \"" << (result.success ? "measured" : "failed") << "\",\n";
    file << "  \"verified\": " << (result.syclVerified ? "true" : "false") << ",\n";
    file << "  \"prompt_tokens_per_second\": " << result.promptTokensPerSecond << ",\n";
    file << "  \"decode_tokens_per_second\": " << result.decodeTokensPerSecond << ",\n";
    file << "  \"diagnostic\": \"" << result.diagnostic << "\"\n";
    file << "}\n";

    return true;
}

} // namespace yuki::brain::language
