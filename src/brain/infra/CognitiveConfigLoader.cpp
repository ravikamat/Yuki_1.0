#include "brain/infra/CognitiveConfigLoader.h"
#include <fstream>
#include <iostream>

namespace yuki {
namespace brain {
namespace infra {

std::optional<nlohmann::json> CognitiveConfigLoader::loadJsonFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return std::nullopt;
    }
    try {
        nlohmann::json json;
        file >> json;
        return json;
    } catch (const std::exception& e) {
        std::cerr << "Failed to parse JSON config: " << e.what() << std::endl;
        return std::nullopt;
    }
}

nlohmann::json CognitiveConfigLoader::getSection(const nlohmann::json& root, const std::string& section) {
    auto it = root.find(section);
    if (it != root.end()) {
        return *it;
    }
    return nlohmann::json::object();
}

CognitiveConfig::VSEConfig CognitiveConfigLoader::loadVSEConfig(const std::string& json_path) {
    CognitiveConfig::VSEConfig config;
    auto json_opt = loadJsonFile(json_path);
    if (!json_opt) return config;
    
    auto section = getSection(*json_opt, "vse");
    if (section.is_object()) {
        config.surprise_threshold = section.value("surprise_threshold", 0.7f);
        config.entropy_threshold = section.value("entropy_threshold", 0.5f);
        config.precision_floor = section.value("precision_floor", 0.05f);
        config.precision_ceil = section.value("precision_ceil", 0.98f);
        config.recovery_rate = section.value("recovery_rate", 0.01f);
        config.baseline_intent = section.value("baseline_intent", 0.125f);
        config.baseline_entity = section.value("baseline_entity", 0.33f);
        config.baseline_tone = section.value("baseline_tone", 0.5f);
        config.baseline_source = section.value("baseline_source", 0.5f);
        config.calibration_warmup_alpha = section.value("calibration_warmup_alpha", 0.3f);
        config.calibration_steady_alpha = section.value("calibration_steady_alpha", 0.05f);
    }
    return config;
}

ConfigSchema::HPCConfigT CognitiveConfigLoader::loadHPCConfig(const std::string& json_path) {
    ConfigSchema::HPCConfigT config;
    auto json_opt = loadJsonFile(json_path);
    if (!json_opt) return config;
    
    auto section = getSection(*json_opt, "hpc");
    if (section.is_object()) {
        config.l1_interval_ms = section.value("l1_interval_ms", 10u);
        config.l2_interval_ms = section.value("l2_interval_ms", 100u);
        config.l3_interval_ms = section.value("l3_interval_ms", 1000u);
        config.l1_dim = section.value("l1_dim", 24u);
        config.l2_dim = section.value("l2_dim", 64u);
        config.l3_dim = section.value("l3_dim", 128u);
        config.l1_precision = section.value("l1_precision", 10.0f);
        config.l2_precision = section.value("l2_precision", 5.0f);
        config.l3_precision = section.value("l3_precision", 2.0f);
        config.top_down_weight = section.value("top_down_weight", 0.7f);
        config.bottom_up_weight = section.value("bottom_up_weight", 0.3f);
        config.surprise_threshold = section.value("surprise_threshold", 2.0f);
    }
    return config;
}

ConfigSchema::YNCConfigT CognitiveConfigLoader::loadYNCConfig(const std::string& json_path) {
    ConfigSchema::YNCConfigT config;
    auto json_opt = loadJsonFile(json_path);
    if (!json_opt) return config;
    
    auto section = getSection(*json_opt, "ync");
    if (section.is_object()) {
        config.sensory_neurons = section.value("sensory_neurons", 128u);
        config.motor_neurons = section.value("motor_neurons", 16u);
        config.interneurons = section.value("interneurons", 64u);
        config.connectivity_density = section.value("connectivity_density", 0.05f);
        config.learning_rate = section.value("learning_rate", 0.01f);
        config.homeostatic_target = section.value("homeostatic_target", 0.1f);
        config.stdp_a_plus = section.value("stdp_a_plus", 0.01f);
        config.stdp_a_minus = section.value("stdp_a_minus", 0.012f);
        config.stdp_tau_plus = section.value("stdp_tau_plus", 20.0f);
        config.stdp_tau_minus = section.value("stdp_tau_minus", 20.0f);
    }
    return config;
}

ConfigSchema::DriveConfigT CognitiveConfigLoader::loadDriveConfig(const std::string& json_path) {
    ConfigSchema::DriveConfigT config;
    auto json_opt = loadJsonFile(json_path);
    if (!json_opt) return config;
    
    auto section = getSection(*json_opt, "drives");
    if (section.is_object()) {
        config.curiosity_prediction_error_gain = section.value("curiosity_prediction_error_gain", 2.0f);
        config.curiosity_competence_progress_gain = section.value("curiosity_competence_progress_gain", 1.5f);
        config.social_prediction_error_gain = section.value("social_prediction_error_gain", 1.0f);
        config.competence_success_gain = section.value("competence_success_gain", 1.0f);
        config.competence_failure_gain = section.value("competence_failure_gain", -0.5f);
        config.energy_compute_cost_gain = section.value("energy_compute_cost_gain", -0.5f);
        config.safety_surprise_gain = section.value("safety_surprise_gain", 2.0f);
        config.autonomy_control_gain = section.value("autonomy_control_gain", 1.0f);
        
        auto decay_rate = section.value("decay_rate", nlohmann::json::array({0.01f, 0.005f, 0.002f, 0.05f, 0.1f, 0.001f}));
        auto setpoint = section.value("setpoint", nlohmann::json::array({0.5f, 0.3f, 0.6f, 0.8f, 0.1f, 0.4f}));
        auto min_val = section.value("min_val", nlohmann::json::array({0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}));
        auto max_val = section.value("max_val", nlohmann::json::array({1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f}));
        
        auto make_float6 = [](const nlohmann::json& arr) {
            std::array<float, 6> values{};
            for (size_t i = 0; i < 6 && i < arr.size(); ++i) {
                values[i] = arr[i];
            }
            return std::make_unique<ConfigSchema::Float6>(values);
        };
        
        config.decay_rate = make_float6(decay_rate);
        config.setpoint = make_float6(setpoint);
        config.min_val = make_float6(min_val);
        config.max_val = make_float6(max_val);
    }
    return config;
}

ConfigSchema::SelfModelConfigT CognitiveConfigLoader::loadSelfModelConfig(const std::string& json_path) {
    ConfigSchema::SelfModelConfigT config;
    auto json_opt = loadJsonFile(json_path);
    if (!json_opt) return config;
    
    auto section = getSection(*json_opt, "self_model");
    if (section.is_object()) {
        config.short_horizon_hours = section.value("short_horizon_hours", 1u);
        config.medium_horizon_hours = section.value("medium_horizon_hours", 24u);
        config.long_horizon_hours = section.value("long_horizon_hours", 168u);
        config.alpha = section.value("alpha", 0.1f);
        config.min_uncertainty = section.value("min_uncertainty", 0.05f);
        config.max_uncertainty = section.value("max_uncertainty", 0.5f);
        config.max_error_modes_per_domain = section.value("max_error_modes_per_domain", 5u);
        config.error_mode_threshold = section.value("error_mode_threshold", 0.1f);
    }
    return config;
}

ConfigSchema::ToMConfigT CognitiveConfigLoader::loadToMConfig(const std::string& json_path) {
    ConfigSchema::ToMConfigT config;
    auto json_opt = loadJsonFile(json_path);
    if (!json_opt) return config;
    
    auto section = getSection(*json_opt, "tom");
    if (section.is_object()) {
        config.max_recursion_depth = section.value("max_recursion_depth", 2u);
        config.alpha = section.value("alpha", 0.1f);
        config.num_user_intents = section.value("num_user_intents", 8u);
        config.num_user_goals = section.value("num_user_goals", 10u);
        config.num_user_beliefs = section.value("num_user_beliefs", 20u);
        config.min_uncertainty = section.value("min_uncertainty", 0.1f);
        config.max_uncertainty = section.value("max_uncertainty", 0.9f);
        config.surprise_threshold = section.value("surprise_threshold", 0.3f);
    }
    return config;
}

ConfigSchema::MetacognitionConfigT CognitiveConfigLoader::loadMetacognitionConfig(const std::string& json_path) {
    ConfigSchema::MetacognitionConfigT config;
    auto json_opt = loadJsonFile(json_path);
    if (!json_opt) return config;
    
    auto section = getSection(*json_opt, "metacognition");
    if (section.is_object()) {
        config.short_window_seconds = section.value("short_window_seconds", 60u);
        config.medium_window_seconds = section.value("medium_window_seconds", 300u);
        config.long_window_seconds = section.value("long_window_seconds", 3600u);
        config.low_confidence_threshold = section.value("low_confidence_threshold", 0.4f);
        config.high_uncertainty_threshold = section.value("high_uncertainty_threshold", 0.6f);
        config.high_surprise_threshold = section.value("high_surprise_threshold", 0.7f);
        config.high_resource_pressure = section.value("high_resource_pressure", 0.8f);
        config.shift_cooldown_seconds = section.value("shift_cooldown_seconds", 30u);
        config.compute_budget_warning = section.value("compute_budget_warning", 0.7f);
        config.compute_budget_critical = section.value("compute_budget_critical", 0.9f);
        config.calibration_alpha = section.value("calibration_alpha", 0.1f);
    }
    return config;
}

ConfigSchema::IdentityConfigT CognitiveConfigLoader::loadIdentityConfig(const std::string& json_path) {
    ConfigSchema::IdentityConfigT config;
    auto json_opt = loadJsonFile(json_path);
    if (!json_opt) return config;
    
    auto section = getSection(*json_opt, "identity");
    if (section.is_object()) {
        config.identity_dim = section.value("identity_dim", 64u);
        config.num_core_values = section.value("num_core_values", 10u);
        config.num_instrumental_values = section.value("num_instrumental_values", 20u);
        config.identity_stability = section.value("identity_stability", 0.99f);
        config.value_stability = section.value("value_stability", 0.995f);
        config.value_learning_rate = section.value("value_learning_rate", 0.01f);
        config.min_examples_for_value_change = section.value("min_examples_for_value_change", 50u);
    }
    return config;
}

ConfigSchema::PersonaConfigT CognitiveConfigLoader::loadPersonaConfig(const std::string& json_path) {
    ConfigSchema::PersonaConfigT config;
    auto json_opt = loadJsonFile(json_path);
    if (!json_opt) return config;
    
    auto section = getSection(*json_opt, "persona");
    if (section.is_object()) {
        config.max_conversation_episodes = section.value("max_conversation_episodes", 10000u);
        config.max_turn_memory = section.value("max_turn_memory", 100u);
        config.episode_ttl_hours = section.value("episode_ttl_hours", 720u);
        config.emotion_decay_per_hour = section.value("emotion_decay_per_hour", 0.1f);
        config.valence_threshold = section.value("valence_threshold", 0.3f);
        config.arousal_threshold = section.value("arousal_threshold", 0.5f);
        config.consistency_threshold = section.value("consistency_threshold", 0.7f);
        config.adaptivity = section.value("adaptivity", 0.1f);
        config.friend_mode_social_drive_threshold = section.value("friend_mode_social_drive_threshold", 0.6f);
        config.friend_mode_precision_threshold = section.value("friend_mode_precision_threshold", 0.4f);
    }
    return config;
}

ConfigSchema::PolicyConfigT CognitiveConfigLoader::loadPolicyConfig(const std::string& json_path) {
    ConfigSchema::PolicyConfigT config;
    auto json_opt = loadJsonFile(json_path);
    if (!json_opt) return config;
    
    auto section = getSection(*json_opt, "policy");
    if (section.is_object()) {
        config.planning_horizon = section.value("planning_horizon", 5u);
        config.num_policies = section.value("num_policies", 32u);
        config.efe_temperature = section.value("efe_temperature", 1.0f);
        config.num_actions = section.value("num_actions", 12u);
        config.use_heuristic_policies = section.value("use_heuristic_policies", true);
        config.use_random_policies = section.value("use_random_policies", true);
        config.num_heuristic = section.value("num_heuristic", 8u);
        config.num_random = section.value("num_random", 8u);
        config.surprise_replan_threshold = section.value("surprise_replan_threshold", 2.0f);
        config.precision_drop_threshold = section.value("precision_drop_threshold", 0.3f);
        config.min_replan_interval_ms = section.value("min_replan_interval_ms", 500u);
    }
    return config;
}

ConfigSchema::ResourcePolicyConfigT CognitiveConfigLoader::loadResourcePolicyConfig(const std::string& json_path) {
    ConfigSchema::ResourcePolicyConfigT config;
    auto json_opt = loadJsonFile(json_path);
    if (!json_opt) return config;
    
    auto section = getSection(*json_opt, "resource_policy");
    if (section.is_object()) {
        config.compute_cost_per_ms = section.value("compute_cost_per_ms", 0.0001f);
        config.memory_cost_per_mb_hour = section.value("memory_cost_per_mb_hour", 0.00001f);
        config.network_cost_per_mb = section.value("network_cost_per_mb", 0.0001f);
        config.tool_cost_multiplier = section.value("tool_cost_multiplier", 1.5f);
        config.daily_compute_budget = section.value("daily_compute_budget", 10.0f);
        config.daily_network_budget = section.value("daily_network_budget", 1.0f);
        config.emergency_reserve = section.value("emergency_reserve", 5.0f);
        config.financial_viability_weight = section.value("financial_viability_weight", 0.3f);
        config.sustainability_drive_gain = section.value("sustainability_drive_gain", 0.1f);
        config.bankruptcy_threshold = section.value("bankruptcy_threshold", 0.1f);
        config.enable_ab_testing = section.value("enable_ab_testing", true);
        config.ab_test_fraction = section.value("ab_test_fraction", 0.1f);
    }
    return config;
}

ConfigSchema::MarketplaceConfigT CognitiveConfigLoader::loadMarketplaceConfig(const std::string& json_path) {
    ConfigSchema::MarketplaceConfigT config;
    auto json_opt = loadJsonFile(json_path);
    if (!json_opt) return config;
    
    auto section = getSection(*json_opt, "marketplace");
    if (section.is_object()) {
        config.platforms = section.value("platforms", std::vector<std::string>{"github", "upwork", "custom"});
        config.api_base_url = section.value("api_base_url", std::string{""});
        config.api_key = section.value("api_key", std::string{""});
        config.min_bid_amount = section.value("min_bid_amount", 10.0f);
        config.max_bid_amount = section.value("max_bid_amount", 10000.0f);
        config.bid_margin = section.value("bid_margin", 0.2f);
        config.max_concurrent_bids = section.value("max_concurrent_bids", 5u);
        config.initial_reputation = section.value("initial_reputation", 0.5f);
        config.reputation_decay_per_day = section.value("reputation_decay_per_day", 0.001f);
        config.platform_fee = section.value("platform_fee", 0.1f);
        config.payment_timeout_hours = section.value("payment_timeout_hours", 168u);
        config.min_task_match_score = section.value("min_task_match_score", 0.6f);
        config.excluded_keywords = section.value("excluded_keywords", std::vector<std::string>{});
    }
    return config;
}

ConfigSchema::UpgradeConfigT CognitiveConfigLoader::loadUpgradeConfig(const std::string& json_path) {
    ConfigSchema::UpgradeConfigT config;
    auto json_opt = loadJsonFile(json_path);
    if (!json_opt) return config;
    
    auto section = getSection(*json_opt, "upgrade");
    if (section.is_object()) {
        config.check_interval_hours = section.value("check_interval_hours", 6u);
        config.competence_gap_threshold = section.value("competence_gap_threshold", 0.3f);
        config.performance_degradation_threshold = section.value("performance_degradation_threshold", 0.1f);
        config.max_concurrent_upgrades = section.value("max_concurrent_upgrades", 2u);
        config.min_upgrade_confidence = section.value("min_upgrade_confidence", 0.7f);
        config.min_roi = section.value("min_roi", 1.5f);
        config.sandbox_test_duration_hours = section.value("sandbox_test_duration_hours", 24u);
        config.ab_test_traffic_fraction = section.value("ab_test_traffic_fraction", 0.1f);
        config.promotion_threshold = section.value("promotion_threshold", 0.6f);
        config.rollback_threshold = section.value("rollback_threshold", 0.1f);
        config.max_rollback_time_hours = section.value("max_rollback_time_hours", 1u);
    }
    return config;
}

ConfigSchema::KnowledgeSyncConfigT CognitiveConfigLoader::loadKnowledgeSyncConfig(const std::string& json_path) {
    ConfigSchema::KnowledgeSyncConfigT config;
    auto json_opt = loadJsonFile(json_path);
    if (!json_opt) return config;
    
    auto section = getSection(*json_opt, "knowledge_sync");
    if (section.is_object()) {
        config.sync_interval_hours = section.value("sync_interval_hours", 24u);
        config.quick_sync_interval_minutes = section.value("quick_sync_interval_minutes", 30u);
        config.monitored_domains = section.value("monitored_domains", std::vector<std::string>{
            "technology", "science", "medicine", "programming", "ai",
            "news", "finance", "climate", "space", "biology"
        });
        config.max_sources_per_domain = section.value("max_sources_per_domain", 10u);
        config.max_articles_per_source = section.value("max_articles_per_source", 5u);
        config.min_source_credibility = section.value("min_source_credibility", 0.6f);
        config.min_fact_confidence = section.value("min_fact_confidence", 0.7f);
        config.max_facts_per_sync = section.value("max_facts_per_sync", 100u);
        config.auto_resolve_conflicts = section.value("auto_resolve_conflicts", true);
        config.conflict_threshold = section.value("conflict_threshold", 0.3f);
        config.max_fact_age_hours = section.value("max_fact_age_hours", 720u);
        config.freshness_decay_per_day = section.value("freshness_decay_per_day", 0.02f);
    }
    return config;
}

ConfigSchema::SkillAcquisitionConfigT CognitiveConfigLoader::loadSkillAcquisitionConfig(const std::string& json_path) {
    ConfigSchema::SkillAcquisitionConfigT config;
    auto json_opt = loadJsonFile(json_path);
    if (!json_opt) return config;
    
    auto section = getSection(*json_opt, "skill_acquisition");
    if (section.is_object()) {
        config.enable_curriculum = section.value("enable_curriculum", true);
        config.enable_research = section.value("enable_research", true);
        config.enable_practice = section.value("enable_practice", true);
        config.enable_verification = section.value("enable_verification", true);
        config.enable_integration = section.value("enable_integration", true);
        config.curriculum_min_quality = section.value("curriculum_min_quality", 0.7f);
        config.research_min_sources = section.value("research_min_sources", 5.0f);
        config.practice_min_sessions = section.value("practice_min_sessions", 10.0f);
        config.verification_threshold = section.value("verification_threshold", 0.8f);
        config.integration_confidence = section.value("integration_confidence", 0.7f);
        config.max_compute_per_skill = section.value("max_compute_per_skill", 1000.0f);
        config.max_time_hours = section.value("max_time_hours", 24.0f);
        config.max_parallel_skills = section.value("max_parallel_skills", 2u);
        config.max_retries_per_stage = section.value("max_retries_per_stage", 3u);
        config.retry_backoff_hours = section.value("retry_backoff_hours", 1.0f);
    }
    return config;
}

ConfigSchema::UpgradeSchedulerConfigT CognitiveConfigLoader::loadUpgradeSchedulerConfig(const std::string& json_path) {
    ConfigSchema::UpgradeSchedulerConfigT config;
    auto json_opt = loadJsonFile(json_path);
    if (!json_opt) return config;
    
    auto section = getSection(*json_opt, "upgrade_scheduler");
    if (section.is_object()) {
        config.check_interval_hours = section.value("check_interval_hours", 6u);
        config.competence_gap_threshold = section.value("competence_gap_threshold", 0.3f);
        config.performance_degradation_threshold = section.value("performance_degradation_threshold", 0.1f);
        config.max_concurrent_upgrades = section.value("max_concurrent_upgrades", 2u);
        config.min_upgrade_confidence = section.value("min_upgrade_confidence", 0.7f);
        config.min_roi = section.value("min_roi", 1.5f);
        config.sandbox_test_duration_hours = section.value("sandbox_test_duration_hours", 24u);
        config.ab_test_traffic_fraction = section.value("ab_test_traffic_fraction", 0.1f);
        config.promotion_threshold = section.value("promotion_threshold", 0.6f);
        config.rollback_threshold = section.value("rollback_threshold", 0.1f);
        config.max_rollback_time_hours = section.value("max_rollback_time_hours", 1u);
    }
    return config;
}

ConfigSchema::CheckpointConfigT CognitiveConfigLoader::loadCheckpointConfig(const std::string& json_path) {
    ConfigSchema::CheckpointConfigT config;
    auto json_opt = loadJsonFile(json_path);
    if (!json_opt) return config;
    
    auto section = getSection(*json_opt, "checkpoint");
    if (section.is_object()) {
        config.periodic_interval_minutes = section.value("periodic_interval_minutes", 10u);
        config.checkpoint_on_shutdown = section.value("checkpoint_on_shutdown", true);
        config.checkpoint_on_crash = section.value("checkpoint_on_crash", true);
        config.checkpoint_directory = section.value("checkpoint_directory", std::string{"data/checkpoints"});
        config.max_checkpoints = section.value("max_checkpoints", 100u);
        config.compress = section.value("compress", true);
        config.encrypt = section.value("encrypt", false);
        config.checkpoint_vse = section.value("checkpoint_vse", true);
        config.checkpoint_hpc = section.value("checkpoint_hpc", true);
        config.checkpoint_ync = section.value("checkpoint_ync", true);
        config.checkpoint_drives = section.value("checkpoint_drives", true);
        config.checkpoint_self_model = section.value("checkpoint_self_model", true);
        config.checkpoint_identity = section.value("checkpoint_identity", true);
        config.checkpoint_autonomy = section.value("checkpoint_autonomy", true);
        config.checkpoint_policy = section.value("checkpoint_policy", true);
        config.checkpoint_reasoning = section.value("checkpoint_reasoning", true);
        config.checkpoint_memory = section.value("checkpoint_memory", true);
        config.checkpoint_economy = section.value("checkpoint_economy", true);
        config.checkpoint_embodiment = section.value("checkpoint_embodiment", true);
        config.checkpoint_world = section.value("checkpoint_world", true);
        config.validate_on_load = section.value("validate_on_load", true);
        config.validate_checksums = section.value("validate_checksums", true);
    }
    return config;
}

ConfigSchema::EventLogConfigT CognitiveConfigLoader::loadEventLogConfig(const std::string& json_path) {
    ConfigSchema::EventLogConfigT config;
    auto json_opt = loadJsonFile(json_path);
    if (!json_opt) return config;
    
    auto section = getSection(*json_opt, "event_log");
    if (section.is_object()) {
        config.log_directory = section.value("log_directory", std::string{"data/cognition_logs"});
        config.max_file_size_mb = section.value("max_file_size_mb", 100u);
        config.max_files = section.value("max_files", 1000u);
        config.compress = section.value("compress", true);
        config.retention_days_hours = section.value("retention_days_hours", 720u);
        config.buffer_size = section.value("buffer_size", 10000u);
        config.flush_interval_ms = section.value("flush_interval_ms", 100u);
        config.build_index = section.value("build_index", true);
        config.index_flush_interval = section.value("index_flush_interval", 1000u);
    }
    return config;
}

ConfigSchema::MetricsConfigT CognitiveConfigLoader::loadMetricsConfig(const std::string& json_path) {
    ConfigSchema::MetricsConfigT config;
    auto json_opt = loadJsonFile(json_path);
    if (!json_opt) return config;
    
    auto section = getSection(*json_opt, "metrics");
    if (section.is_object()) {
        config.bind_address = section.value("bind_address", std::string{"0.0.0.0"});
        config.port = section.value("port", 9090u);
        config.metrics_path = section.value("metrics_path", std::string{"/metrics"});
        config.scrape_interval_seconds = section.value("scrape_interval_seconds", 15u);
        config.enable_default_metrics = section.value("enable_default_metrics", true);
        config.max_label_cardinality = section.value("max_label_cardinality", 1000u);
        config.enable_histograms = section.value("enable_histograms", true);
        config.enable_summaries = section.value("enable_summaries", true);
    }
    return config;
}

ConfigSchema::SensorimotorConfigT CognitiveConfigLoader::loadSensorimotorConfig(const std::string& json_path) {
    ConfigSchema::SensorimotorConfigT config;
    auto json_opt = loadJsonFile(json_path);
    if (!json_opt) return config;
    
    auto section = getSection(*json_opt, "sensorimotor");
    if (section.is_object()) {
        config.enable_text = section.value("enable_text", true);
        config.enable_audio = section.value("enable_audio", true);
        config.enable_vision = section.value("enable_vision", true);
        config.enable_screen = section.value("enable_screen", true);
        config.enable_robotics = section.value("enable_robotics", false);
        config.observation_interval_ms = section.value("observation_interval_ms", 100u);
        config.action_timeout_ms = section.value("action_timeout_ms", 5000u);
        config.observation_buffer_size = section.value("observation_buffer_size", 1000u);
    }
    return config;
}

ConfigSchema::GUIAutomationConfigT CognitiveConfigLoader::loadGUIAutomationConfig(const std::string& json_path) {
    ConfigSchema::GUIAutomationConfigT config;
    auto json_opt = loadJsonFile(json_path);
    if (!json_opt) return config;
    
    auto section = getSection(*json_opt, "gui_automation");
    if (section.is_object()) {
        config.enable_uia = section.value("enable_uia", true);
        config.uia_timeout_ms = section.value("uia_timeout_ms", 5000u);
        config.enable_uia_cache = section.value("enable_uia_cache", true);
        config.cache_ttl_ms = section.value("cache_ttl_ms", 1000u);
        config.enable_ocr = section.value("enable_ocr", true);
        config.ocr_language = section.value("ocr_language", std::string{"eng"});
        config.ocr_dpi = section.value("ocr_dpi", 300u);
        config.ocr_confidence_threshold = section.value("ocr_confidence_threshold", 0.7f);
        config.enable_visual_grounding = section.value("enable_visual_grounding", true);
        config.grounding_iou_threshold = section.value("grounding_iou_threshold", 0.5f);
        config.default_click_delay_ms = section.value("default_click_delay_ms", 50u);
        config.default_type_delay_ms = section.value("default_type_delay_ms", 20u);
        config.action_timeout_ms = section.value("action_timeout_ms", 10000u);
        config.enable_element_cache = section.value("enable_element_cache", true);
        config.cache_size = section.value("cache_size", 1000u);
    }
    return config;
}

ConfigSchema::WorldModelConfigT CognitiveConfigLoader::loadWorldModelConfig(const std::string& json_path) {
    ConfigSchema::WorldModelConfigT config;
    auto json_opt = loadJsonFile(json_path);
    if (!json_opt) return config;
    
    auto section = getSection(*json_opt, "world_model");
    if (section.is_object()) {
        config.latent_dim = section.value("latent_dim", 256u);
        config.deterministic_dim = section.value("deterministic_dim", 256u);
        config.stochastic_dim = section.value("stochastic_dim", 32u);
        config.discrete_categories = section.value("discrete_categories", 32u);
        config.encoder_layers = section.value("encoder_layers", 4u);
        config.decoder_layers = section.value("decoder_layers", 4u);
        config.rssm_layers = section.value("rssm_layers", 2u);
        config.hidden_dim = section.value("hidden_dim", 1024u);
        config.dropout = section.value("dropout", 0.1f);
        config.learning_rate = section.value("learning_rate", 0.0003f);
        config.kl_weight = section.value("kl_weight", 1.0f);
        config.free_nats = section.value("free_nats", 3.0f);
        config.batch_size = section.value("batch_size", 32u);
        config.sequence_length = section.value("sequence_length", 50u);
        config.grad_clip = section.value("grad_clip", 100.0f);
        config.imagination_horizon = section.value("imagination_horizon", 15u);
        config.imagination_samples = section.value("imagination_samples", 10u);
        config.text_weight = section.value("text_weight", 1.0f);
        config.image_weight = section.value("image_weight", 1.0f);
        config.reward_weight = section.value("reward_weight", 1.0f);
    }
    return config;
}

ConfigSchema::SimulatorConfigT CognitiveConfigLoader::loadSimulatorConfig(const std::string& json_path) {
    ConfigSchema::SimulatorConfigT config;
    auto json_opt = loadJsonFile(json_path);
    if (!json_opt) return config;
    
    auto section = getSection(*json_opt, "simulator");
    if (section.is_object()) {
        config.enable_physics = section.value("enable_physics", true);
        config.gravity = section.value("gravity", -9.81f);
        config.timestep = section.value("timestep", 0.016666667f);
        config.substeps = section.value("substeps", 4u);
        config.enable_rendering = section.value("enable_rendering", false);
        config.render_width = section.value("render_width", 800u);
        config.render_height = section.value("render_height", 600u);
        config.max_agents = section.value("max_agents", 10u);
        config.enable_tool_simulation = section.value("enable_tool_simulation", true);
        config.record_trajectories = section.value("record_trajectories", true);
        config.max_trajectory_length = section.value("max_trajectory_length", 10000u);
    }
    return config;
}

} // namespace infra
} // namespace brain
} // namespace yuki