// IdleLearningPipeline.h — Stage 3: Sequential child-learning while idle
// Drive → Gate → ResearchPlanner → Tools → Verification → Consolidation

#pragma once
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <atomic>
#include <chrono>
#include <mutex>
#include <queue>

#include "brain/organism/DriveSystem.h"
// Forward declaration
namespace yuki::organism { class OrganismController; }
#include "brain/research/core/ResearchPlanner.h"
#include "brain/research/core/ToolRegistry.h"
#include "brain/research/tools/SandboxExecuteTool.h"
#include "brain/memory/CognitiveMemoryFabric.h"
#include "brain/memory/HdcSemanticGraph.h"
#include "brain/memory/SemanticBinder.h"
#include "brain/memory/EpisodicStore.h"
#include "brain/ync/DevelopmentalEngine.h"

namespace yuki {
namespace organism {

struct IdleLearningConfig {
    // Idle detection
    float idle_threshold_seconds = 30.0f;        // No user activity before idle
    float deep_idle_seconds = 300.0f;            // Deep learning mode
    
    // Learning loop
    float learning_cycle_interval_ms = 1000.0f;  // Cycle interval
    size_t max_concurrent_tasks = 3;             // Max parallel learning tasks
    
    // Developmental gates
    bool require_competence_test = true;         // Stage N+1 unlocks after N passes
    float competence_threshold = 0.75f;          // Competence test pass threshold
    
    // Resource limits
    float max_learning_power_pct = 0.3f;         // Max 30% power for learning
    float max_learning_compute_pct = 0.4f;       // Max 40% compute for learning
};

struct LearningTask {
    std::string id;
    std::string topic;
    std::string domain;
    float priority = 0.0f;
    std::chrono::steady_clock::time_point created;
    std::chrono::steady_clock::time_point started;
    std::chrono::steady_clock::time_point completed;
    bool success = false;
    std::string result_summary;
    std::vector<std::string> tools_used;
};

struct CompetenceTest {
    std::string domain;
    std::vector<std::string> test_queries;
    float pass_threshold = 0.75f;
    bool passed = false;
    std::chrono::steady_clock::time_point tested_at;
};

class IdleLearningPipeline {
public:
    explicit IdleLearningPipeline(const IdleLearningConfig& cfg = {});
    ~IdleLearningPipeline();

    // Initialize with required subsystems
    bool initialize(
        yuki::organism::OrganismController* organism,
        yuki::research::ResearchPlanner* planner,
        yuki::research::ToolRegistry* tools,
        yuki::research::SandboxExecuteTool* sandbox,
        yuki::brain::memory::CognitiveMemoryFabric* cmf,
        yuki::memory::HdcSemanticGraph* semantic_graph,
        ync::DevelopmentalEngine* dev_engine,
        yuki::brain::memory::SemanticBinder* semantic_binder
    );

    // Start/stop the learning pipeline
    void start();
    void stop();
    bool is_running() const noexcept { return running_.load(); }

    // Called periodically by OrganismController tick
    void tick(float dt_seconds);

    // Called when user activity detected (resets idle timer)
    void on_user_activity();

    // Force a learning cycle (for testing)
    void force_learning_cycle();

    // Get current learning tasks
    std::vector<LearningTask> get_active_tasks();
    std::vector<LearningTask> get_completed_tasks();

    // Competence testing
    bool run_competence_test(const std::string& domain);
    std::vector<CompetenceTest> get_competence_tests();

    // Semantic binding
    void bind_new_concept(const std::string& name, 
                          const std::vector<std::string>& related_episodes,
                          const std::string& valence,
                          const std::vector<std::string>& affordances);

    struct Stats {
        std::atomic<uint64_t> total_learning_cycles{0};
        std::atomic<uint64_t> successful_tasks{0};
        std::atomic<uint64_t> failed_tasks{0};
        std::atomic<uint64_t> competence_tests_passed{0};
        std::atomic<uint64_t> competence_tests_failed{0};
        std::atomic<float> avg_task_duration_ms{0.0f};
        std::atomic<float> total_learning_time_seconds{0.0f};
    };
    Stats get_stats();

private:
    IdleLearningConfig config_;
    
    // Dependencies (non-owning)
    yuki::organism::OrganismController* organism_ = nullptr;
    yuki::research::ResearchPlanner* planner_ = nullptr;
    yuki::research::ToolRegistry* tools_ = nullptr;
    yuki::research::SandboxExecuteTool* sandbox_ = nullptr;
    yuki::brain::memory::CognitiveMemoryFabric* cmf_ = nullptr;
    yuki::memory::HdcSemanticGraph* semantic_graph_ = nullptr;
    ync::DevelopmentalEngine* dev_engine_ = nullptr;
    yuki::brain::memory::SemanticBinder* semantic_binder_ = nullptr;

    // State
    std::atomic<bool> running_{false};
    std::thread worker_thread_;
    mutable std::mutex tasks_mutex_;
    std::queue<LearningTask> pending_tasks_;
    std::vector<LearningTask> active_tasks_;
    std::vector<LearningTask> completed_tasks_;
    std::vector<CompetenceTest> competence_tests_;
    std::atomic<float> idle_time_seconds_{0.0f};
    std::atomic<bool> user_active_{false};
    std::atomic<bool> in_deep_idle_{false};
    
    Stats stats_;
    
    // Worker thread
    void worker_loop();
    
    // Learning cycle steps
    void select_learning_task();
    void execute_learning_task(LearningTask& task);
    void verify_task_result(LearningTask& task);
    void consolidate_learning(const LearningTask& task);
    
    // Competence gating
    bool check_competence_gate(const std::string& domain);
    void unlock_next_stage(const std::string& domain);
    
    // Semantic binding
    void digest_into_semantic_graph(const LearningTask& task);
    
    // Task generation from drive deficits
    std::vector<LearningTask> generate_tasks_from_drives();
    
    // Resource checking
    bool check_learning_budget();
};

} // namespace organism
} // namespace yuki