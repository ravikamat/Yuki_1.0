#pragma once
// CognitiveConfig — Unified flatbuffers configuration for entire cognitive system
// Hot-reloadable, versioned, schema-validated, no compile-time constants

#include <vector>
#include <string>
#include <unordered_map>
#include <chrono>
#include <variant>
#include <optional>
#include <functional>
#include <flatbuffers/flatbuffers.h>
#include "ConfigSchema_generated.h"

namespace yuki {
namespace brain {
namespace infra {

struct CognitiveConfig {
    // Version
    uint32_t schema_version = 1;
    uint32_t config_version = 1;
    
    // Core cognitive parameters - VSEConfig uses FlatBuffers-generated type (single source of truth: ConfigSchema.fbs)
    using VSEConfig = ConfigSchema::VSEConfigT;
    VSEConfig vse;
    
    struct HPCConfig {
        std::chrono::milliseconds l1_interval{10};
        std::chrono::milliseconds l2_interval{100};
        std::chrono::milliseconds l3_interval{1000};
        size_t l1_dim = 24;
        size_t l2_dim = 64;
        size_t l3_dim = 128;
        float l1_precision = 10.0f;
        float l2_precision = 5.0f;
        float l3_precision = 2.0f;
        float top_down_weight = 0.7f;
        float bottom_up_weight = 0.3f;
        float surprise_threshold = 2.0f;
    } hpc;
    
    struct YNCConfig {
        size_t sensory_neurons = 128;
        size_t motor_neurons = 16;
        size_t interneurons = 64;
        float connectivity_density = 0.05f;
        float learning_rate = 0.01f;
        float homeostatic_target = 0.1f;
        float stdp_a_plus = 0.01f;
        float stdp_a_minus = 0.012f;
        float stdp_tau_plus = 20.0f;
        float stdp_tau_minus = 20.0f;
    } ync;
    
    struct DriveConfig {
        float curiosity_prediction_error_gain = 2.0f;
        float curiosity_competence_progress_gain = 1.5f;
        float social_prediction_error_gain = 1.0f;
        float competence_success_gain = 1.0f;
        float competence_failure_gain = -0.5f;
        float energy_compute_cost_gain = -0.5f;
        float safety_surprise_gain = 2.0f;
        float autonomy_control_gain = 1.0f;
        
        std::array<float, 6> decay_rate = {0.01f, 0.005f, 0.002f, 0.05f, 0.1f, 0.001f};
        std::array<float, 6> setpoint = {0.5f, 0.3f, 0.6f, 0.8f, 0.1f, 0.4f};
        std::array<float, 6> min_val = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
        std::array<float, 6> max_val = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
    } drives;
    
    struct SelfModelConfig {
        std::chrono::hours short_horizon{1};
        std::chrono::hours medium_horizon{24};
        std::chrono::hours long_horizon{168};
        float alpha = 0.1f;
        float min_uncertainty = 0.05f;
        float max_uncertainty = 0.5f;
        int max_error_modes_per_domain = 5;
        float error_mode_threshold = 0.1f;
    } self_model;
    
    struct ToMConfig {
        int max_recursion_depth = 2;
        float alpha = 0.1f;
        int num_user_intents = 8;
        int num_user_goals = 10;
        int num_user_beliefs = 20;
        float min_uncertainty = 0.1f;
        float max_uncertainty = 0.9f;
        float surprise_threshold = 0.3f;
    } tom;
    
    struct MetacognitionConfig {
        std::chrono::seconds short_window{60};
        std::chrono::seconds medium_window{300};
        std::chrono::seconds long_window{3600};
        float low_confidence_threshold = 0.4f;
        float high_uncertainty_threshold = 0.6f;
        float high_surprise_threshold = 0.7f;
        float high_resource_pressure = 0.8f;
        std::chrono::seconds shift_cooldown{30};
        float compute_budget_warning = 0.7f;
        float compute_budget_critical = 0.9f;
        float calibration_alpha = 0.1f;
    } metacognition;
    
    struct IdentityConfig {
        int identity_dim = 64;
        int num_core_values = 10;
        int num_instrumental_values = 20;
        float identity_stability = 0.99f;
        float value_stability = 0.995f;
        float value_learning_rate = 0.01f;
        int min_examples_for_value_change = 50;
    } identity;
    
