#include "src/brain/language/LocalModelBenchmark.h"
#include "src/brain/platform/RuntimeProcess.h"
#include <fstream>
#include <sstream>
#include <regex>
#include <chrono>
#include <iostream>

namespace yuki::brain::language {

LocalModelBenchmarkResult LocalModelBenchmark::run(
    const yuki::brain::platform::LocalModelRuntimeConfig& config,
    const yuki::brain::platform::IntelOneApiRuntimeStatus& runtimeStatus) const {

    LocalModelBenchmarkResult result;
    result.modelId = config.llamaCpp.modelPath;

    if (config.llamaCpp.benchmarkExecutable.empty()) {
        result.diagnostic = "Benchmark executable path is empty";
        return result;
    }

    std::vector<std::string> args = {
        "-m", config.llamaCpp.modelPath,
        "-n", "128",
        "-ngl", std::to_string(config.llamaCpp.gpuLayers)
    };

    yuki::brain::platform::RuntimeProcess proc;
    auto procRes = proc.runAndCapture(config.llamaCpp.benchmarkExecutable, args, "", 60000);

    result.rawOutput = procRes.stdoutText + "\n" + procRes.stderrText;

    if (!procRes.completed || procRes.exitCode != 0) {
        result.diagnostic = "llama-bench execution failed or timed out. Output: " + result.rawOutput;
        return result;
    }

    result.success = true;

    // Parsing logic for llama-bench output table / metrics
    // Typical llama-bench output formats:
    // | model | size | params | backend | ngl | test | t/s |
    // or lines like: "prompt processing: 125.4 tok/s", "eval: 32.1 tok/s"
    std::string text = result.rawOutput;

    std::regex promptRegex(R"((?:pp|prompt|prompt processing)\s*[:=]?\s*([\d\.]+)\s*(?:t/s|tok/s|tokens/s)?)", std::regex::icase);
    std::regex evalRegex(R"((?:tg|eval|generation|decode)\s*[:=]?\s*([\d\.]+)\s*(?:t/s|tok/s|tokens/s)?)", std::regex::icase);

    std::smatch match;
    if (std::regex_search(text, match, promptRegex) && match.size() > 1) {
        try { result.promptTokensPerSecond = std::stof(match[1].str()); } catch (...) {}
    }
    if (std::regex_search(text, match, evalRegex) && match.size() > 1) {
        try { result.decodeTokensPerSecond = std::stof(match[1].str()); } catch (...) {}
    }

    // Fallback parser if regex misses: parse any decimal numbers near tok/s or t/s
    if (result.decodeTokensPerSecond <= 0.0f) {
        std::regex numTokRegex(R"(([\d\.]+)\s*(?:t/s|tok/s))", std::regex::icase);
        auto words_begin = std::sregex_iterator(text.begin(), text.end(), numTokRegex);
        auto words_end = std::sregex_iterator();
        std::vector<float> found;
        for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
            try { found.push_back(std::stof((*i)[1].str())); } catch (...) {}
        }
        if (found.size() >= 2) {
            result.promptTokensPerSecond = found[0];
            result.decodeTokensPerSecond = found[1];
        } else if (found.size() == 1) {
            result.decodeTokensPerSecond = found[0];
        }
    }

    // Verification rule: SYCL probe must have detected an Intel GPU and decode t/s must be positive and >= min threshold
    if (runtimeStatus.intelGpuDetected && result.decodeTokensPerSecond >= config.modelPolicy.minimumDecodeTokensPerSecond) {
        result.syclVerified = true;
        result.deviceName = runtimeStatus.detectedDevices.empty() ? "Intel SYCL GPU" : runtimeStatus.detectedDevices.front();
        result.diagnostic = "SYCL benchmark verified successfully (" + std::to_string(result.decodeTokensPerSecond) + " decode tok/s)";
    } else if (!runtimeStatus.intelGpuDetected) {
        result.diagnostic = "Benchmark completed but Intel SYCL GPU was not detected by probe";
    } else {
        result.diagnostic = "Benchmark decode throughput (" + std::to_string(result.decodeTokensPerSecond) + " tok/s) below minimum threshold (" + std::to_string(config.modelPolicy.minimumDecodeTokensPerSecond) + " tok/s)";
    }

    return result;
}

bool LocalModelBenchmark::persist(const LocalModelBenchmarkResult& result,
                                  const std::string& outputPath) const {
    std::ofstream file(outputPath);
    if (!file.is_open()) return false;

    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    file << "{\n";
    file << "  \"schema_version\": 1,\n";
    file << "  \"backend\": \"llama_cpp_sycl\",\n";
    file << "  \"status\": \"" << (result.syclVerified ? "verified" : (result.success ? "measured" : "failed")) << "\",\n";
    file << "  \"model_id\": \"" << result.modelId << "\",\n";
    file << "  \"device_name\": \"" << result.deviceName << "\",\n";
    file << "  \"prompt_tokens_per_second\": " << result.promptTokensPerSecond << ",\n";
    file << "  \"decode_tokens_per_second\": " << result.decodeTokensPerSecond << ",\n";
    file << "  \"measured_at_unix_ms\": " << now << ",\n";
    file << "  \"verified\": " << (result.syclVerified ? "true" : "false") << "\n";
    file << "}\n";

    return true;
}

} // namespace yuki::brain::language
