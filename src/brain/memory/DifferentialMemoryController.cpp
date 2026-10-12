#include "brain/memory/DifferentialMemoryController.h"
#include "brain/core/Logger.h"
#include <algorithm>
#include <chrono>
#include <random>
#include "brain/core/RandomEngine.h"

namespace yuki {
namespace brain {
namespace memory {



DifferentialMemoryController::DifferentialMemoryController(const DifferentialMemoryControllerConfig& cfg)
    : config_(cfg) {
    yuki::core::Logger::instance().log(yuki::core::LogLevel::INFO, "DifferentialMemoryController", "Initialized");
}

std::pair<DMCDecision, DecisionToken> DifferentialMemoryController::evaluate(const std::array<float, 24>& posterior, const std::array<float, 24>& context) {
    DecisionToken token;
    token.id = static_cast<uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
    token.timestamp = std::chrono::steady_clock::now();
    
    DMCDecision action = DMCDecision::DEFER;
    float confidence = 0.5f;
    
    // Simplified decision logic
    float max_posterior = 0.0f;
    for (float v : posterior) {
        if (v > max_posterior) max_posterior = v;
    }
    
    if (max_posterior > config_.promotion_threshold) {
        action = DMCDecision::PROMOTE;
        confidence = max_posterior;
    } else if (max_posterior < 0.3f) {
        action = DMCDecision::REJECT;
        confidence = 1.0f - max_posterior;
    } else {
        action = DMCDecision::DEFER;
        confidence = 0.5f;
    }
    
    token.id = static_cast<uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
    token.timestamp = std::chrono::steady_clock::now();
    token.confidence = confidence;
    
    return {action, token};
}

void DifferentialMemoryController::recordOutcome(const DecisionToken& token, bool success, float precision) {
    // Record outcome for learning
}

} // namespace memory
} // namespace brain
} // namespace yuki