    struct PersonaConfig {
        int max_conversation_episodes = 10000;
        int max_turn_memory = 100;
        std::chrono::hours episode_ttl{24 * 30};
        float emotion_decay_per_hour = 0.1f;
        float valence_threshold = 0.3f;
        float arousal_threshold = 0.5f;
        float consistency_threshold = 0.7f;
        float adaptivity = 0.1f;
        float friend_mode_social_drive_threshold = 0.6f;
        float friend_mode_precision_threshold = 0.4f;
    } persona;
    
    struct PolicyConfig {
        int planning_horizon = 5;
        int num_policies = 32;
        float efe_temperature = 1.0f;
        int num_actions = 12;
        bool use_heuristic_policies = true;
        bool use_random_policies = true;
        int num_heuristic = 8;
        int num_random = 8;
        float surprise_replan_threshold = 2.0f;
        float precision_drop_threshold = 0.3f;
        std::chrono::milliseconds min_replan_interval{500};
    } policy;
    
    struct ResourcePolicyConfig {
        float compute_cost_per_ms = 0.0001f;
        float memory_cost_per_mb_hour = 0.00001f;
        float network_cost_per_mb = 0.0001f;
        float tool_cost_multiplier = 1.5f;
        float daily_compute_budget = 10.0f;
        float daily_network_budget = 1.0f;
        float emergency_reserve = 5.0f;
        float financial_viability_weight = 0.3f;
        float sustainability_drive_gain = 0.1f;
        float bankruptcy_threshold = 0.1f;
        bool enable_ab_testing = true;
        float ab_test_fraction = 0.1f;
    } resource_policy;
    
    struct MarketplaceConfig {
        std::vector<std::string> platforms = {"github", "upwork", "custom"};
        std::string api_base_url;
        std::string api_key;
        float min_bid_amount = 10.0f;
        float max_bid_amount = 10000.0f;
        float bid_margin = 0.2f;
        int max_concurrent_bids = 5;
        float initial_reputation = 0.5f;
        float reputation_decay_per_day = 0.001f;
        float platform_fee = 0.1f;
        std::chrono::hours payment_timeout{24 * 7};
        float min_task_match_score = 0.6f;
        std::vector<std::string> excluded_keywords;
    } marketplace;
    
    struct UpgradeConfig {
        std::chrono::hours check_interval{6};
        float competence_gap_threshold = 0.3f;
        float performance_degradation_threshold = 0.1f;
        int max_concurrent_upgrades = 2;
        float min_upgrade_confidence = 0.7f;
        float min_roi = 1.5f;
        std::chrono::hours sandbox_test_duration{24};
        float ab_test_traffic_fraction = 0.1f;
        float promotion_threshold = 0.6f;
        float rollback_threshold = 0.1f;
        std::chrono::hours max_rollback_time{1};
    } upgrade;
    
    struct KnowledgeSyncConfig {
        std::chrono::hours sync_interval{24};
        std::chrono::minutes quick_sync_interval{30};
        std::vector<std::string> monitored_domains = {
            "technology", "science", "medicine", "programming", "ai",
            "news", "finance", "climate", "space", "biology"
        };
        int max_sources_per_domain = 10;
        int max_articles_per_source = 5;
        float min_source_credibility = 0.6f;
        float min_fact_confidence = 0.7f;
        int max_facts_per_sync = 100;
        bool auto_resolve_conflicts = true;
        float conflict_threshold = 0.3f;
        std::chrono::hours max_fact_age{24 * 30};
        float freshness_decay_per_day = 0.02f;
    } knowledge_sync;
    
    struct SkillAcquisitionConfig {
        bool enable_curriculum = true;
        bool enable_research = true;
        bool enable_practice = true;
        bool enable_verification = true;
        bool enable_integration = true;
        float curriculum_min_quality = 0.7f;
        float research_min_sources = 5.0f;
        float practice_min_sessions = 10.0f;
        float verification_threshold = 0.8f;
        float integration_confidence = 0.7f;
        float max_compute_per_skill = 1000.0f;
        float max_time_hours = 24.0f;
        int max_parallel_skills = 2;
        int max_retries_per_stage = 3;
        float retry_backoff_hours = 1.0f;
    } skill_acquisition;
    
