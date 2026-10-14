#pragma once
// CognitiveField — The unified continuous cognition substrate
// Integrates FieldScheduler, HPC, Active Inference, and all timescales

#include <memory>
#include <string>
#include <vector>
#include <array>
#include <atomic>
#include "FieldScheduler.h"
#include "CognitiveSnapshot.h"
#include "brain/causality/CausalWorldModel.h"

// Forward declarations for types outside yuki::brain
namespace yuki::memory { class HdcSemanticGraph; }
namespace ync          { class NeuromorphicSimulator; }

namespace yuki {
namespace brain {
namespace inference { class VariationalStateEstimator; class GenerativeModel; }
namespace memory { class CognitiveMemoryFabric; class EpisodicStore; class AwakeConsolidationEngine; }
namespace language { class BrocaTranslator; class WernickeComprehender; class PhonologicalLoop; }
namespace policy { class ActiveInferencePolicyEngine; }
namespace self { class RichSelfModel; }
namespace organism { class ContinuousDriveSystem; }
namespace core { class HierarchicalPredictiveCoding; }
namespace predictive { class LatentWorldModel; }
namespace autonomy { class CuriosityEngine; }

namespace field {

struct CognitiveFieldConfig {
    field::SchedulerConfig scheduler;
    
    // Component enable flags
    bool enable_hpc = true;
    bool enable_active_inference = true;
    bool enable_awake_consolidation = true;
    bool enable_phonological_loop = true;
    bool enable_drives = true;
    bool enable_self_model = true;
    bool enable_ync = true;
    
    // HPC config
    size_t hpc_levels = 3;
    std::vector<std::chrono::milliseconds> hpc_intervals = {
        std::chrono::milliseconds(10),   // L1
        std::chrono::milliseconds(100),  // L2
        std::chrono::milliseconds(1000)  // L3
    };
};

class CognitiveField {
public:
    explicit CognitiveField(const CognitiveFieldConfig& cfg = CognitiveFieldConfig{});
    ~CognitiveField();
    
    // Non-copyable, movable
    CognitiveField(const CognitiveField&) = delete;
    CognitiveField& operator=(const CognitiveField&) = delete;
    CognitiveField(CognitiveField&&) noexcept;
    CognitiveField& operator=(CognitiveField&&) noexcept;
    
    // ─────────────────────────────────────────────────────────────────────────
    // Wiring: Set component pointers (called during BabyMode initialization)
    // ─────────────────────────────────────────────────────────────────────────
    
    void setVSE(inference::VariationalStateEstimator* vse);
    void setGenerativeModel(inference::GenerativeModel* gm);
    void setMemoryFabric(memory::CognitiveMemoryFabric* cmf);
    void setEpisodicStore(memory::EpisodicStore* es);
    void setHdcGraph(::yuki::memory::HdcSemanticGraph* hdc);
    void setBrocaTranslator(language::BrocaTranslator* broca);
    void setWernickeComprehender(language::WernickeComprehender* wernicke);
    void setPhonologicalLoop(language::PhonologicalLoop* pl);
    // void setPolicyEngine(policy::ActiveInferencePolicyEngine* pe);  // Moved to not_in_use
    void setDriveSystem(organism::ContinuousDriveSystem* ds);
    void setSelfModel(self::RichSelfModel* sm);
    void setYNC(::ync::NeuromorphicSimulator* ync);
    
    // Additional cognitive subsystems
    void setHPC(yuki::brain::core::HierarchicalPredictiveCoding* hpc);
    void setAwakeConsolidation(yuki::brain::memory::AwakeConsolidationEngine* ace);
    void setPolicyEngine(yuki::brain::policy::ActiveInferencePolicyEngine* pe);
    
    // Phase 1-2: Latent World Model + Causal + MPC
    void setLatentWorldModel(yuki::brain::predictive::LatentWorldModel* lwm);
    void setCausalWorldModel(yuki::brain::causality::CausalWorldModel* cwm);
    
    // Curiosity Engine
    void setCuriosityEngine(autonomy::CuriosityEngine* ce);
    
    // ─────────────────────────────────────────────────────────────────────────
    // Control
    // ─────────────────────────────────────────────────────────────────────────
    
    void start();
    void stop();
    bool isRunning() const;
    
    // ─────────────────────────────────────────────────────────────────────────
    // Sensory injection (main entry point for user input)
    // ─────────────────────────────────────────────────────────────────────────
    
