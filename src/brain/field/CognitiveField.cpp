#include "brain/field/CognitiveField.h"
#include "brain/field/FieldScheduler.h"
#include "brain/field/CognitiveSnapshot.h"
#include "brain/core/Logger.h"
#include "brain/inference/VariationalStateEstimator.h"
#include "brain/inference/GenerativeModel.h"
#include "brain/memory/CognitiveMemoryFabric.h"
#include "brain/memory/EpisodicStore.h"
#include "brain/memory/HdcSemanticGraph.h"
#include "brain/language/BrocaTranslator.h"
#include "brain/language/WernickeComprehender.h"
#include "brain/language/PhonologicalLoop.h"
// #include "brain/policy/ActiveInferencePolicyEngine.h"  // Moved to not_in_use - optional
#include "brain/organism/DriveSystem.h"
#include "brain/self/SelfModel.h"
#include "brain/ync/NeuromorphicSimulator.h"
#include "brain/core/HierarchicalPredictiveCoding.h"
#include "brain/memory/AwakeConsolidationEngine.h"
#include "brain/policy/ActiveInferencePolicyEngine.h"
#include "brain/organism/ContinuousDriveSystem.h"
#include "brain/autonomy/CuriosityEngine.h"
#include <algorithm>
#include <chrono>

namespace yuki {
namespace brain {
namespace field {

// Forward declare WernickeInput type for use in callbacks
using WernickeInput = ::yuki::brain::language::WernickeInput;

CognitiveField::CognitiveField(const CognitiveFieldConfig& cfg)
    : config_(cfg), scheduler_(cfg.scheduler) {
    yuki::core::Logger::instance().log(yuki::core::LogLevel::INFO, "CognitiveField", "Created");
}

CognitiveField::~CognitiveField() {
    stop();
}

CognitiveField::CognitiveField(CognitiveField&& other) noexcept
    : config_(other.config_),
      scheduler_(std::move(other.scheduler_)),
      running_(other.running_.load()),
      thought_ready_(other.thought_ready_.load()),
      new_user_turn_(other.new_user_turn_.load()),
      current_thought_(std::move(other.current_thought_)),
      vse_(other.vse_),
      generative_model_(other.generative_model_),
      cmf_(other.cmf_),
      episodic_store_(other.episodic_store_),
      hdc_(other.hdc_),
      broca_(other.broca_),
      wernicke_(other.wernicke_),
      phonological_loop_(other.phonological_loop_),
      // policy_engine_(other.policy_engine_),  // Moved to not_in_use
      drive_system_(other.drive_system_),
      self_model_(other.self_model_),
      ync_(other.ync_),
      hpc_(other.hpc_),
      awake_consolidation_(other.awake_consolidation_),
      policy_engine_(other.policy_engine_) {}

CognitiveField& CognitiveField::operator=(CognitiveField&& other) noexcept {
    if (this != &other) {
        stop();
        config_ = other.config_;
        scheduler_ = std::move(other.scheduler_);
        running_ = other.running_.load();
        thought_ready_ = other.thought_ready_.load();
        new_user_turn_ = other.new_user_turn_.load();
        current_thought_ = std::move(other.current_thought_);
        vse_ = other.vse_;
        generative_model_ = other.generative_model_;
        cmf_ = other.cmf_;
        episodic_store_ = other.episodic_store_;
        hdc_ = other.hdc_;
        broca_ = other.broca_;
        wernicke_ = other.wernicke_;
        phonological_loop_ = other.phonological_loop_;
        // policy_engine_ = other.policy_engine_;  // Moved to not_in_use
        drive_system_ = other.drive_system_;
        self_model_ = other.self_model_;
        ync_ = other.ync_;
        hpc_ = other.hpc_;
        awake_consolidation_ = other.awake_consolidation_;
        policy_engine_ = other.policy_engine_;
    }
    return *this;
}

// ─────────────────────────────────────────────────────────────────────────────
// Wiring
// ─────────────────────────────────────────────────────────────────────────────

void CognitiveField::setVSE(inference::VariationalStateEstimator* vse) { vse_ = vse; }
void CognitiveField::setGenerativeModel(inference::GenerativeModel* gm) { generative_model_ = gm; }
void CognitiveField::setMemoryFabric(memory::CognitiveMemoryFabric* cmf) { cmf_ = cmf; }
void CognitiveField::setEpisodicStore(memory::EpisodicStore* es) { episodic_store_ = es; }
void CognitiveField::setHdcGraph(::yuki::memory::HdcSemanticGraph* hdc) { hdc_ = hdc; }
void CognitiveField::setBrocaTranslator(language::BrocaTranslator* broca) { broca_ = broca; }
void CognitiveField::setWernickeComprehender(language::WernickeComprehender* wernicke) { wernicke_ = wernicke; }
void CognitiveField::setPhonologicalLoop(language::PhonologicalLoop* pl) { phonological_loop_ = pl; }
// void CognitiveField::setPolicyEngine(policy::ActiveInferencePolicyEngine* pe) { policy_engine_ = pe; }  // Moved to not_in_use
void CognitiveField::setDriveSystem(organism::ContinuousDriveSystem* ds) { drive_system_ = ds; }
void CognitiveField::setSelfModel(self::SelfModel* sm) { self_model_ = sm; }
void CognitiveField::setYNC(::ync::NeuromorphicSimulator* ync) { ync_ = ync; }

void CognitiveField::setHPC(yuki::brain::core::HierarchicalPredictiveCoding* hpc) { hpc_ = hpc; }
void CognitiveField::setAwakeConsolidation(yuki::brain::memory::AwakeConsolidationEngine* ace) { awake_consolidation_ = ace; }
void CognitiveField::setPolicyEngine(yuki::brain::policy::ActiveInferencePolicyEngine* pe) { policy_engine_ = pe; }

void CognitiveField::setLatentWorldModel(yuki::brain::predictive::LatentWorldModel* lwm) { latent_world_model_ = lwm; }
void CognitiveField::setCausalWorldModel(::yuki::brain::causality::CausalWorldModel* cwm) { causal_world_model_ = cwm; }
void CognitiveField::setCuriosityEngine(autonomy::CuriosityEngine* ce) { curiosity_engine_ = ce; }

// ─────────────────────────────────────────────────────────────────────────────
// Control
// ─────────────────────────────────────────────────────────────────────────────

void CognitiveField::start() {
    if (running_.exchange(true)) return;
    
    setupCallbacks();
    scheduler_.start();
    
    if (phonological_loop_) phonological_loop_->start();
    
    yuki::core::Logger::instance().log(yuki::core::LogLevel::INFO, "CognitiveField", "Started");
}

void CognitiveField::stop() {
    if (!running_.exchange(false)) return;
    
    scheduler_.stop();
    
    if (phonological_loop_) phonological_loop_->stop();
    
    yuki::core::Logger::instance().log(yuki::core::LogLevel::INFO, "CognitiveField", "Stopped");
}

bool CognitiveField::isRunning() const {
    return running_.load();
}

// ─────────────────────────────────────────────────────────────────────────────
// Sensory injection
// ─────────────────────────────────────────────────────────────────────────────

bool CognitiveField::injectText(const std::string& text, const std::vector<float>& embedding) {
    if (!running_.load()) return false;
    
    // 1. Capture user input IMMEDIATELY into state (before scheduler)
    last_user_text_ = text;
    new_user_turn_.store(true, std::memory_order_release);
    thought_ready_.store(false, std::memory_order_release);
    
    if (!embedding.empty()) {
        size_t copy_count = std::min(embedding.size(), last_sensory_embedding.size());
        for (size_t i = 0; i < copy_count; ++i) {
            last_sensory_embedding[i] = embedding[i];
        }
    }
    
    // 2. Push to phonological loop for perception binding
    if (phonological_loop_) {
        std::vector<float> embedding_vec(last_sensory_embedding.begin(), last_sensory_embedding.end());
        phonological_loop_->perceive(text, embedding_vec, "perception");
    }
    
    // 3. Then push to scheduler for async processing
    yuki::brain::field::SensoryEvent event;
    event.event_id = yuki::brain::field::FieldScheduler::nextEventId();
    event.timestamp = std::chrono::steady_clock::now();
    event.modality = "text";
    event.data = text;
    event.embedding = embedding;
    event.target_timescale = yuki::brain::field::Timescale::FAST_10MS;  // Process at phonological level
    event.blocking = false;
    
    return scheduler_.injectSensory(event);
}

bool CognitiveField::injectTextBlocking(const std::string& text, std::chrono::milliseconds timeout) {
    if (!running_.load()) return false;
    
    // 1. Capture user input IMMEDIATELY into state (before scheduler)
    last_user_text_ = text;
    new_user_turn_.store(true, std::memory_order_release);
    thought_ready_.store(false, std::memory_order_release);
    
    // 2. Push to phonological loop for perception binding
    if (phonological_loop_) {
        std::vector<float> embedding_vec(last_sensory_embedding.begin(), last_sensory_embedding.end());
        phonological_loop_->perceive(text, embedding_vec, "perception");
    }
    
    // 3. Then push to scheduler for async processing
    yuki::brain::field::SensoryEvent event;
    event.event_id = yuki::brain::field::FieldScheduler::nextEventId();
    event.timestamp = std::chrono::steady_clock::now();
    event.modality = "text";
    event.data = text;
    event.target_timescale = yuki::brain::field::Timescale::SLOW_1S;  // Wait for belief update
    event.blocking = true;
    
    return scheduler_.injectSensoryBlocking(event, timeout);
}

// ─────────────────────────────────────────────────────────────────────────────
// Thought retrieval
// ─────────────────────────────────────────────────────────────────────────────

bool CognitiveField::waitForThought(std::chrono::milliseconds deadline) {
    std::unique_lock<std::mutex> lock(thought_mtx_);
    auto timeout_time = std::chrono::steady_clock::now() + deadline;
    
    while (running_.load() && !thought_ready_.load()) {
        if (thought_cv_.wait_until(lock, timeout_time) == std::cv_status::timeout) {
            return false;
        }
    }
    
    if (thought_ready_.load()) {
        thought_ready_.store(false);
        return true;
    }
    return false;
}

std::string CognitiveField::getThought() const {
    std::lock_guard<std::mutex> lock(thought_mtx_);
    return current_thought_;
}

CognitiveSnapshot CognitiveField::getSnapshot() const {
    return scheduler_.getMergedSnapshot();
}

// ─────────────────────────────────────────────────────────────────────────────
// Metrics
// ─────────────────────────────────────────────────────────────────────────────

CognitiveField::FieldMetrics CognitiveField::getMetrics() const {
    FieldMetrics metrics;
    metrics.timescale_stats = scheduler_.getAllStats();
    
    if (vse_) {
        const auto& belief = vse_->currentBelief();
        metrics.belief_entropy = belief.entropy();
        metrics.free_energy = belief.free_energy;
        
        // Dominant intent
        int map_idx = 0;
        float max_q = belief.q_intent[0];
        for (int i = 1; i < 8; ++i) {
            if (belief.q_intent[i] > max_q) {
                max_q = belief.q_intent[i];
                map_idx = i;
            }
        }
        static const char* INTENT_NAMES[8] = {
            "CHITCHAT", "QUERY", "COMMAND", "EMOTIONAL_VENT",
            "META_QUESTION", "TUTORIAL", "CREATIVE", "ABORT"
        };
        metrics.dominant_intent = INTENT_NAMES[map_idx];
        metrics.intent_confidence = max_q;
    }
    
    if (cmf_ && cmf_->episodicStore()) {
        metrics.memory_size = cmf_->episodicStore()->size();
    }
    
    // policy_engine_ moved to not_in_use - not available in minimal build
    // if (policy_engine_) {
    //     metrics.active_goals = policy_engine_->getActiveGoalCount();
    // }
    metrics.active_goals = 0;
    
    return metrics;
}

// ─────────────────────────────────────────────────────────────────────────────
// Callback setup
// ─────────────────────────────────────────────────────────────────────────────

void CognitiveField::setupCallbacks() {
    // Register sensory callback FIRST - captures input before timescale callbacks
    scheduler_.setSensoryCallback([this](const yuki::brain::field::SensoryEvent& event) {
        if (event.modality == "text" && !event.data.empty()) {
            last_user_text_ = event.data;
            new_user_turn_.store(true, std::memory_order_release);
            thought_ready_.store(false, std::memory_order_release);
            
            if (!event.embedding.empty()) {
                size_t copy_count = std::min(event.embedding.size(), last_sensory_embedding.size());
                for (size_t i = 0; i < copy_count; ++i) {
                    last_sensory_embedding[i] = event.embedding[i];
                }
            }
            
            // Also update phonological loop with the latest sensory input
            if (phonological_loop_) {
                std::vector<float> embedding_vec(last_sensory_embedding.begin(), last_sensory_embedding.end());
                phonological_loop_->perceive(last_user_text_, embedding_vec, "perception");
            }
        }
    });
    
    // FAST timescales (1ms, 10ms) - Sensory, Phonological, Reflex
    scheduler_.setFastCallback([this](const CognitiveSnapshot& current, CognitiveSnapshot& next) {
        // Update phonological loop with sensory input
        if (!last_user_text_.empty() && phonological_loop_) {
            std::vector<float> embedding_vec(last_sensory_embedding.begin(), last_sensory_embedding.end());
            phonological_loop_->perceive(last_user_text_, embedding_vec, "perception");
        }
        
        // Update YNC if enabled
        if (config_.enable_ync && ync_) {
            // YNC processes at 1ms/10ms
            // ync_->step();  // Would need to expose step function
        }
        
        // next is already initialized as copy of current - modifications persist
    });
    
    // MEDIUM timescale (100ms) - Working memory, Situational
    scheduler_.setMediumCallback([this](const CognitiveSnapshot& current, CognitiveSnapshot& next) {
        // Wernicke comprehension (language → belief features)
        if (wernicke_ && !last_user_text_.empty()) {
            WernickeInput actual_w_input;
            actual_w_input.user_text = last_user_text_;
            actual_w_input.context = current.phonological_store.empty() ? "" : current.phonological_store.back();
            actual_w_input.q_intent = current.vse.q_intent;
            actual_w_input.q_engagement = current.vse.q_engagement;
            actual_w_input.q_urgency = current.vse.q_urgency;
            actual_w_input.safety_mass = current.vse.safety_mass;
            
            auto w_output = wernicke_->comprehend(actual_w_input);
            if (w_output.success) {
                next.vse.q_intent = w_output.intent_delta;
                next.vse.q_engagement = w_output.engagement_delta;
                next.vse.q_urgency = w_output.urgency_delta;
                next.vse.safety_mass = w_output.safety_delta;
                
                next.prediction_errors.fill(0.0f);  // Will be computed by HPC
            }
        }
        
        // Phonological loop rehearsal
        if (phonological_loop_) {
            auto pl_state = phonological_loop_->getState();
            next.phonological_store.clear();
            next.phonological_store.reserve(pl_state.store.size());
            for (const auto& item : pl_state.store) {
                next.phonological_store.push_back(item.text);
            }
            next.inner_speech = pl_state.current_inner_speech;
            next.rehearsal_count = pl_state.rehearsal_buffer.size();
        }
        
        // next is already initialized as copy of current - modifications persist
    });
    
    // SLOW timescales (1s, 1min, 1hr) - Belief, Policy, Drives, Consolidation
    scheduler_.setSlowCallback([this](const CognitiveSnapshot& current, CognitiveSnapshot& next) {
        // 1. Update VSE belief from sensory evidence
        updateVSEBelief(current, next);
        
        // 2. Hierarchical Predictive Coding
        if (config_.enable_hpc) {
            runHPC(current, next);
        }
        
        // 3. Active Inference Policy
        if (config_.enable_active_inference) {
            runActiveInference(current, next);
        }
        
        // 4. Drive system
        if (config_.enable_drives) {
            updateDrives(current, next);
        }
        
        // 5. Awake consolidation
        if (config_.enable_awake_consolidation) {
            runAwakeConsolidation(current, next);
        }
        
        // 6. Generate thought/language (Broca) - ONLY when new user input
        if (new_user_turn_.exchange(false)) {
            generateThought(next);
        }
        
        // next is already initialized as copy of current - modifications persist
    });
}

// ─────────────────────────────────────────────────────────────────────────────
// Internal processing
// ─────────────────────────────────────────────────────────────────────────────

void CognitiveField::updateVSEBelief(const CognitiveSnapshot& current, CognitiveSnapshot& next) {
    if (!vse_) return;
    
    // VSE runs in its own thread; sync its belief state to next snapshot
    const auto& belief = vse_->currentBelief();
    
    // Copy BeliefState fields to VSESnapshot
    next.vse.q_intent = belief.q_intent;
    next.vse.q_engagement = belief.q_engagement;
    next.vse.q_urgency = belief.q_urgency;
    next.vse.safety_mass = belief.safety_mass;
    next.vse.surprise_budget = belief.surprise_budget;
    next.vse.entropy = belief.belief_entropy;
    next.vse.free_energy = belief.free_energy;
    
    // Compute prediction error from sensory vs expected
    if (!last_sensory_embedding.empty() && generative_model_) {
        // Convert VSESnapshot to GenerativeBeliefState for prediction
        yuki::brain::inference::GenerativeBeliefState gen_belief;
        // Note: GenerativeBeliefState needs to be populated from VSESnapshot
        // For now, use a simplified prediction
        std::vector<float> prediction = generative_model_->predictSensory(gen_belief);
        
        if (!prediction.empty() && prediction.size() == last_sensory_embedding.size()) {
            std::vector<float> prediction_error(last_sensory_embedding.size());
            for (size_t i = 0; i < last_sensory_embedding.size(); ++i) {
                prediction_error[i] = last_sensory_embedding[i] - prediction[i];
            }
            
            // Precision-weighted update
            std::vector<float> precision(last_sensory_embedding.size(), 1.0f);
            vse_->updateBelief(prediction_error, precision, 0.05f);
            
            // Re-copy updated belief
            const auto& updated_belief = vse_->currentBelief();
            next.vse.q_intent = updated_belief.q_intent;
            next.vse.q_engagement = updated_belief.q_engagement;
            next.vse.q_urgency = updated_belief.q_urgency;
            next.vse.safety_mass = updated_belief.safety_mass;
            next.vse.surprise_budget = updated_belief.surprise_budget;
            next.vse.entropy = updated_belief.belief_entropy;
            next.vse.free_energy = updated_belief.free_energy;
        }
    }
    
    // Incorporate phonological loop inner speech as self-generated evidence
    if (phonological_loop_) {
        auto pl_state = phonological_loop_->getState();
        if (!pl_state.current_inner_speech.empty()) {
            // Inner speech updates belief (self-monitoring)
            // This is a simplified version - full implementation would encode inner speech
        }
    }
}

void CognitiveField::runHPC(const CognitiveSnapshot& current, CognitiveSnapshot& next) {
    if (!hpc_) return;
    
    // Prepare observation from sensory input and VSE state
    std::array<float, 24> observation{};
    
    // Fill from sensory embedding if available
    if (!current.sensory_embedding.empty()) {
        size_t copy_count = std::min(current.sensory_embedding.size(), observation.size());
        for (size_t i = 0; i < copy_count; ++i) {
            observation[i] = current.sensory_embedding[i];
        }
    } else if (!last_sensory_embedding.empty()) {
        // Fallback to last known sensory embedding
        size_t copy_count = std::min(last_sensory_embedding.size(), observation.size());
        for (size_t i = 0; i < copy_count; ++i) {
            observation[i] = last_sensory_embedding[i];
        }
    }
    
    // Also incorporate VSE belief state into observation
    for (int i = 0; i < 8 && (i + 16) < 24; ++i) {
        observation[16 + i] = current.vse.q_intent[i];
    }
    
    // Run HPC step
    auto hpc_output = hpc_->step(observation);
    
    // Store prediction errors from L1 (sensory level) in NEXT snapshot
    for (size_t i = 0; i < hpc_output.l1.prediction_error.size() && i < next.prediction_errors.size(); ++i) {
        next.prediction_errors[i] = hpc_output.l1.prediction_error[i];
    }
    
    // Store free energy, surprise from L1
    next.hpc_free_energy = hpc_output.l1.free_energy;
    next.hpc_surprise = hpc_output.l1.surprise;
    
    // Store hierarchical states for introspection
    next.hpc_levels.clear();
    auto toArray = [](const std::vector<float>& vec) {
        std::array<float, 24> arr{};
        size_t copy_count = std::min(vec.size(), arr.size());
        for (size_t i = 0; i < copy_count; ++i) arr[i] = vec[i];
        return arr;
    };
    next.hpc_levels.push_back(toArray(hpc_output.l0.prediction));
    next.hpc_levels.push_back(toArray(hpc_output.l1.prediction));
    next.hpc_levels.push_back(toArray(hpc_output.l2.prediction));
    next.hpc_levels.push_back(toArray(hpc_output.l3.prediction));
    next.hpc_levels.push_back(toArray(hpc_output.l4.prediction));
    
    // Phase 1-2: Run LatentWorldModel with HPC level observations
    if (latent_world_model_) {
        // Prepare level observations from HPC output
        std::array<std::vector<float>, 5> level_obs;
        level_obs[0] = hpc_output.l0.prediction;  // L0: VSE/Sensory
        level_obs[1] = hpc_output.l1.prediction;  // L1: Intent
        level_obs[2] = hpc_output.l2.prediction;  // L2: Situational
        level_obs[3] = hpc_output.l3.prediction;  // L3: Conceptual
        level_obs[4] = hpc_output.l4.prediction;  // L4: Abstract
        
        // Feed to HPC for LWM integration
        hpc_->setLevelObservations(level_obs);
        
        // Run LatentWorldModel forward pass
        auto lwm_output = latent_world_model_->forward(level_obs, {}, true);
        
        // Store LWM output for introspection (commented out - needs CognitiveSnapshot extension)
        // next.lwm_total_loss = lwm_output.total_loss;
        // next.lwm_level_losses[0] = lwm_output.reconstruction_losses[0];
        // next.lwm_level_losses[1] = lwm_output.reconstruction_losses[1];
        // next.lwm_level_losses[2] = lwm_output.reconstruction_losses[2];
        // next.lwm_level_losses[3] = lwm_output.reconstruction_losses[3];
        // next.lwm_level_losses[4] = lwm_output.reconstruction_losses[4];
    }
}

void CognitiveField::runActiveInference(const CognitiveSnapshot& current, CognitiveSnapshot& next) {
    if (!policy_engine_) return;
    
    // Select policy based on current belief state
    auto result = policy_engine_->selectPolicy(current);
    
    // Store in NEXT snapshot
    next.policy.selected_policy = result.selected_policy;
    next.policy.expected_free_energy = result.selected_efe;
    next.policy.policy_values = {result.selected_efe};
    next.policy.re_planning = result.re_planned;
    
    // If policy includes "speak" action (ACTION_ANSWER_QUERY = 4), prepare for Broca
    if (result.selected_action == yuki::brain::policy::PolicyEngineConfig::ACTION_ANSWER_QUERY) {
        next.policy.re_planning = true;  // Mark as communication intent
    }
}

void CognitiveField::updateDrives(const CognitiveSnapshot& current, CognitiveSnapshot& next) {
    if (!drive_system_) return;
    
    constexpr float dt = 1.0f;  // 1 second slow timescale
    
    // Real interoceptive signals from cognitive state
    if (hpc_) {
        drive_system_->setPredictionError(hpc_->getFreeEnergy());
        drive_system_->setSurprise(hpc_->getSurprise());
    }
    
    // Social prediction error from VSE engagement mismatch
    float expected_engagement = current.vse.q_engagement[1]; // MEDIUM baseline
    float actual_engagement = current.vse.q_engagement[2];   // HIGH
    drive_system_->setSocialPredictionError(std::abs(expected_engagement - actual_engagement));
    
    // Competence progress from entropy reduction
    static float last_entropy = 1.0f;
    float competence_progress = last_entropy - current.vse.entropy;
    drive_system_->setCompetenceProgress(competence_progress);
    last_entropy = current.vse.entropy;
    
    // Energy from actual compute budget (placeholder - would come from OrganismController)
    drive_system_->setComputeBudget(1.0f);
    
    // Control level from policy execution success
    float control_level = (current.policy.selected_policy >= 0) ? 0.7f : 0.3f;
    drive_system_->setControlLevel(control_level);
    
    // Step the drive ODEs
    drive_system_->step(current, dt);
    
    // Step CuriosityEngine (learning-progress curiosity)
    if (curiosity_engine_) {
        auto curiosity_output = curiosity_engine_->step(current, drive_system_);
        // Store curiosity signals in next snapshot for introspection
        next.curiosity_signals = curiosity_output.curiosity_signals;
        next.curiosity_target_domain = curiosity_output.target_domain;
        next.curiosity_exploration_bonus = curiosity_output.exploration_bonus;
        next.curiosity_rationale = curiosity_output.rationale;
    }
    
    // Store drive states in next snapshot - convert DriveState to DriveSnapshot
    const auto& drive_state = drive_system_->getState();
    next.drives.curiosity = drive_state.value[static_cast<size_t>(yuki::brain::organism::DriveType::CURIOSITY)];
    next.drives.social = drive_state.value[static_cast<size_t>(yuki::brain::organism::DriveType::SOCIAL)];
    next.drives.competence = drive_state.value[static_cast<size_t>(yuki::brain::organism::DriveType::COMPETENCE)];
    next.drives.energy = drive_state.value[static_cast<size_t>(yuki::brain::organism::DriveType::ENERGY)];
    next.drives.safety_need = drive_state.value[static_cast<size_t>(yuki::brain::organism::DriveType::SAFETY_NEED)];
}

void CognitiveField::runAwakeConsolidation(const CognitiveSnapshot& current, CognitiveSnapshot& next) {
    if (!awake_consolidation_) return;
    
    // Consolidation triggered by:
    // 1. High prediction error (surprise)
    // 2. High free energy
    // 3. Drive system signals (curiosity, competence)
    
    float consolidation_trigger = 0.0f;
    if (hpc_) {
        consolidation_trigger += hpc_->getSurprise() * 0.5f;
        consolidation_trigger += hpc_->getFreeEnergy() * 0.3f;
    }
    if (drive_system_) {
        consolidation_trigger += drive_system_->getDrive(yuki::brain::organism::DriveType::CURIOSITY) * 0.2f;
    }
    
    // Pass trigger strength to consolidation engine
    awake_consolidation_->setConsolidationPressure(consolidation_trigger);
    
    // Step awake consolidation
    awake_consolidation_->step(current);
    
    // Store consolidation metrics
    next.consolidation_pressure = consolidation_trigger;
    next.consolidation_cycles_triggered = awake_consolidation_->getCyclesTriggered();
}

void CognitiveField::generateThought(const CognitiveSnapshot& next) {
    if (!broca_) return;
    
    language::BrocaInput b_input;
    b_input.q_intent = next.vse.q_intent;
    b_input.q_engagement = next.vse.q_engagement;
    b_input.q_urgency = next.vse.q_urgency;
    b_input.safety_mass = next.vse.safety_mass;
    b_input.surprise_budget = next.vse.surprise_budget;
    b_input.entropy = next.vse.entropy;
    b_input.user_input = last_user_text_;
    
    // Retrieved context from CMF (semantic + episodic)
    if (cmf_ && cmf_->episodicStore()) {
        b_input.retrieved_context = cmf_->retrieveContextForQuery(last_user_text_, 500);
    }
    
    // Memory context from working memory
    b_input.memory_context = next.working_memory_summary;
    
    // User name from UserModel (if available)
    // b_input.user_name = user_model_ ? user_model_->getName() : "";
    
    // Cognitive intent from policy or InputAnalyzer
    b_input.cognitive_intent = next.policy.selected_policy >= 0 
        ? next.policy.selected_policy 
        : 0;
    
    // Precision from VSE
    b_input.precision.intent = next.precision.intent;
    b_input.precision.entity = next.precision.entity;
    b_input.precision.tone = next.precision.tone;
    b_input.precision.safety = next.precision.safety;
    b_input.precision.source = next.precision.source;
    
    // HPC prediction errors for uncertainty signaling
    b_input.hpc_prediction_errors = next.prediction_errors;
    b_input.hpc_free_energy = next.hpc_free_energy;
    
    // Drive states for motivational modulation
    b_input.drive_states.curiosity = next.drives.curiosity;
    b_input.drive_states.social = next.drives.social;
    b_input.drive_states.competence = next.drives.competence;
    b_input.drive_states.energy = next.drives.energy;
    b_input.drive_states.safety_need = next.drives.safety_need;
    
    // Translate belief to language
    auto output = broca_->translate(b_input);
    
    if (output.success && !output.response_text.empty()) {
        std::lock_guard<std::mutex> lock(thought_mtx_);
        current_thought_ = output.response_text;
        thought_ready_.store(true);
        thought_cv_.notify_all();
        
        // Feed back to phonological loop as inner speech
        if (phonological_loop_) {
            phonological_loop_->innerSpeak(output.response_text, "deliberation");
        }
    }
}

} // namespace field
} // namespace brain
} // namespace yuki