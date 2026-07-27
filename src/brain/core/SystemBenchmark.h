#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include <memory>
#include <chrono>
#include <functional>
#include <unordered_map>

namespace yuki { namespace core {

struct BenchmarkResult {
    std::string subsystem;
    double latencyMs = 0.0;
    double throughputOpsPerSec = 0.0;
    double memoryPeakMb = 0.0;
    double baselineLatencyMs = 0.0;
    double baselineThroughput = 0.0;
    bool regressionDetected = false;
    double regressionSeverity = 0.0;
};

struct BenchmarkReport {
    uint64_t timestamp = 0;
    std::vector<BenchmarkResult> results;
    double overallRegressionScore = 0.0;
    size_t regressionsFound = 0;
};

class SystemBenchmark {
public:
    SystemBenchmark();
    ~SystemBenchmark();
    SystemBenchmark(const SystemBenchmark&) = delete;
    SystemBenchmark& operator=(const SystemBenchmark&) = delete;
    SystemBenchmark(SystemBenchmark&&) noexcept;
    SystemBenchmark& operator=(SystemBenchmark&&) noexcept;

    using BenchmarkFunction = std::function<void()>;
    void registerSubsystem(const std::string& name,
                           BenchmarkFunction setup,
                           BenchmarkFunction benchmark,
                           BenchmarkFunction teardown);

    void setBaseline(const std::string& name, double latencyMs, double throughput, double memoryMb);
    bool loadBaselines(const std::string& filepath);
    bool saveBaselines(const std::string& filepath);

    BenchmarkReport runFullBenchmark(size_t iterations = 100);
    BenchmarkResult runSubsystemBenchmark(const std::string& name, size_t iterations = 100);

    std::vector<BenchmarkResult> checkRegression(const BenchmarkReport& current);

    // Binary serialization: magic = 0x53424E43 ('SBNC')
    std::vector<uint8_t> serialize() const;
    bool deserialize(const std::vector<uint8_t>& data);

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

}} // namespace yuki::core
