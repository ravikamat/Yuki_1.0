#pragma once
// CuriosityEngine — Learning-progress driven curiosity for autonomous exploration
// Computes curiosity signals from prediction error reduction (learning progress)
// across intent, engagement, urgency, and surprise domains.

#include <array>
#include <deque>
#include <string>
#include <chrono>
#include <vector>

namespace yuki {
namespace brain {
namespace field { struct CognitiveSnapshot; }
namespace organism { class ContinuousDriveSystem; }

namespace autonomy {

struct CuriosityConfig {
    static constexpr int NUM_DOMAINS = 13;  // 8 intent + 3 engagement + 2 urgency + 1 surprise
    
    float history_window_seconds = 300.0f;  // 5 min history window
    float novelty_bonus = 0.3f;             // Bonus for unexplored domains
    float curiosity_gain = 1.0f;            // Global curiosity scaling
    float competence_modulation = 0.5f;     // How much competence reduces curiosity
    float directed_explore_gain = 2.0f;     // Gain for directed exploration bonus
    float min_slope_significance = 0.05f;   // Minimum learning progress to trigger exploration
    float alpha = 0.1f;                     // EMA rate for competence estimation
};

struct DomainCuriosityState {
    float last_pe = 0.5f;
    float competence_estimate = 0.5f;
    float learning_progress = 0.0f;
    float curiosity_signal = 0.0f;
    int visit_count = 0;
    std::chrono::steady_clock::time_point last_visit;
    std::deque<std::pair<std::chrono::steady_clock::time_point, float>> pe_history;
};

struct CuriosityOutput {
    std::array<float, CuriosityConfig::NUM_DOMAINS> curiosity_signals{};
    int target_domain = -1;
    float exploration_bonus = 0.0f;
    std::string rationale;
};

class CuriosityEngine {
public:
    explicit CuriosityEngine(const CuriosityConfig& cfg = CuriosityConfig{});
    
    // Main step: compute curiosity signals from cognitive snapshot and drives
    CuriosityOutput step(const field::CognitiveSnapshot& snap,
                         const organism::ContinuousDriveSystem* drives = nullptr);
    
    // Record prediction error for a domain (call after each inference step)
    void recordPredictionError(int domain, float pe, bool was_novel = false);
    
    // Record outcome for competence estimation
    void recordOutcome(int domain, bool success);
    
    // Get full exploration target info
    CuriosityOutput getExplorationTargetFull() const;
    
    // Get curiosity signal for specific domain
    float getCuriositySignal(int domain) const;
    
    // Get most curious domain index
    int getMostCuriousDomain() const;
    
    // Check if exploration should be triggered
    bool shouldExplore() const;
    
    // Get exploration target domain
    int getExplorationTarget() const;

private:
    CuriosityConfig config_;
    std::array<DomainCuriosityState, CuriosityConfig::NUM_DOMAINS> domains_;
    int last_target_domain_ = -1;
    std::array<float, CuriosityConfig::NUM_DOMAINS> last_curiosity_signals_{};
    
    float computeLearningProgress(int domain) const;
    float modulateByCompetence(float raw_curiosity, float competence) const;
    int selectExplorationTarget() const;
};

} // namespace autonomy
} // namespace brain
} // namespace yuki