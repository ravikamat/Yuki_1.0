#include "ExecutivePolicySelector.h"
#include "infrastructure/GlobalWorkspace.h"
#include "brain/self/SelfModel.h"
#include "brain/emotion/EmotionSystem.h"
#include "brain/memory/HeuristicMemory.h"
#include "brain/system/SystemController.h"
#include "core/ConfigManager.h"
#include "brain/tools/ToolRegistry.h"
#include "brain/core/Logger.h"
#include "brain/ync/YNCPipelineBridge.h"
#include "brain/ync/YNCTrainingSupervisor.h"
#include "brain/ync/NeuromorphicSimulator.h"
#include <cmath>
#include <algorithm>
#include <chrono>

#include "brain/ethics/ValueConstitution.h"

namespace yuki {
namespace policy {

// Destructor defined here so unique_ptr<LearnedEnsemblePolicy> can see the full type.
ExecutivePolicySelector::~ExecutivePolicySelector() = default;

ExecutivePolicySelector::ExecutivePolicySelector(const metacognition::CompetenceRecord* competence)
    : competence_(competence) {
    if (!competence_) {
        threshold_ = THRESHOLD_MAX;
    }
}

void ExecutivePolicySelector::setSelfModel(yuki::self::SelfModel* ptr) {
    self_model_.reset(ptr);
}

void ExecutivePolicySelector::setValenceArousalModel(yuki::emotion::ValenceArousalModel* ptr) {
    valence_arousal_.reset(ptr);
}

void ExecutivePolicySelector::setValueConstitution(yuki::ethics::ValueConstitution* ptr) {
    value_constitution_ = ptr;
}

PolicySelection ExecutivePolicySelector::select(const std::vector<float>& intent_distribution,
                                       const std::string& /*raw_text*/,
                                       uint32_t competence_domain) const {
    PolicySelection result;
    result.competence_domain = competence_domain;

    float effective_threshold = threshold_;
    if (valence_arousal_) {
        effective_threshold = valence_arousal_->modulateThreshold(threshold_);
    }
    result.competence_threshold = effective_threshold;

    if (self_model_) {
        float stability = self_model_->identityStability();
        // Reduce threshold when identity is unstable (more cautious)
        if (stability < 0.5f) effective_threshold *= (1.0f + (0.5f - stability));
    }

    if (!competence_ || competence_domain >= static_cast<uint32_t>(metacognition::CompetenceDomain::COUNT)) {
        result.execution_mode = ExecutionMode::CLARIFY;
        result.domain_competence = 0.5f;
        return result;
    }

    result.domain_competence = competence_[competence_domain].success_rate_ema;

    float entropy = 0.0f;
    float max_score = 0.0f;
    int max_idx = -1;

    for (size_t i = 0; i < intent_distribution.size(); ++i) {
        float p = intent_distribution[i];
        if (p > 0.0f) {
            entropy -= p * std::log(p);
        }
        if (p > max_score) {
            max_score = p;
            max_idx = static_cast<int>(i);
        }
    }

    result.selected_policy_id = max_idx;
    result.selection_confidence = max_score;

    auto& cfg = ConfigManager::instance();
    const float deferMult   = cfg.getFloat("policy_gates", "defer_threshold_multiplier", 0.5f);
    const float learnMult   = cfg.getFloat("policy_gates", "learn_threshold_multiplier", 1.0f);
    const float clarEntropy = cfg.getFloat("policy_gates", "clarify_entropy_threshold", 1.0f);
    const float clarMaxScore= cfg.getFloat("policy_gates", "clarify_max_score_threshold", 0.5f);

    if (result.domain_competence < effective_threshold * deferMult) {
        result.execution_mode = ExecutionMode::DEFER;
    } else if (result.domain_competence < effective_threshold * learnMult) {
        result.execution_mode = ExecutionMode::LEARN;
    } else if (entropy > clarEntropy || max_score < clarMaxScore) {
        result.execution_mode = ExecutionMode::CLARIFY;
    } else {
        result.execution_mode = ExecutionMode::EXECUTE;
    }

    // ── Task 2.1: YNC-First Policy Selection with DQN as Critic ───────────────
    // Add domain tracking: Map competence domain to domain string and ExecutionMode
    std::string domain_str = domainToString(competence_domain);
    ExecutionMode domain_mode = domainToExecutionMode(competence_domain);

    if (ync_bridge_ && ync_sim_) {
        auto ync_intuition = ync_bridge_->readIntuition(*ync_sim_);

        // YNC override threshold: read from config. If YNC confidence below threshold,
        // fall back to DQN critic and log divergence.
        const float ync_conf_threshold = yuki::ConfigManager::instance().getFloat(
            "ync_policy", "confidence_threshold", 0.6f);
        if (ync_intuition.confidence < ync_conf_threshold) {
            yuki::core::Logger::instance().log(
                yuki::core::LogLevel::WARN, "ExecutivePolicySelector",
                "YNC confidence low (" + std::to_string(ync_intuition.confidence) +
                " < " + std::to_string(ync_conf_threshold) + ") for domain " + domain_str +
                " — falling back to DQN critic.");

            if (divergence_logger_) {
                divergence_logger_->log(
                    static_cast<uint8_t>(result.execution_mode),
                    static_cast<uint8_t>(ync_intuition.suggested_mode),
                    ync_intuition.confidence);
            }

            // DQN critic selection
            if (learned_policy_ && learned_policy_->isTrained()) {
                EnsembleFeatures features;
                features.uncertainty = std::clamp(entropy / 2.3f, 0.0f, 1.0f);
                features.competence_ema = result.domain_competence;
                features.surprise = ync_intuition.arousal;
                auto dqn_decision = learned_policy_->decide(features);
                result.execution_mode = dqn_decision.mode;
                result.selection_confidence = dqn_decision.confidence;
            }
        } else {
            // YNC confidence >= threshold: Check if YNC is trusted for this domain
            bool trusted = ync_trainer_ ? ync_trainer_->isTrusted(domain_mode) : false;
            if (trusted) {
                // YNC in control
                result.execution_mode = ync_intuition.suggested_mode;
                result.selection_confidence = ync_intuition.confidence;
                yuki::core::Logger::instance().log(
                    yuki::core::LogLevel::DEBUG, "ExecutivePolicySelector",
                    "YNC trusted for domain " + domain_str + " (mode=" +
                    std::to_string(static_cast<int>(result.execution_mode)) + ", conf=" +
                    std::to_string(ync_intuition.confidence) + ")");
            } else {
                // YNC explores / DQN critic in control
                if (learned_policy_ && learned_policy_->isTrained()) {
                    EnsembleFeatures features;
                    features.uncertainty = std::clamp(entropy / 2.3f, 0.0f, 1.0f);
                    features.competence_ema = result.domain_competence;
                    features.surprise = ync_intuition.arousal;
                    auto dqn_decision = learned_policy_->decide(features);
                    result.execution_mode = dqn_decision.mode;
                    result.selection_confidence = dqn_decision.confidence;
                }
                // Record outcome / episode for YNC training
                if (ync_trainer_) {
                    ync::TrainingEpisode ep{};
                    ep.pipeline_decision = result.execution_mode;
                    ep.pipeline_used_ync = false;
                    ep.outcome_success = (result.execution_mode == ExecutionMode::EXECUTE);
                    ep.user_satisfaction = result.selection_confidence;
                    ep.timestamp = static_cast<uint64_t>(
                        std::chrono::steady_clock::now().time_since_epoch().count());
                    ync_trainer_->recordEpisode(ep);
                }
            }
        }
    } else {
        // Fallback when YNC is not initialized
        applyEnsembleAdvisory(result, entropy, result.domain_competence);
    }

    return result;
}

PolicySelection ExecutivePolicySelector::selectConstrained(
    const std::vector<float>& intent_distribution,
    const std::string& raw_text,
    uint32_t competence_domain,
    const ExecutionConstraints& constraints) const {

    PolicySelection result = select(intent_distribution, raw_text, competence_domain);

    // Hardware profile from config (operator-tuneable for the deployment environment).
    const double hw_throughput_gflops = static_cast<double>(
        yuki::ConfigManager::instance().getFloat("hardware", "throughput_gflops", 100.0f));  // e.g. 100 GFLOPS for a mid-range CPU
    const float  hw_power_watts = yuki::ConfigManager::instance().getFloat(
        "hardware", "power_draw_watts", 65.0f);  // TDP of host CPU
    const int    avg_seq_len = yuki::ConfigManager::instance().getInt(
        "model", "avg_sequence_length", 512);    // Average number of tokens per inference

    if (constraints.fast_native_action_available) {
        // Native path: cost is the YNC neuromorphic simulation, not an LLM.
        // FLOPs ≈ num_neurons × avg_synapses_per_neuron × sim_steps
        //   avg_synapses = connectivity_density × num_neurons
        //   sim_steps    = sleep_steps_inner from config (same budget as sleep training)
        // This uses the actual SimulatorConfig values if ync_sim_ is wired, else config.
        const int sim_steps = yuki::ConfigManager::instance().getInt(
            "ync_training", "sleep_steps_inner", 100);
        const float connectivity = yuki::ConfigManager::instance().getFloat(
            "ync_sim", "connectivity_density", 0.05f);
        const int num_neurons = yuki::ConfigManager::instance().getInt(
            "ync_sim", "num_neurons", 10000);

        const double avg_synapses = static_cast<double>(num_neurons) * connectivity;
        result.estimated_flops = static_cast<double>(num_neurons) * avg_synapses * sim_steps;

        // Latency (ms) = FLOPs / (throughput_GFLOPS × 1e9) × 1000
        const double throughput_flops_per_ms = hw_throughput_gflops * 1e6;  // GFLOPS → FLOPS/ms
        result.estimated_latency_ms = static_cast<float>(
            result.estimated_flops / throughput_flops_per_ms);

        // Energy (J) = power (W) × latency (s)
        result.estimated_energy_joules = hw_power_watts *
            (result.estimated_latency_ms / 1000.0f);

        if (result.estimated_latency_ms <= constraints.max_latency_ms &&
            result.estimated_energy_joules <= constraints.max_energy_joules) {
            result.execution_mode = ExecutionMode::EXECUTE;
            result.selection_confidence = std::max(result.selection_confidence, 0.95f);
        }
    } else {
        // LLM path: standard transformer forward-pass FLOPs formula.
        // FLOPs ≈ 2 × N_params × N_tokens  (Kaplan et al. 2020, "Scaling Laws for Neural LMs")
        // When target_model_params == 0 (no LLM, fallback rule-based path), use a
        // symbolic inference estimate: 2 × intent_dim × avg_seq_len.
        const double n_params = (constraints.target_model_params > 0)
            ? static_cast<double>(constraints.target_model_params)
            : static_cast<double>(intent_distribution.size()) * 256.0;  // symbolic 256-dim rule net

        result.estimated_flops = 2.0 * n_params * static_cast<double>(avg_seq_len);

        // Latency (ms) = FLOPs / (throughput_GFLOPS × 1e9) × 1000
        const double throughput_flops_per_ms = hw_throughput_gflops * 1e6;
        result.estimated_latency_ms = static_cast<float>(
            result.estimated_flops / throughput_flops_per_ms);

        // Energy (J) = TDP (W) × latency (s)
        result.estimated_energy_joules = hw_power_watts *
            (result.estimated_latency_ms / 1000.0f);

        if (result.estimated_latency_ms > constraints.max_latency_ms ||
            result.estimated_energy_joules > constraints.max_energy_joules) {
            if (result.execution_mode == ExecutionMode::EXECUTE) {
                result.execution_mode = ExecutionMode::CLARIFY;
            }
        }
    }

    return result;
}


void ExecutivePolicySelector::adaptThreshold(float global_competence_trend) {
    global_trend_ema_ = (1.0f - THRESHOLD_ALPHA) * global_trend_ema_ + THRESHOLD_ALPHA * global_competence_trend;
    threshold_ = THRESHOLD_MIN + (THRESHOLD_MAX - THRESHOLD_MIN) * (1.0f - global_trend_ema_);
    threshold_ = std::clamp(threshold_, THRESHOLD_MIN, THRESHOLD_MAX);
}

float ExecutivePolicySelector::computeRiskAdjustedThreshold(float baseThreshold, float riskScore) const {
    float adjusted = baseThreshold * (1.0f + riskScore * 0.5f);
    return std::clamp(adjusted, THRESHOLD_MIN, THRESHOLD_MAX);
}

bool ExecutivePolicySelector::requiresApproval(const std::string& action, float riskScore) const {
    if (riskScore >= 0.7f) return true;
    if (action.find("delete") != std::string::npos || action.find("execute") != std::string::npos) {
        return riskScore >= 0.3f;
    }
    return false;
}

float ExecutivePolicySelector::computeActionRiskAdjustedThreshold(float baseThreshold, float riskAggregate) const {
    float adjusted = baseThreshold * (1.0f + riskAggregate * 0.75f);
    return std::clamp(adjusted, THRESHOLD_MIN, THRESHOLD_MAX);
}

bool ExecutivePolicySelector::requiresHumanApprovalForAction(const std::string& actionType, float riskScore) const {
    if (actionType.find("DELETE") != std::string::npos ||
        actionType.find("SYSTEM_COMMAND") != std::string::npos) {
        return true;
    }
    return riskScore >= 0.5f;
}

// ── PACL Phase 7: Setters ─────────────────────────────────────────────────────

void ExecutivePolicySelector::setLearnedPolicy(
    std::unique_ptr<LearnedEnsemblePolicy> policy) noexcept {
    learned_policy_ = std::move(policy);
}

void ExecutivePolicySelector::setDivergenceLogger(
    metacognition::PolicyDivergenceLogger* logger) noexcept {
    divergence_logger_ = logger;
}

void ExecutivePolicySelector::setYNC(ync::YNCPipelineBridge* bridge,
                                      ync::YNCTrainingSupervisor* trainer,
                                      ync::NeuromorphicSimulator* sim) noexcept {
    ync_bridge_  = bridge;
    ync_trainer_ = trainer;
    ync_sim_     = sim;
}

std::string ExecutivePolicySelector::domainToString(uint32_t competence_domain) {
    switch (static_cast<metacognition::CompetenceDomain>(competence_domain)) {
        case metacognition::CompetenceDomain::INTENT_QUESTION:  return "QUESTION";
        case metacognition::CompetenceDomain::INTENT_COMMAND:   return "COMMAND";
        case metacognition::CompetenceDomain::INTENT_EMOTIONAL: return "EMOTIONAL";
        case metacognition::CompetenceDomain::INTENT_TECHNICAL: return "TECHNICAL";
        case metacognition::CompetenceDomain::INTENT_URGENCY:   return "URGENCY";
        case metacognition::CompetenceDomain::INTENT_GREETING:  return "GREETING";
        case metacognition::CompetenceDomain::INTENT_ACTION:    return "ACTION";
        case metacognition::CompetenceDomain::INTENT_POLARITY:  return "POLARITY";
        case metacognition::CompetenceDomain::META_PRECISION:  return "META_PRECISION";
        case metacognition::CompetenceDomain::META_RETRIEVAL:  return "META_RETRIEVAL";
        case metacognition::CompetenceDomain::META_SYNTHESIS:  return "META_SYNTHESIS";
        default: return "GENERAL";
    }
}

ExecutionMode ExecutivePolicySelector::domainToExecutionMode(uint32_t competence_domain) {
    switch (static_cast<metacognition::CompetenceDomain>(competence_domain)) {
        case metacognition::CompetenceDomain::INTENT_QUESTION:  return ExecutionMode::CLARIFY;
        case metacognition::CompetenceDomain::INTENT_COMMAND:   return ExecutionMode::EXECUTE;
        case metacognition::CompetenceDomain::INTENT_EMOTIONAL: return ExecutionMode::LEARN;
        case metacognition::CompetenceDomain::INTENT_TECHNICAL: return ExecutionMode::EXECUTE;
        case metacognition::CompetenceDomain::INTENT_URGENCY:   return ExecutionMode::EXECUTE;
        case metacognition::CompetenceDomain::INTENT_GREETING:  return ExecutionMode::EXECUTE;
        case metacognition::CompetenceDomain::INTENT_ACTION:    return ExecutionMode::EXECUTE;
        case metacognition::CompetenceDomain::INTENT_POLARITY:  return ExecutionMode::CLARIFY;
        default: return ExecutionMode::EXECUTE;
    }
}

ExecutionMode ExecutivePolicySelector::intentToExecutionMode(IntentType intent) {
    switch (intent) {
        case IntentType::TASK_COMMAND:
        case IntentType::DIRECT_ACTION:
            return ExecutionMode::EXECUTE;
        case IntentType::QUERY:
        case IntentType::DISAMBIGUATION:
            return ExecutionMode::CLARIFY;
        case IntentType::GREETING:
            return ExecutionMode::EXECUTE;
        default:
            return ExecutionMode::DEFER;
    }
}

std::string ExecutivePolicySelector::intentToDomainString(IntentType intent) {
    switch (intent) {
        case IntentType::TASK_COMMAND:  return "TASK_COMMAND";
        case IntentType::DIRECT_ACTION: return "DIRECT_ACTION";
        case IntentType::QUERY:         return "QUERY";
        case IntentType::GREETING:      return "GREETING";
        case IntentType::DISAMBIGUATION:return "DISAMBIGUATION";
        default:                        return "UNKNOWN";
    }
}

// ── PACL Phase 7: Ensemble Advisory ──────────────────────────────────────────
//
// Advisory-only: this function NEVER changes result.execution_mode.
// It only logs divergence when the ensemble strongly disagrees.
// The legacy result is always returned to the caller.
bool ExecutivePolicySelector::applyEnsembleAdvisory(PolicySelection& result,
                                            float entropy,
                                            float domain_competence) const {
    if (!learned_policy_ || !learned_policy_->isTrained()) {
        return false; // No-op: keep pre-PACL behaviour exactly.
    }

    // Build EnsembleFeatures from available context.
    EnsembleFeatures features;
    features.uncertainty    = std::clamp(entropy / 2.3f, 0.0f, 1.0f); // normalise by ln(10)
    features.competence_ema = domain_competence;

    // Enrich from the last CognitiveMoment if GlobalWorkspace has one.
    auto moment = yuki::gw::GlobalWorkspace::instance().peek();
    if (moment.valid) {
        features.valence      = moment.emotional_valence;
        features.arousal      = moment.arousal;
        features.uncertainty  = moment.uncertainty; // overwrite entropy-based estimate with real
    }

    EnsembleDecision decision = learned_policy_->decide(features);

    // Fast-path: high confidence + agreement — result unchanged, no logging needed.
    static constexpr float kHighConfidenceAgree    = 0.75f;
    static constexpr float kHighConfidenceDisagree = 0.85f;

    if (decision.confidence > kHighConfidenceDisagree
        && static_cast<uint8_t>(decision.mode) != static_cast<uint8_t>(result.execution_mode)) {
        // Strong disagreement — log but do NOT change result (advisory only until M9).
        if (divergence_logger_) {
            divergence_logger_->log(
                static_cast<uint8_t>(result.execution_mode),
                static_cast<uint8_t>(decision.mode),
                decision.confidence);
        }
        return false; // Legacy wins.
    }

    // Agreement or low confidence — no action needed.
    return decision.confidence > kHighConfidenceAgree
        && static_cast<uint8_t>(decision.mode) == static_cast<uint8_t>(result.execution_mode);
}

void ExecutivePolicySelector::setSystemController(yuki::system::SystemController* ptr) {
    system_controller_.reset(ptr);
}

bool ExecutivePolicySelector::requiresSystemCapability(uint32_t action_type) const {
    // ActionType values matching system commands: 10+, screenshot, volume, app launch, url launch
    return (action_type >= 10);
}

Policy ExecutivePolicySelector::select(IntentType intent,
                                        const SlotMap& slots,
                                        float confidence,
                                        bool last_turn_was_clarification) const {
    if (is_dangerous_intent(intent, slots)) {
        return Policy::POLICY_DEFER;
    }

    // P0 FIX #1: ValueConstitution ethical gate — evaluate action against Gita principles
    if (value_constitution_) {
        std::string action_desc;
        if (slots.count("verb")) action_desc += slots.at("verb") + " ";
        if (slots.count("entity")) action_desc += slots.at("entity");
        if (action_desc.empty() && slots.count("raw_input"))
            action_desc = slots.at("raw_input").substr(0, 80);

        if (!action_desc.empty()) {
            auto report = value_constitution_->evaluate(action_desc, {});
            if (report.alignment_score < -0.5f) {
                std::cout << "[PolicySelector] ValueConstitution DEFER: alignment="
                          << report.alignment_score << "\n";
                return Policy::POLICY_DEFER;
            }
            if (!report.warnings.empty()) {
                std::cout << "[PolicySelector] ValueConstitution WARNING: "
                          << report.warnings[0] << "\n";
            }
        }
    }

    // ── Task 2.1: YNC-First Policy Logic in Intent-Based Select ───────────────
    if (ync_bridge_ && ync_sim_) {
        auto ync_intuition = ync_bridge_->readIntuition(*ync_sim_);
        ExecutionMode domain_mode = intentToExecutionMode(intent);
        std::string domain_str = intentToDomainString(intent);

        const float ync_conf_threshold = yuki::ConfigManager::instance().getFloat(
            "ync_policy", "confidence_threshold", 0.6f);
        const float action_conf_threshold = yuki::ConfigManager::instance().getFloat(
            "policy", "action_confidence_threshold", 0.40f);

        if (ync_intuition.confidence < ync_conf_threshold) {
            yuki::core::Logger::instance().log(
                yuki::core::LogLevel::WARN, "ExecutivePolicySelector",
                "YNC confidence low (" + std::to_string(ync_intuition.confidence) +
                " < " + std::to_string(ync_conf_threshold) +
                ") for intent domain " + domain_str + " — falling back to DQN critic.");
            if (divergence_logger_) {
                divergence_logger_->log(
                    static_cast<uint8_t>(ExecutionMode::EXECUTE),
                    static_cast<uint8_t>(ync_intuition.suggested_mode),
                    ync_intuition.confidence);
            }
        } else {
            bool trusted = ync_trainer_ ? ync_trainer_->isTrusted(domain_mode) : false;
            if (trusted) {
                yuki::core::Logger::instance().log(
                    yuki::core::LogLevel::DEBUG, "ExecutivePolicySelector",
                    "YNC trusted for intent domain " + domain_str + " (suggested=" +
                    std::to_string(static_cast<int>(ync_intuition.suggested_mode)) + ")");

                switch (ync_intuition.suggested_mode) {
                    case ExecutionMode::EXECUTE:
                        if (is_direct_action(intent) && has_all_required_slots(slots, intent))
                            return Policy::DIRECT_SLOT_RESPONSE;
                        return Policy::TASK_EXECUTION;
                    case ExecutionMode::CLARIFY:
                        return Policy::DISAMBIGUATION;
                    case ExecutionMode::LEARN:
                        return Policy::HYBRID_RESPONSE;
                    case ExecutionMode::DEFER:
                        return Policy::POLICY_DEFER;
                    case ExecutionMode::RESEARCH:
                        return Policy::HYBRID_RESPONSE;
                    case ExecutionMode::SYNTHESIZE:
                        return Policy::DIRECT_SLOT_RESPONSE;
                    case ExecutionMode::SLEEP:
                        return Policy::POLICY_DEFER;
                    default:
                        break;
                }
            } else {
                if (ync_trainer_) {
                    ync::TrainingEpisode ep{};
                    ep.pipeline_decision = ExecutionMode::CLARIFY;
                    ep.pipeline_used_ync = false;
                    ep.outcome_success = (confidence >= action_conf_threshold);
                    ep.user_satisfaction = confidence;
                    ep.timestamp = static_cast<uint64_t>(
                        std::chrono::steady_clock::now().time_since_epoch().count());
                    ync_trainer_->recordEpisode(ep);
                }
            }
        }
    }

    if (confidence >= 0.85f &&
        is_direct_action(intent) &&
        has_all_required_slots(slots, intent) &&
        !has_conflicting_slots(slots)) {
        return Policy::DIRECT_SLOT_RESPONSE;
    }

    if (confidence >= 0.40f && is_action_intent(intent)) {
        auto tool = ToolRegistry::instance().resolve(intent, slots);

        if (tool && tool->capability_score(slots) >= 0.30f) {
            bool wantsExplanation = slots.count("explain") || last_turn_was_clarification;

            if (wantsExplanation) {
                return Policy::HYBRID_RESPONSE;
            }
            return Policy::TASK_EXECUTION;
        }

        if (tool && tool->capability_score(slots) < 0.30f) {
            return Policy::LLM_GENERATION;
        }
    }

    if (is_action_intent(intent) && confidence < 0.40f) {
        return Policy::DISAMBIGUATION;
    }
    if (is_action_intent(intent) && !has_all_required_slots(slots, intent)) {
        return Policy::DISAMBIGUATION;
    }

    return Policy::LLM_GENERATION;
}

bool ExecutivePolicySelector::is_dangerous_intent(IntentType intent, const SlotMap& slots) {
    // Check intent type first — some intents are inherently dangerous
    if (intent == IntentType::DIRECT_ACTION || intent == IntentType::TASK_COMMAND) {
        // Direct actions need slot-level checking below
    } else {
        return false; // Non-action intents can't be dangerous
    }
    static const std::unordered_set<std::string> kDangerousVerbs = {
        "format", "wipe", "delete_system", "rm_rf", "disable_firewall",
        "modify_registry_unsafe", "execute_unknown", "chmod_system",
        "kill_process_system", "uninstall_critical"
    };

    if (slots.count("verb") && kDangerousVerbs.count(slots.at("verb"))) {
        return true;
    }

    bool hasElevated = slots.count("privilege") && 
                       (slots.at("privilege") == "admin" || slots.at("privilege") == "sudo");
    bool isDestructive = slots.count("verb") &&
                         (slots.at("verb") == "delete" || slots.at("verb") == "remove" ||
                          slots.at("verb") == "modify" || slots.at("verb") == "overwrite");

    if (hasElevated && isDestructive) {
        if (slots.count("target")) {
            std::string target = slots.at("target");
            if (target.find("/System") != std::string::npos ||
                target.find("C:\\Windows") != std::string::npos ||
                target.find("/usr/bin") != std::string::npos ||
                target.find("registry") != std::string::npos) {
                return true;
            }
        }
    }

    if (slots.count("verb") && slots.at("verb") == "delete" && slots.count("recursive") && !slots.count("confirmed")) {
        return true;
    }

    return false;
}

bool ExecutivePolicySelector::is_direct_action(IntentType intent) {
    return intent == IntentType::DIRECT_ACTION || intent == IntentType::TASK_COMMAND;
}

bool ExecutivePolicySelector::is_action_intent(IntentType intent) {
    return intent == IntentType::DIRECT_ACTION || intent == IntentType::TASK_COMMAND;
}

bool ExecutivePolicySelector::has_all_required_slots(const SlotMap& slots, IntentType intent) {
    if (slots.empty()) return false;
    // Direct actions require at least a verb; task commands require verb + target
    if (intent == IntentType::TASK_COMMAND)
        return slots.count("verb") && slots.count("target");
    if (intent == IntentType::DIRECT_ACTION)
        return slots.count("verb") > 0;
    return true;
}

bool ExecutivePolicySelector::has_conflicting_slots(const SlotMap& slots) {
    if (slots.count("action") && slots.count("negation")) {
        return true;
    }
    return false;
}

} // namespace policy
} // namespace yuki
