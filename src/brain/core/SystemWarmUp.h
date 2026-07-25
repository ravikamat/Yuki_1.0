#pragma once
#include <chrono>
#include <vector>
#include <memory>

namespace yuki {
    class TurnCoordinator;
    namespace memory { class MemoryFabric; }
    namespace research { class ToolRegistry; }
    namespace security { class SecuritySandbox; }
}

namespace yuki::core {

struct WarmUpMetrics {
    std::chrono::milliseconds total_duration{0};
    std::chrono::milliseconds thread_pool_duration{0};
    std::chrono::milliseconds sqlite_duration{0};
    std::chrono::milliseconds model_cache_duration{0};
    bool success{false};
};

class SystemWarmUp {
public:
    SystemWarmUp() = default;

    // Idempotent: safe to call multiple times; skips already-warmed subsystems.
    WarmUpMetrics execute(
        TurnCoordinator* coordinator,
        memory::MemoryFabric* memory,
        research::ToolRegistry* tools,
        security::SecuritySandbox* sandbox
    );

    bool isWarmed() const { return warmed_; }

private:
    bool warmed_{false};

    void warmThreadPools();
    void warmSQLite(memory::MemoryFabric* memory);
    void warmPerceptionModels(TurnCoordinator* coordinator);
    void warmSecurityCache(security::SecuritySandbox* sandbox);
};

} // namespace yuki::core
