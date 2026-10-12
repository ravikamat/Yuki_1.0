#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <chrono>
#include <memory>
#include <mutex>

namespace yuki {
namespace brain {
namespace memory {

struct DifferentialMemoryControllerConfig {
    float promotion_threshold = 0.7f;
    float decay_rate = 0.01f;
    size_t max_decisions = 10000;
};

struct DecisionToken {
    uint64_t id = 0;
    std::string description;
    float confidence = 0.0f;
    std::chrono::steady_clock::time_point timestamp;
    std::unordered_map<std::string, float> features;
};

enum class DMCDecision {
    CONSOLIDATE = 0,
    PROMOTE = 1,
    REJECT = 2,
    DEFER = 3
};

struct DMCDecisionResult {
    DMCDecision action;
    DecisionToken token;
    float confidence = 0.0f;
    std::string rationale;
};

class DifferentialMemoryController {
public:
    explicit DifferentialMemoryController(const DifferentialMemoryControllerConfig& cfg = DifferentialMemoryControllerConfig{});
    ~DifferentialMemoryController() = default;
    
    void setConfig(const DifferentialMemoryControllerConfig& cfg) { config_ = cfg; }
    
    std::pair<DMCDecision, DecisionToken> evaluate(
        const std::array<float, 24>& posterior,
        const std::array<float, 24>& context);
    
    void recordOutcome(const DecisionToken& token, bool success, float precision);
    
    const DifferentialMemoryControllerConfig& config() const noexcept { return config_; }
    
private:
    DifferentialMemoryControllerConfig config_;
    std::vector<DecisionToken> decision_history_;
    std::mutex mutex_;
};

} // namespace memory
} // namespace brain
} // namespace yuki