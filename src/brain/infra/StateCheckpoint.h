#pragma once
// StateCheckpoint — Full state checkpoint/resume for all cognitive components
// VSE belief, HPC levels, YNC weights, drives, self-model, identity — every N min + shutdown

#include <vector>
#include <string>
#include <unordered_map>
#include <chrono>
#include <memory>
#include <variant>
#include <fstream>
#include <thread>
#include "brain/field/CognitiveSnapshot.h"
#include "brain/inference/VariationalStateEstimator.h"
#include "brain/core/HierarchicalPredictiveCoding.h"
#include "brain/organism/ContinuousDriveSystem.h"
#include "brain/self/RichSelfModel.h"
#include "brain/self/IdentityCore.h"
#include "brain/self/ActiveToM.h"
#include "brain/self/MetacognitiveMonitor.h"
// #include "brain/self/CompetenceTracker.h"  // deferred
// #include "brain/autonomy/GoalGenerator.h"
// #include "brain/autonomy/CuriosityEngine.h"
// #include "brain/autonomy/CurriculumBuilder.h"  // deferred
// #include "brain/autonomy/SkillAcquisitionPipeline.h"  // deferred
// #include "brain/autonomy/ToolDiscovery.h"  // deferred
// #include "brain/autonomy/KnowledgeSync.h"  // deferred
// #include "brain/autonomy/UpgradeScheduler.h"  // deferred
// #include "brain/policy/ActiveInferencePolicyEngine.h"
// #include "brain/reasoning/BeliefRevisionEngine.h"  // deferred
// #include "brain/reasoning/StructureLearner.h"  // deferred
// #include "brain/reasoning/UnifiedReasoningEngine.h"  // deferred
// #include "brain/memory/AwakeConsolidationEngine.h"  // deferred
// #include "brain/economy/TaskMarketplace.h"  // deferred
// #include "brain/economy/ResourceAwarePolicy.h"  // deferred
// #include "brain/embodiment/SensorimotorInterface.h"  // deferred
// #include "brain/embodiment/GUIAutomation.h"  // deferred
// #include "brain/world/WorldModel.h"  // deferred
// #include "brain/world/EnvironmentSimulator.h"  // deferred

// Forward declarations for deferred components
namespace yuki {
namespace brain {
namespace ync { class NeuromorphicSimulator; }
namespace self { class CompetenceTracker; }
namespace autonomy { class CurriculumBuilder; class SkillAcquisitionPipeline; class ToolDiscovery; class KnowledgeSync; class UpgradeScheduler; }
namespace reasoning { class BeliefRevisionEngine; class StructureLearner; class UnifiedReasoningEngine; }
namespace memory { class AwakeConsolidationEngine; }
namespace economy { class TaskMarketplace; class ResourceAwarePolicy; }
namespace embodiment { class SensorimotorInterface; class GUIAutomation; }
namespace world { class WorldModel; class EnvironmentSimulator; }
}
}

namespace yuki {
namespace brain {
namespace infra {

struct CheckpointConfig {
    // Scheduling
    std::chrono::minutes periodic_interval = std::chrono::minutes(10);
    bool checkpoint_on_shutdown = true;
    bool checkpoint_on_crash = true;
    
    // Storage
    std::string checkpoint_directory = "data/checkpoints";
    size_t max_checkpoints = 100;
    bool compress = true;
    bool encrypt = false;
    
    // Components to checkpoint
    bool checkpoint_vse = true;
    bool checkpoint_hpc = true;
    bool checkpoint_ync = true;
    bool checkpoint_drives = true;
    bool checkpoint_self_model = true;
    bool checkpoint_identity = true;
    bool checkpoint_autonomy = true;
    bool checkpoint_policy = true;
    bool checkpoint_reasoning = true;
    bool checkpoint_memory = true;
    bool checkpoint_economy = true;
    bool checkpoint_embodiment = true;
    bool checkpoint_world = true;
    
