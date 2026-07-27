#include "brain/core/IntegrationOrchestrator.h"
#include "brain/core/Logger.h"

#include <algorithm>
#include <cstring>
#include <chrono>

namespace yuki { namespace core {

struct ModuleRecord {
    std::string name;
    std::vector<std::string> dependencies;
    IntegrationOrchestrator::HealthCallback callback;
};

class IntegrationOrchestrator::Impl {
public:
    std::unordered_map<std::string, ModuleRecord> modules_;

    Impl() = default;

    std::vector<std::string> detectCycles() const {
        std::vector<std::string> cycles;
        std::unordered_map<std::string, int> color; // 0: white, 1: gray, 2: black

        for (const auto& kv : modules_) {
            color[kv.first] = 0;
        }

        std::function<bool(const std::string&, std::vector<std::string>&)> dfs =
            [&](const std::string& u, std::vector<std::string>& path) -> bool {
            color[u] = 1;
            path.push_back(u);

            auto it = modules_.find(u);
            if (it != modules_.end()) {
                for (const auto& v : it->second.dependencies) {
                    if (modules_.find(v) != modules_.end()) {
                        if (color[v] == 1) {
                            // Cycle detected
                            std::string cycleStr = "";
                            for (const auto& node : path) cycleStr += node + "->";
                            cycleStr += v;
                            cycles.push_back(cycleStr);
                            return true;
                        } else if (color[v] == 0) {
                            if (dfs(v, path)) return true;
                        }
                    }
                }
            }

            color[u] = 2;
            path.pop_back();
            return false;
        };

        for (const auto& kv : modules_) {
            if (color[kv.first] == 0) {
                std::vector<std::string> path;
                dfs(kv.first, path);
            }
        }

        return cycles;
    }
};

IntegrationOrchestrator::IntegrationOrchestrator() : pImpl(std::make_unique<Impl>()) {
    yuki::core::Logger::instance().log(yuki::core::LogLevel::DEBUG, "IntegrationOrchestrator initialized");
}

IntegrationOrchestrator::~IntegrationOrchestrator() = default;

IntegrationOrchestrator::IntegrationOrchestrator(IntegrationOrchestrator&&) noexcept = default;
IntegrationOrchestrator& IntegrationOrchestrator::operator=(IntegrationOrchestrator&&) noexcept = default;

void IntegrationOrchestrator::registerModule(const std::string& name,
                                              const std::vector<std::string>& dependencies,
                                              HealthCallback healthCheck) {
    ModuleRecord rec;
    rec.name = name;
    rec.dependencies = dependencies;
    rec.callback = healthCheck;
    pImpl->modules_[name] = rec;
}

void IntegrationOrchestrator::unregisterModule(const std::string& name) {
    pImpl->modules_.erase(name);
}

bool IntegrationOrchestrator::hasCycles() const {
    return !pImpl->detectCycles().empty();
}

std::vector<std::string> IntegrationOrchestrator::detectCycles() {
    return pImpl->detectCycles();
}

bool IntegrationOrchestrator::validateCoherence() {
    if (hasCycles()) return false;

    // Verify all declared dependencies exist
    for (const auto& kv : pImpl->modules_) {
        for (const auto& dep : kv.second.dependencies) {
            if (pImpl->modules_.find(dep) == pImpl->modules_.end()) {
                yuki::core::Logger::instance().log(yuki::core::LogLevel::WARN,
                    "Module " + kv.first + " depends on unregistered module " + dep);

            }
        }
    }
    return true;
}

HealthReport IntegrationOrchestrator::getSystemHealth() {
    HealthReport report;
    report.cyclesDetected = pImpl->detectCycles();

    if (pImpl->modules_.empty()) {
        report.overallScore = 1.0;
        return report;
    }

    double totalScore = 0.0;
    uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    for (const auto& kv : pImpl->modules_) {
        ModuleStatus status;
        status.name = kv.first;
        status.lastHeartbeat = now;

        if (kv.second.callback) {
            status = kv.second.callback();
        } else {
            status.health = ModuleHealth::OK;
        }

        report.modules.push_back(status);

        switch (status.health) {
            case ModuleHealth::OK:
                report.okCount++;
                totalScore += 1.0;
                break;
            case ModuleHealth::DEGRADED:
                report.degradedCount++;
                totalScore += 0.5;
                report.warnings.push_back("Module degraded: " + status.name);
                break;
            case ModuleHealth::FAILED:
                report.failedCount++;
                totalScore += 0.0;
                report.warnings.push_back("Module failed: " + status.name);
                break;
            case ModuleHealth::UNKNOWN:
            default:
                totalScore += 0.75;
                break;
        }
    }

    report.overallScore = totalScore / static_cast<double>(pImpl->modules_.size());
    return report;
}

bool IntegrationOrchestrator::validatePath(const std::vector<std::string>& modulePath) {
    if (modulePath.empty()) return true;
    for (const auto& name : modulePath) {
        if (pImpl->modules_.find(name) == pImpl->modules_.end()) return false;
    }
    return true;
}

std::vector<std::string> IntegrationOrchestrator::identifyFaultyModules() {
    std::vector<std::string> faulty;
    auto report = getSystemHealth();
    for (const auto& mod : report.modules) {
        if (mod.health == ModuleHealth::FAILED || mod.health == ModuleHealth::DEGRADED) {
            faulty.push_back(mod.name);
        }
    }
    return faulty;
}

size_t IntegrationOrchestrator::getModuleCount() const {
    return pImpl->modules_.size();
}

std::vector<uint8_t> IntegrationOrchestrator::serialize() const {
    std::vector<uint8_t> buf;
    uint32_t magic = 0x494E544F; // 'INTO'
    uint32_t count = static_cast<uint32_t>(pImpl->modules_.size());

    buf.resize(8);
    std::memcpy(buf.data(), &magic, 4);
    std::memcpy(buf.data() + 4, &count, 4);

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

bool IntegrationOrchestrator::deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < 16) return false;

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
    if (magic != 0x494E544F) return false;

    return true;
}

}} // namespace yuki::core