    struct ToolDiscoveryConfig {
        int max_search_results = 20;
        int max_docs_to_analyze = 10;
        float min_relevance_score = 0.4f;
        bool use_llm_for_generation = true;
        int max_generation_attempts = 3;
        int max_test_cases = 10;
        float min_test_success_rate = 0.8f;
        float test_timeout_seconds = 60.0f;
        bool auto_register = true;
        bool require_approval = false;
        bool validate_safety = true;
        std::vector<std::string> blocked_domains;
        std::vector<std::string> blocked_keywords;
    } tool_discovery;
    
    struct CurriculumBuilderConfig {
        int max_resources_per_module = 5;
        int max_search_results = 20;
        float min_resource_quality = 0.4f;
        float max_hours_per_day = 2.0f;
        float practice_ratio = 0.6f;
        float mastery_threshold = 0.8f;
        int verification_attempts = 3;
        bool auto_generate_project = true;
        int project_complexity_levels = 3;
    } curriculum_builder;
    
    struct ToolPrimitiveConfig {
        float forward_model_lr = 0.01f;
        int min_samples_for_reliable = 10;
        int state_dim = 64;
        int action_dim = 16;
        int outcome_dim = 32;
        float execution_timeout_seconds = 30.0f;
        int max_retries = 3;
        bool require_approval = false;
        float safety_threshold = 0.1f;
    } tool_primitive;
    
    struct BeliefRevisionConfig {
        float surprise_threshold = 0.7f;
        float conflict_threshold = 0.3f;
        float evidence_weight_threshold = 0.5f;
        int num_counterfactuals = 5;
        float perturbation_sigma = 0.1f;
        float cf_horizon_hours = 24.0f;
        float source_reliability_prior = 0.7f;
        float recency_weight = 0.9f;
        int min_evidence_samples = 3;
        bool enable_counterfactual = true;
        bool enable_gradual_revision = true;
        float max_revision_per_step = 0.2f;
        float consolidation_threshold = 0.8f;
        int consolidation_delay_hours = 1;
    } belief_revision;
    
    struct StructureLearnerConfig {
        float independence_threshold = 0.05f;
        int max_conditioning_set_size = 3;
        float edge_confidence_threshold = 0.7f;
        float intervention_weight = 2.0f;
        int min_interventions_per_edge = 3;
        int max_nodes = 1000;
        float prune_threshold = 0.1f;
        float learning_rate = 0.01f;
        int max_iterations = 100;
        bool sync_with_hdc = true;
        float hdc_sync_weight = 0.5f;
    } structure_learner;
    
    struct UnifiedReasoningConfig {
        int planning_horizon = 5;
        int max_policies = 32;
        float efe_temperature = 1.0f;
        bool absorb_htn_planner = true;
        bool absorb_causal_reasoning = true;
        bool absorb_counterfactual = true;
        int default_mode = 1; // DELIBERATIVE
        float mode_switch_threshold = 0.3f;
        bool enable_skill_chaining = true;
        int max_skill_chain_length = 5;
        float skill_composition_cost = 0.1f;
    } unified_reasoning;
    
    struct UpgradeSchedulerConfig {
        std::chrono::hours check_interval{6};
        float competence_gap_threshold = 0.3f;
        float performance_degradation_threshold = 0.1f;
        int max_concurrent_upgrades = 2;
        float min_upgrade_confidence = 0.7f;
        float min_roi = 1.5f;
        std::chrono::hours sandbox_test_duration{24};
        float ab_test_traffic_fraction = 0.1f;
        float promotion_threshold = 0.6f;
        float rollback_threshold = 0.1f;
        std::chrono::hours max_rollback_time{1};
    } upgrade_scheduler;
    
    struct CheckpointConfig {
        std::chrono::minutes periodic_interval{10};
        bool checkpoint_on_shutdown = true;
        bool checkpoint_on_crash = true;
        std::string checkpoint_directory = "data/checkpoints";
        size_t max_checkpoints = 100;
        bool compress = true;
        bool encrypt = false;
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
        bool validate_on_load = true;
        bool validate_checksums = true;
    } checkpoint;
    