    // Validation
    bool validate_on_load = true;
    bool validate_checksums = true;
};

struct CheckpointMetadata {
    std::string checkpoint_id;
    std::chrono::system_clock::time_point timestamp;
    std::string session_id;
    std::string version;
    std::string git_commit;
    std::unordered_map<std::string, bool> components_included;
    std::unordered_map<std::string, size_t> component_sizes;
    std::string checksum;
    size_t total_size_bytes = 0;
    std::chrono::milliseconds save_duration;
};

struct CheckpointResult {
    bool success = false;
    std::string checkpoint_id;
    std::string error_message;
    CheckpointMetadata metadata;
};

struct RestoreResult {
    bool success = false;
    std::string error_message;
    CheckpointMetadata metadata;
    std::chrono::milliseconds restore_duration;
};

class StateCheckpoint {
public:
    explicit StateCheckpoint(const CheckpointConfig& cfg = CheckpointConfig{});
    ~StateCheckpoint();
    
    void setConfig(const CheckpointConfig& cfg) { config_ = cfg; }
    
    // Component registration
    void registerVSE(inference::VariationalStateEstimator* vse) { vse_ = vse; }
    void registerHPC(core::HierarchicalPredictiveCoding* hpc) { hpc_ = hpc; }
    void registerYNC(ync::NeuromorphicSimulator* ync) { ync_ = ync; }
    void registerDrives(organism::ContinuousDriveSystem* drives) { drives_ = drives; }
    void registerSelfModel(self::RichSelfModel* sm) { self_model_ = sm; }
    void registerIdentityCore(self::IdentityCore* ic) { identity_core_ = ic; }
    void registerActiveToM(self::ActiveToM* tom) { active_tom_ = tom; }
    void registerMetacognition(self::MetacognitiveMonitor* mc) { metacognition_ = mc; }
    void registerCompetenceTracker(self::CompetenceTracker* ct) { competence_tracker_ = ct; }
    // void registerGoalGenerator(autonomy::GoalGenerator* gg) { goal_generator_ = gg; }
// void registerCuriosityEngine(autonomy::CuriosityEngine* ce) { curiosity_engine_ = ce; }
// void registerCurriculumBuilder(autonomy::CurriculumBuilder* cb) { curriculum_builder_ = cb; }
// void registerSkillPipeline(autonomy::SkillAcquisitionPipeline* sp) { skill_pipeline_ = sp; }
// void registerToolDiscovery(autonomy::ToolDiscovery* td) { tool_discovery_ = td; }
// void registerKnowledgeSync(autonomy::KnowledgeSync* ks) { knowledge_sync_ = ks; }
// void registerUpgradeScheduler(autonomy::UpgradeScheduler* us) { upgrade_scheduler_ = us; }
// void registerPolicyEngine(policy::ActiveInferencePolicyEngine* pe) { policy_engine_ = pe; }
    void registerBeliefRevision(reasoning::BeliefRevisionEngine* bre) { belief_revision_ = bre; }
    void registerStructureLearner(reasoning::StructureLearner* sl) { structure_learner_ = sl; }
    void registerUnifiedReasoning(reasoning::UnifiedReasoningEngine* ure) { unified_reasoning_ = ure; }
    void registerAwakeConsolidation(memory::AwakeConsolidationEngine* ace) { awake_consolidation_ = ace; }
    void registerTaskMarketplace(economy::TaskMarketplace* tm) { task_marketplace_ = tm; }
    void registerResourcePolicy(economy::ResourceAwarePolicy* rap) { resource_policy_ = rap; }
    void registerSensorimotor(embodiment::SensorimotorInterface* si) { sensorimotor_ = si; }
    void registerGUIAutomation(embodiment::GUIAutomation* ga) { gui_automation_ = ga; }
    void registerWorldModel(world::WorldModel* wm) { world_model_ = wm; }
    void registerEnvironmentSimulator(world::EnvironmentSimulator* es) { env_simulator_ = es; }
    
