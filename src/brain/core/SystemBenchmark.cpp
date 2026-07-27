#include "brain/core/SystemBenchmark.h"
#include "brain/core/Logger.h"

#include <fstream>
#include <sstream>
#include <cstring>
#include <algorithm>

namespace yuki { namespace core {

struct SubsystemBenchRecord {
    std::string name;
    SystemBenchmark::BenchmarkFunction setup;
    SystemBenchmark::BenchmarkFunction benchmark;
    SystemBenchmark::BenchmarkFunction teardown;
    double baselineLatencyMs = 0.0;
    double baselineThroughput = 0.0;
    double baselineMemoryMb = 0.0;
};

class SystemBenchmark::Impl {
public:
    std::unordered_map<std::string, SubsystemBenchRecord> subsystems_;

    Impl() = default;
};

SystemBenchmark::SystemBenchmark() : pImpl(std::make_unique<Impl>()) {
    yuki::core::Logger::instance().log(yuki::core::LogLevel::DEBUG, "SystemBenchmark initialized");
}

SystemBenchmark::~SystemBenchmark() = default;

SystemBenchmark::SystemBenchmark(SystemBenchmark&&) noexcept = default;
SystemBenchmark& SystemBenchmark::operator=(SystemBenchmark&&) noexcept = default;

void SystemBenchmark::registerSubsystem(const std::string& name,
                                        BenchmarkFunction setup,
                                        BenchmarkFunction benchmark,
                                        BenchmarkFunction teardown) {
    SubsystemBenchRecord rec;
    rec.name = name;
    rec.setup = setup;
    rec.benchmark = benchmark;
    rec.teardown = teardown;
    pImpl->subsystems_[name] = rec;
}

void SystemBenchmark::setBaseline(const std::string& name, double latencyMs, double throughput, double memoryMb) {
    auto it = pImpl->subsystems_.find(name);
    if (it != pImpl->subsystems_.end()) {
        it->second.baselineLatencyMs = latencyMs;
        it->second.baselineThroughput = throughput;
        it->second.baselineMemoryMb = memoryMb;
    }
}

bool SystemBenchmark::loadBaselines(const std::string& filepath) {
    std::ifstream ifs(filepath);
    if (!ifs.is_open()) return false;

    std::string line;
    while (std::getline(ifs, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream iss(line);
        std::string name;
        double lat = 0.0, tp = 0.0, mem = 0.0;
        if (iss >> name >> lat >> tp >> mem) {
            setBaseline(name, lat, tp, mem);
        }
    }
    return true;
}

bool SystemBenchmark::saveBaselines(const std::string& filepath) {
    std::ofstream ofs(filepath);
    if (!ofs.is_open()) return false;

    ofs << "# YUKI SystemBenchmark Baselines\n";
    for (const auto& kv : pImpl->subsystems_) {
        ofs << kv.first << " " << kv.second.baselineLatencyMs << " "
            << kv.second.baselineThroughput << " " << kv.second.baselineMemoryMb << "\n";
    }
    return true;
}

BenchmarkResult SystemBenchmark::runSubsystemBenchmark(const std::string& name, size_t iterations) {
    BenchmarkResult res;
    res.subsystem = name;

    auto it = pImpl->subsystems_.find(name);
    if (it == pImpl->subsystems_.end() || !it->second.benchmark) {
        return res;
    }

    const auto& rec = it->second;
    res.baselineLatencyMs = rec.baselineLatencyMs;
    res.baselineThroughput = rec.baselineThroughput;

    if (rec.setup) rec.setup();

    auto tStart = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < iterations; ++i) {
        rec.benchmark();
    }
    auto tEnd = std::chrono::high_resolution_clock::now();

    if (rec.teardown) rec.teardown();

    double totalMs = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
    res.latencyMs = iterations > 0 ? (totalMs / static_cast<double>(iterations)) : 0.0;
    res.throughputOpsPerSec = totalMs > 0.0 ? ((static_cast<double>(iterations) / totalMs) * 1000.0) : 0.0;
    res.memoryPeakMb = 1.0; // Baseline memory estimation

    if (rec.baselineLatencyMs > 0.0) {
        if (res.latencyMs > 1.2 * rec.baselineLatencyMs) {
            res.regressionDetected = true;
            res.regressionSeverity = (res.latencyMs - rec.baselineLatencyMs) / rec.baselineLatencyMs;
        }
    }

    return res;
}

BenchmarkReport SystemBenchmark::runFullBenchmark(size_t iterations) {
    BenchmarkReport report;
    report.timestamp = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    double sumReg = 0.0;
    for (const auto& kv : pImpl->subsystems_) {
        auto result = runSubsystemBenchmark(kv.first, iterations);
        report.results.push_back(result);
        if (result.regressionDetected) {
            report.regressionsFound++;
            sumReg += result.regressionSeverity;
        }
    }

    if (!report.results.empty()) {
        report.overallRegressionScore = sumReg / static_cast<double>(report.results.size());
    }

    return report;
}

std::vector<BenchmarkResult> SystemBenchmark::checkRegression(const BenchmarkReport& current) {
    std::vector<BenchmarkResult> regressions;
    for (const auto& res : current.results) {
        if (res.regressionDetected) {
            regressions.push_back(res);
        }
    }
    return regressions;
}

std::vector<uint8_t> SystemBenchmark::serialize() const {
    std::vector<uint8_t> buf;
    uint32_t magic = 0x53424E43; // 'SBNC'

    buf.resize(4);
    std::memcpy(buf.data(), &magic, 4);

    uint64_t hash = 0xcbf29ce484222325ULL;
    for (uint8_t byte : buf) {
        hash ^= byte;
        hash *= 0x100000001b3ULL;
    }
    size_t off = buf.size();
    buf.resize(off + 8);
    std::memcpy(buf.data() + off, &hash, 8);

    return buf;
}

bool SystemBenchmark::deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < 12) return false;

    size_t payload_len = data.size() - 8;
    uint64_t expected_hash = 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < payload_len; ++i) {
        expected_hash ^= data[i];
        expected_hash *= 0x100000001b3ULL;
    }

    uint64_t actual_hash = 0;
    std::memcpy(&actual_hash, data.data() + payload_len, 8);
    if (expected_hash != actual_hash) return false;

    uint32_t magic = 0;
    std::memcpy(&magic, data.data(), 4);
    if (magic != 0x53424E43) return false;

    return true;
}

}} // namespace yuki::core