    // Inject text input (non-blocking)
    bool injectText(const std::string& text, const std::vector<float>& embedding = {});
    
    // Inject text and wait for processing (blocking)
    bool injectTextBlocking(const std::string& text, std::chrono::milliseconds timeout = std::chrono::seconds(5));
    
    // ─────────────────────────────────────────────────────────────────────────
    // Thought retrieval (for TurnCoordinator thin adapter)
    // ─────────────────────────────────────────────────────────────────────────
    
    // Wait for next commit and get thought
    bool waitForThought(std::chrono::milliseconds deadline);
    
    // Get current thought (language-ready)
    std::string getThought() const;
    
    // Get full cognitive snapshot
    CognitiveSnapshot getSnapshot() const;
    
    // ─────────────────────────────────────────────────────────────────────────
    // Direct access to components (for advanced use)
    // ─────────────────────────────────────────────────────────────────────────
    
    FieldScheduler& scheduler() { return scheduler_; }
    const FieldScheduler& scheduler() const { return scheduler_; }
    
    // ─────────────────────────────────────────────────────────────────────────
    // Metrics & monitoring
    // ─────────────────────────────────────────────────────────────────────────
    
    struct FieldMetrics {
        std::array<field::TimescaleStats, 6> timescale_stats;
        double belief_entropy = 0.0;
        double free_energy = 0.0;
        size_t memory_size = 0;
        size_t active_goals = 0;
        std::string dominant_intent;
        float intent_confidence = 0.0f;
    };
    
    FieldMetrics getMetrics() const;

private:
    CognitiveFieldConfig config_;
    FieldScheduler scheduler_;
    std::atomic<bool> running_{false};
    std::atomic<bool> thought_ready_{false};
    std::atomic<bool> new_user_turn_{false};
    std::string current_thought_;
    mutable std::mutex thought_mtx_;
    std::condition_variable thought_cv_;
    
    // Component pointers (not owned)
    inference::VariationalStateEstimator* vse_ = nullptr;
    inference::GenerativeModel* generative_model_ = nullptr;
    memory::CognitiveMemoryFabric* cmf_ = nullptr;
    memory::EpisodicStore* episodic_store_ = nullptr;
    ::yuki::memory::HdcSemanticGraph* hdc_ = nullptr;
    language::BrocaTranslator* broca_ = nullptr;
    language::WernickeComprehender* wernicke_ = nullptr;
    language::PhonologicalLoop* phonological_loop_ = nullptr;
    //     policy::ActiveInferencePolicyEngine* policy_engine_ = nullptr;  // Moved to not_in_use
    organism::ContinuousDriveSystem* drive_system_ = nullptr;
    self::RichSelfModel* self_model_ = nullptr;
    ::ync::NeuromorphicSimulator* ync_ = nullptr;
    
    // Additional cognitive subsystems
    yuki::brain::core::HierarchicalPredictiveCoding* hpc_ = nullptr;
    yuki::brain::memory::AwakeConsolidationEngine* awake_consolidation_ = nullptr;
    yuki::brain::policy::ActiveInferencePolicyEngine* policy_engine_ = nullptr;
    
    // Phase 1-2: Latent World Model + Causal + MPC
    yuki::brain::predictive::LatentWorldModel* latent_world_model_ = nullptr;
    yuki::brain::causality::CausalWorldModel* causal_world_model_ = nullptr;
    
    // Curiosity Engine
    autonomy::CuriosityEngine* curiosity_engine_ = nullptr;
    
    // Internal state
    std::array<float, 24> last_sensory_embedding{};
    std::string last_user_text_;
    
    // Setup callbacks
    void setupCallbacks();
    
    // Internal processing helpers
    void updateVSEBelief(const CognitiveSnapshot& current, CognitiveSnapshot& next);
    void runHPC(const CognitiveSnapshot& current, CognitiveSnapshot& next);
    void runLatentWorldModel(const CognitiveSnapshot& current, CognitiveSnapshot& next);
    void runActiveInference(const CognitiveSnapshot& current, CognitiveSnapshot& next);
    void updateDrives(const CognitiveSnapshot& current, CognitiveSnapshot& next);
    void runAwakeConsolidation(const CognitiveSnapshot& current, CognitiveSnapshot& next);
    void generateThought(const CognitiveSnapshot& next);
};

} // namespace field
} // namespace brain
} // namespace yuki