    // Main operations
    CheckpointResult saveCheckpoint(const std::string& reason = "periodic");
    RestoreResult loadCheckpoint(const std::string& checkpoint_id);
    RestoreResult loadLatestCheckpoint();
    
    // Management
    std::vector<CheckpointMetadata> listCheckpoints() const;
    bool deleteCheckpoint(const std::string& checkpoint_id);
    bool validateCheckpoint(const std::string& checkpoint_id) const;
    
    // Scheduling
    void startPeriodicCheckpoint();
    void stopPeriodicCheckpoint();
    void triggerCheckpoint(const std::string& reason = "manual");
    
    // Serialization
    std::string serializeMetadata() const;
    bool deserializeMetadata(const std::string& data);
    
    // Getters
    const CheckpointConfig& getConfig() const;
    bool isPeriodicRunning() const;

private:
    CheckpointConfig config_;
    
    // Component pointers (not owned)
    inference::VariationalStateEstimator* vse_ = nullptr;
    core::HierarchicalPredictiveCoding* hpc_ = nullptr;
    ync::NeuromorphicSimulator* ync_ = nullptr;
    organism::ContinuousDriveSystem* drives_ = nullptr;
    self::RichSelfModel* self_model_ = nullptr;
    self::IdentityCore* identity_core_ = nullptr;
    self::ActiveToM* active_tom_ = nullptr;
    self::MetacognitiveMonitor* metacognition_ = nullptr;
    self::CompetenceTracker* competence_tracker_ = nullptr;
    // autonomy::GoalGenerator* goal_generator_ = nullptr;
// autonomy::CuriosityEngine* curiosity_engine_ = nullptr;
// autonomy::CurriculumBuilder* curriculum_builder_ = nullptr;
// autonomy::SkillAcquisitionPipeline* skill_pipeline_ = nullptr;
// autonomy::ToolDiscovery* tool_discovery_ = nullptr;
// autonomy::KnowledgeSync* knowledge_sync_ = nullptr;
// autonomy::UpgradeScheduler* upgrade_scheduler_ = nullptr;
// policy::ActiveInferencePolicyEngine* policy_engine_ = nullptr;
    reasoning::BeliefRevisionEngine* belief_revision_ = nullptr;
    reasoning::StructureLearner* structure_learner_ = nullptr;
    reasoning::UnifiedReasoningEngine* unified_reasoning_ = nullptr;
    memory::AwakeConsolidationEngine* awake_consolidation_ = nullptr;
    economy::TaskMarketplace* task_marketplace_ = nullptr;
    economy::ResourceAwarePolicy* resource_policy_ = nullptr;
    embodiment::SensorimotorInterface* sensorimotor_ = nullptr;
    embodiment::GUIAutomation* gui_automation_ = nullptr;
    world::WorldModel* world_model_ = nullptr;
    world::EnvironmentSimulator* env_simulator_ = nullptr;
    
    // State
    std::atomic<bool> periodic_running_{false};
    std::thread periodic_thread_;
    std::atomic<bool> shutdown_requested_{false};
    std::mutex checkpoint_mutex_;
    
    // Helpers
    std::string generateCheckpointId() const;
    std::string getCheckpointPath(const std::string& checkpoint_id) const;
    std::string getMetadataPath(const std::string& checkpoint_id) const;
    
    // Component serialization
    template<typename T>
    std::string serializeComponent(const T* component) const;
    
    template<typename T>
    bool deserializeComponent(T* component, const std::string& data) const;
    
    // File operations
    bool writeCheckpointFile(const std::string& path, const std::string& data) const;
    bool readCheckpointFile(const std::string& path, std::string& data) const;
    bool computeChecksum(const std::string& data, std::string& checksum) const;
    
    // Periodic thread
    void periodicLoop();
    
    // Validation
    bool validateComponentSizes(const CheckpointMetadata& metadata) const;
};

} // namespace infra
} // namespace brain
} // namespace yuki