    struct EventLogConfig {
        std::string log_directory = "data/cognition_logs";
        size_t max_file_size_mb = 100;
        size_t max_files = 1000;
        bool compress = true;
        std::chrono::hours retention_days{24 * 30};
        size_t buffer_size = 10000;
        std::chrono::milliseconds flush_interval{100};
        bool build_index = true;
        size_t index_flush_interval = 1000;
    } event_log;
    
    struct MetricsConfig {
        std::string bind_address = "0.0.0.0";
        int port = 9090;
        std::string metrics_path = "/metrics";
        std::chrono::seconds scrape_interval{15};
        bool enable_default_metrics = true;
        size_t max_label_cardinality = 1000;
        bool enable_histograms = true;
        bool enable_summaries = true;
    } metrics;
    
    struct SensorimotorConfig {
        bool enable_text = true;
        bool enable_audio = true;
        bool enable_vision = true;
        bool enable_screen = true;
        bool enable_robotics = false;
        std::chrono::milliseconds observation_interval{100};
        std::chrono::milliseconds action_timeout{5000};
        size_t observation_buffer_size = 1000;
    } sensorimotor;
    
    struct GUIAutomationConfig {
        bool enable_uia = true;
        int uia_timeout_ms = 5000;
        bool enable_uia_cache = true;
        int cache_ttl_ms = 1000;
        bool enable_ocr = true;
        std::string ocr_language = "eng";
        int ocr_dpi = 300;
        float ocr_confidence_threshold = 0.7f;
        bool enable_visual_grounding = true;
        float grounding_iou_threshold = 0.5f;
        int default_click_delay_ms = 50;
        int default_type_delay_ms = 20;
        int action_timeout_ms = 10000;
        bool enable_element_cache = true;
        int cache_size = 1000;
    } gui_automation;
    
    struct WorldModelConfig {
        size_t latent_dim = 256;
        size_t deterministic_dim = 256;
        size_t stochastic_dim = 32;
        int discrete_categories = 32;
        size_t encoder_layers = 4;
        size_t decoder_layers = 4;
        size_t rssm_layers = 2;
        size_t hidden_dim = 1024;
        float dropout = 0.1f;
        float learning_rate = 3e-4f;
        float kl_weight = 1.0f;
        float free_nats = 3.0f;
        int batch_size = 32;
        int sequence_length = 50;
        float grad_clip = 100.0f;
        int imagination_horizon = 15;
        int imagination_samples = 10;
        float text_weight = 1.0f;
        float image_weight = 1.0f;
        float reward_weight = 1.0f;
    } world_model;
    
    struct SimulatorConfig {
        bool enable_physics = true;
        float gravity = -9.81f;
        float timestep = 1.0f / 60.0f;
        int substeps = 4;
        bool enable_rendering = false;
        int render_width = 800;
        int render_height = 600;
        int max_agents = 10;
        bool enable_tool_simulation = true;
        bool record_trajectories = true;
        int max_trajectory_length = 10000;
    } simulator;
    
    // Default constructor initializes all sub-configs with defaults
    CognitiveConfig() = default;
    
    // Factory method to load VSE config from JSON (single source of truth: ConfigSchema.fbs)
    static CognitiveConfig loadFromJson(const std::string& json_path = "config/cognitive_config.json");
    
    // FlatBuffers serialization
    flatbuffers::Offset<ConfigSchema::CognitiveConfig> serialize(flatbuffers::FlatBufferBuilder& builder) const;
    static CognitiveConfig deserialize(const flatbuffers::Table* table);
    static CognitiveConfig deserializeFromFile(const std::string& filepath);
    bool serializeToFile(const std::string& filepath) const;
    
    // Hot-reload support
    bool hotReload(const std::string& filepath);
    void registerHotReloadCallback(std::function<void(const CognitiveConfig&)> callback);
    
    // Validation
    bool validate() const;
    std::vector<std::string> getValidationErrors() const;
    
    // Diff
    static std::vector<std::string> diff(const CognitiveConfig& a, const CognitiveConfig& b);
    
    // Schema version
    static constexpr uint32_t SCHEMA_VERSION = 1;
};

} // namespace infra
} // namespace brain
} // namespace yuki