#pragma once

#include "src/brain/platform/DeviceProfile.h"
#include "src/brain/platform/LocalModelRuntimeConfig.h"
#include <atomic>
#include <memory>
#include <string>
#include <cstdint>

namespace yuki::brain::system {

struct BackgroundWorkLease {
    bool permitted{false};
    int workerLimit{0};
    uint64_t expiresAtUnixMs{0};
    std::shared_ptr<std::atomic_bool> cancellationRequested = std::make_shared<std::atomic_bool>(false);
    std::string reason;

    bool isCancelled() const {
        return cancellationRequested && cancellationRequested->load();
    }
    void cancel() {
        if (cancellationRequested) {
            cancellationRequested->store(true);
        }
    }
};

enum class BackgroundJobKind {
    CORPUS_EXTRACTION,
    SELF_PLAY,
    REPLAY_TRAINING,
    MODEL_BENCHMARK
};

class BackgroundWorkGovernor {
public:
    static BackgroundWorkLease evaluateLease(
        const yuki::platform::DeviceProfile& device,
        const platform::ResourcePolicyConfig& policy,
        bool isUserIdle,
        uint64_t userIdleSeconds,
        bool watchdogOk,
        BackgroundJobKind jobKind);

    static bool evaluate(
        const yuki::platform::DeviceProfile& device,
        const platform::ResourcePolicyConfig& policy,
        bool isUserIdle,
        bool watchdogOk);
};

} // namespace yuki::brain::system
