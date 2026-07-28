#include "src/brain/platform/LocalModelRuntimeConfig.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <cctype>

namespace yuki::brain::platform {

static std::string trim(const std::string& str) {
    auto start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

LocalModelRuntimeConfig LocalModelRuntimeConfigLoader::load(const std::string& path) {
    LocalModelRuntimeConfig config;
    std::ifstream file(path);
    if (!file.is_open()) {
        // Return default configuration if file is missing
        return config;
    }

    std::string currentSection;
    std::string line;

    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == ';' || line[0] == '#') continue;

        if (line.front() == '[' && line.back() == ']') {
            currentSection = trim(line.substr(1, line.size() - 2));
            continue;
        }

        auto eqPos = line.find('=');
        if (eqPos == std::string::npos) continue;

        std::string key = trim(line.substr(0, eqPos));
        std::string val = trim(line.substr(eqPos + 1));

        if (currentSection == "oneapi") {
            if (key == "enabled") config.oneApi.enabled = (val == "true" || val == "1");
            else if (key == "oneapi_environment_script") config.oneApi.environmentScript = val;
            else if (key == "sycl_runtime_probe") config.oneApi.syclRuntimeProbe = val;
        } else if (currentSection == "llama_cpp") {
            if (key == "server_executable") config.llamaCpp.serverExecutable = val;
            else if (key == "bench_executable") config.llamaCpp.benchmarkExecutable = val;
            else if (key == "model_path") config.llamaCpp.modelPath = val;
            else if (key == "host") config.llamaCpp.host = val;
            else if (key == "port") config.llamaCpp.port = static_cast<uint16_t>(std::stoi(val));
            else if (key == "context_size") config.llamaCpp.contextSize = std::stoi(val);
            else if (key == "gpu_layers") config.llamaCpp.gpuLayers = std::stoi(val);
            else if (key == "parallel_slots") config.llamaCpp.parallelSlots = std::stoi(val);
            else if (key == "startup_timeout_ms") config.llamaCpp.startupTimeoutMs = std::stoi(val);
            else if (key == "request_timeout_ms") config.llamaCpp.requestTimeoutMs = std::stoi(val);
            else if (key == "health_timeout_ms") config.llamaCpp.healthTimeoutMs = std::stoi(val);
        } else if (currentSection == "resource_policy") {
            if (key == "foreground_cpu_reserve_logical_cores") config.resourcePolicy.foregroundCpuReserveLogicalCores = std::stoi(val);
            else if (key == "minimum_available_ram_mb") config.resourcePolicy.minimumAvailableRamMb = std::stoull(val);
            else if (key == "minimum_available_ram_mb_for_gpu_model") config.resourcePolicy.minimumAvailableRamMbForGpuModel = std::stoull(val);
            else if (key == "maximum_background_cpu_percent") config.resourcePolicy.maximumBackgroundCpuPercent = std::stof(val);
            else if (key == "maximum_background_ram_percent") config.resourcePolicy.maximumBackgroundRamPercent = std::stof(val);
            else if (key == "maximum_background_gpu_percent") config.resourcePolicy.maximumBackgroundGpuPercent = std::stof(val);
            else if (key == "idle_seconds_before_background_work") config.resourcePolicy.idleSecondsBeforeBackgroundWork = std::stoull(val);
        } else if (currentSection == "model_policy") {
            if (key == "require_sycl_benchmark") config.modelPolicy.requireSyclBenchmark = (val == "true" || val == "1");
            else if (key == "minimum_decode_tokens_per_second") config.modelPolicy.minimumDecodeTokensPerSecond = std::stof(val);
            else if (key == "maximum_health_latency_ms") config.modelPolicy.maximumHealthLatencyMs = std::stoi(val);
            else if (key == "maximum_model_ram_mb") config.modelPolicy.maximumModelRamMb = std::stoull(val);
            else if (key == "allow_cpu_local_fallback") config.modelPolicy.allowCpuLocalFallback = (val == "true" || val == "1");
            else if (key == "allow_external_fallback") config.modelPolicy.allowExternalFallback = (val == "true" || val == "1");
        }
    }

    // Validation logic per Section 5.2
    if (config.llamaCpp.port == 0) {
        throw std::runtime_error("local-model runtime port must be non-zero");
    }
    if (config.oneApi.enabled && config.llamaCpp.serverExecutable.empty()) {
        throw std::runtime_error("oneAPI runtime enabled but llama server executable is empty");
    }
    if (config.oneApi.enabled && config.llamaCpp.modelPath.empty()) {
        throw std::runtime_error("oneAPI runtime enabled but model path is empty");
    }
    if (config.resourcePolicy.maximumBackgroundCpuPercent <= 0.0f ||
        config.resourcePolicy.maximumBackgroundCpuPercent > 100.0f) {
        throw std::runtime_error("invalid background CPU percentage");
    }
    if (config.resourcePolicy.maximumBackgroundRamPercent <= 0.0f ||
        config.resourcePolicy.maximumBackgroundRamPercent > 100.0f) {
        throw std::runtime_error("invalid background RAM percentage");
    }
    if (config.llamaCpp.startupTimeoutMs < 0 || config.llamaCpp.requestTimeoutMs < 0) {
        throw std::runtime_error("negative timeouts invalid");
    }
    if (config.llamaCpp.contextSize < 0) {
        throw std::runtime_error("negative context size invalid");
    }

    return config;
}

} // namespace yuki::brain::platform
