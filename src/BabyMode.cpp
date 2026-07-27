// BabyMode.cpp
// Yuki_1.0 â€” Central Processing Gateway
// Yuki_1.0 — Central Processing Gateway

#include "BabyMode.h"
#include "input/VisionSystem.h"
#include "input/PerceptionLayer.h"
#include "PresenceShell.h"
#include "AutoSensor.h"
#include "brain/memory/ActiveInferenceRetrieval.h"
#include "brain/memory/InformationGainEngine.h"
#include <iostream>
#include "input/encoding/ObservationEncoder.h"
#include <iostream>
#include <fstream>
#include <chrono>
#include <algorithm>
#include <cctype>

#include "brain/database/DatabaseManager.h"
#include "brain/database/UniversalCache.h"
#include "brain/core/ResponseResolver.h"
#include "brain/learning/LearningIngestor.h"
#include "brain/predictive/tool_adapter.h"
#include "brain/learning/BackgroundLearningEngine.h"
#include "brain/memory/MemoryDistiller.h"
#include "brain/memory/EpisodicStore.h"
#include "brain/memory/DifferentialMemoryController.h"
#include "brain/language/SentenceBuilder.h"


// ── Constructor ─────────────────────────────────────────────────────────────────────────────

BabyMode::BabyMode(SessionState& session)
    : session_(session),
      router_(subsystems_),
      micRuntime_(subsystems_),
      speakerRuntime_(subsystems_),
      cameraRuntime_(subsystems_),
      screenRuntime_(subsystems_),
      sttRuntime_(micRuntime_, subsystems_),
      spine_(subsystems_, micRuntime_, speakerRuntime_, &screenRuntime_, &cameraRuntime_)
{
    // Register VisionManager with active runtimes
    vision().initialize(&subsystems_, &cameraRuntime_, &screenRuntime_);

    // STT → voice turn callback
    sttRuntime_.setTranscriptCallback([this](const std::string& text) {
        std::cout << "[BabyMode] VOICE event: \"" << text << "\"\n";
        processVoice(text);
    });

    // STT partial transcript → shell voice draft
    sttRuntime_.setPartialTranscriptCallback([](const std::string& partial) {
        std::cout << "[BabyMode] [PARTIAL] \"" << partial << "\"\n";
    });

    // Runtime state query hook for SubsystemControl
    subsystems_.setRuntimeStateQuery([this](SubsystemName name) -> SubsystemRuntimeState {
        switch (name) {
            case SubsystemName::EAR:        return micRuntime_.reportState();
            case SubsystemName::MOUTH:      return speakerRuntime_.reportState();
            case SubsystemName::WORLD_EYE:  return cameraRuntime_.reportState();
            case SubsystemName::SCREEN_EYE: return screenRuntime_.reportState();
            case SubsystemName::BODY_STATE: return SubsystemRuntimeState::RUNNING;
            default:                        return SubsystemRuntimeState::STOPPED;
        }
    });

    // Subsystem change → sync runtimes
    subsystems_.setChangeCallback([this]() {
        syncRuntimesWithSubsystems();
    });

    subsystems_.setAvailable(SubsystemName::BODY_STATE, true);
    subsystems_.setMode(SubsystemName::BODY_STATE, SubsystemMode::AUTO);

    // Start NeuralSpine background world-model refresh thread
    spine_.start();

    // ── Wire mobile URL provider into CommandRouter ───────────────────────
    router_.setMobileUrlProvider([this]() -> std::string {
        return mobileServer_.localUrl();
    });

    // ── Start self-learning knowledge daemon ────────────────────────────────
    std::cout << "[Knowledge] Starting self-learning daemon...\n";

    if (!knowledge_.start()) {
        std::cout << "[Knowledge] Daemon unavailable — internet or Python missing.\n";
    } else {
        static const char* const CURRICULUM[] = {
            "photosynthesis", "gravity", "evolution", "quantum physics",
            "climate change", "human brain", "solar system", "internet",
            "artificial intelligence", "machine learning", "python programming",
            "world war 2", "water", "dna", "democracy", "economics",
            "algorithm", "computer", "noun", "verb", nullptr
        };
        for (int i = 0; CURRICULUM[i]; ++i)
            knowledge_.learnTopic(CURRICULUM[i], KnowledgeDaemon::LearnPriority::P2_GENERAL);
        std::cout << "[Knowledge] Background curriculum queued (" << 20 << " topics).\n";
    }

    // ── Start Mobile/Browser server ──────────────────────────────────────────
    mobileServer_.setMessageHandler([this](const std::string& text) -> std::string {
        UserTurnInput in;
        in.text   = text;
        in.source = InputSourceKind::TYPED;
        TurnResult result = processUserTurn(in);
        return result.responseText.empty() ? ResponseResolver::instance().resolve("system.no_response") : result.responseText;
    });
    
    mobileServer_.setStatusHandler([this]() -> std::string {
        return ResponseResolver::instance().resolve("system.session_active");
    });
    
    mobileServer_.setSkillsHandler([this]() -> std::string {
        return ResponseResolver::instance().resolve("system.skills_active");
    });
    
    mobileServer_.setConceptsHandler([this](int maxItems) -> std::string {
        (void)maxItems;
        return ResponseResolver::instance().resolve("system.concepts_loaded");
    });

    mobileServer_.start(8765);

    // ── Auto-start all sensors ────────────────────────────────────────────────────────
    autoStartAllSensors(*this);

    cmf_ = std::make_shared<yuki::memory::CognitiveMemoryFabric>();
    if (!cmf_->init()) {
        std::cerr << "[BabyMode] CMF init failed.\n";
    } else {
        cmf_->start();

        // Startup tamper-evidence check on episode chain
        if (auto* es = cmf_->episodicStore()) {
            auto cv = es->verifyChain(0);
            if (!cv.valid) {
                std::cerr << "[BabyMode] TAMPER WARNING: episode chain broken at ID "
                          << cv.first_broken_id
                          << " expected: " << cv.expected_hash
                          << " stored: "   << cv.stored_hash << "\n";
            } else {
                std::cout << "[BabyMode] Episode chain integrity OK (session 0)\n";
            }
        }
    }

    text_encoder_ = std::make_shared<yuki::perception::TextEncoder>();

    knowledge_.setMemoryFabric(cmf_);

    // ── Scrapling runtime: SmartScraper + KnowledgeExtractor ────────────────────────
    smart_scraper_ = std::make_unique<SmartScraper>();
    // TLS profile already defaults to Chrome116 in SmartScraper constructor.
    // Non-blocking connectivity test — failure is logged but not fatal.
    std::thread([this]() {
        try {
            auto test_page = smart_scraper_->scrape("https://httpbin.org/get", 5000);
            std::cout << "[Scrapling] Connectivity OK: elapsed="
                      << test_page.elapsed_ms << "ms\n";
        } catch (const std::exception& e) {
            std::cout << "[Scrapling] Connectivity test failed: " << e.what() << "\n";
        } catch (...) {
            std::cout << "[Scrapling] Connectivity test failed (unknown error)\n";
        }
    }).detach();

    knowledge_extractor_ = std::make_unique<KnowledgeExtractor>();

    DatabaseManager::instance().init("yuki_global.db");
    UniversalCache::instance().preload();
    LearningIngestor::instance().start();  // background learning thread

    // ── Phase 4: BackgroundLearningEngine ──────────────────────────────────────────
    ble_ = std::make_unique<yuki::learning::BackgroundLearningEngine>();
    ble_->init(cmf_, text_encoder_, vse_.get());
    ble_->setCurriculumTopics({
        "english_literature", "english_grammar", "vocabulary",
        "human_psychology",   "dialogue_patterns", "mathematics",
        "algorithms",         "code_cpp",          "general_knowledge"
    });
    ble_->setKnowledgeDaemon(&knowledge_);
    ble_->start();



    // ── Phase 4.5: MemoryDistiller (sleep consolidation) ──────────────────────────
    distiller_ = std::make_unique<yuki::memory::MemoryDistiller>();
    distiller_->init(cmf_, vse_.get());
    distiller_->start();

    // ── Phase 3: SleepThread (idle cognitive maintenance) ──────────────────────────
    sleep_thread_ = std::make_unique<yuki::sleep::SleepThread>();
    sleep_thread_->setEpisodicStore(cmf_->episodicStore());
    sleep_thread_->setSemanticGraph(cmf_->hdcSemanticGraph());
    sleep_thread_->setVSE(vse_.get());
    sleep_thread_->setCMF(cmf_.get());   // wire DMC + T3 access
    sleep_thread_->start();

    // ── Wire DMC to SleepThread ──
    if (auto* dmc = cmf_->differentialMemoryController()) {
        sleep_thread_->setDifferentialMemoryController(dmc);
        std::cout << "[BabyMode] DMC wired to SleepThread.\n";
    }
}

BabyMode::~BabyMode() {
    // Stop Phase 3 SleepThread first (pure cognitive maintenance, no disk flush)
    if (sleep_thread_) sleep_thread_->stop();
    // Stop Phase 4.5 first (may flush index to disk)
    if (distiller_)  distiller_->stop();
    if (ble_)        ble_->stop();
    // Scrapling components (no background thread, just cleanup)
    knowledge_extractor_.reset();
    smart_scraper_.reset();
    LearningIngestor::instance().stop();  // drain queue before DB closes
    mobileServer_.stop();
    vision().initialize(nullptr, nullptr, nullptr);
    spine_.stop();
    vse_trainer_.reset();
    local_llm_.reset();
    knowledge_.stop();
    if (cmf_) cmf_->stop();
    subsystems_.setChangeCallback(nullptr);
    subsystems_.setRuntimeStateQuery(nullptr);
    sttRuntime_.setTranscriptCallback(nullptr);
}

// ── Accessors ─────────────────────────────────────────────────────────────────────────────────────

CheckpointTracer&    BabyMode::tracer()     { return tracer_; }
SubsystemControl&    BabyMode::subsystems() { return subsystems_; }
CommandRouter&       BabyMode::router()     { return router_; }
SpeechToTextRuntime& BabyMode::stt()        { return sttRuntime_; }
NeuralSpine&         BabyMode::spine()      { return spine_; }

void BabyMode::setAvatarCallback(AvatarCallback cb) {
    avatarCb_ = cb;
    speakerRuntime_.setPhaseCallback([this](SpeakPhase phase, const std::string& text) {
        if (!avatarCb_) return;
        if (phase == SpeakPhase::SPEAKING) {
            avatarCb_("SPEAKING", text);
        } else if (phase == SpeakPhase::COMPLETED ||
                   phase == SpeakPhase::IDLE      ||
                   phase == SpeakPhase::BLOCKED   ||
                   phase == SpeakPhase::FAILED) {
            avatarCb_("IDLE", "");
        }
    });
}

// ── Output Delivery ───────────────────────────────────────────────────────────────────────────────

void BabyMode::deliverResponse(const std::string& text) {
    if (text.empty()) return;

    // Request TTS speech — avatar state is driven by phaseCallback
    SpeakResult sr = speakerRuntime_.speak(text);
    std::cout << "[Speak] Accepted: " << (sr.accepted ? "YES" : "NO")
              << " Reason: " << sr.reason << "\n";
}

// ── Boot-time greeting ────────────────────────────────────────────────────────────────────────────

void BabyMode::announceReady() {
    const std::string greeting = ResponseResolver::instance().resolve("BOOT_GREETING");

    std::cout << "\n[Yuki] " << greeting << "\n\n";

    {
        std::lock_guard<std::mutex> lock(session_.historyMutex);
        session_.history.push_back({"Yuki", greeting, false});
    }

    SpeakResult sr = speakerRuntime_.speak(greeting);
    std::cout << "[Ready] TTS Accepted: " << (sr.accepted ? "YES" : "NO") << "\n";
}

// ── Setters ───────────────────────────────────────────────────────────────────────────────────────

void BabyMode::setPredictiveEngine(std::unique_ptr<yuki::TurnCoordinator> coordinator,
                                   std::shared_ptr<yuki::MemoryStore> memory_store,
                                   std::shared_ptr<yuki::UserModel> user_model) {
    coordinator_ = std::move(coordinator);
    memory_store_ = memory_store;
    user_model_ = user_model;
    if (coordinator_ && memory_store_) {
        coordinator_->set_memory_store(memory_store_);
    }
    if (coordinator_ && cmf_) {
        coordinator_->setMemoryFabric(cmf_.get());
        std::cout << "[BabyMode] CMF wired to TurnCoordinator.\n";
    }
    if (coordinator_ && text_encoder_) {
        coordinator_->setTextEncoder(text_encoder_.get());
        std::cout << "[BabyMode] TextEncoder wired to TurnCoordinator.\n";
    }
    if (coordinator_ && cmf_ && vse_) {
        vse_->generativeModel().setProceduralStore(cmf_->proceduralStore());
        vse_->generativeModel().loadMappings();

        ige_ = std::make_unique<yuki::memory::InformationGainEngine>(&vse_->generativeModel());
        air_ = std::make_unique<yuki::memory::ActiveInferenceRetrieval>(ige_.get(), cmf_->episodicStore(), cmf_->hdcSemanticGraph());
        coordinator_->setAIR(air_.get());
        std::cout << "[BabyMode] AIR and IGE wired to TurnCoordinator.\n";
    }

    // â”€â”€ Phase 0: VSE Bootstrap Training â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    // Inject 160 synthetic examples to bootstrap GenerativeModel intent mappings.
    // Runs ONCE here. Model continues learning from real turns via EMA updates.
    if (vse_) {
        vse_trainer_ = std::make_unique<yuki::inference::VseBootstrapTrainer>(vse_.get());
        int injected = vse_trainer_->injectAll();
        std::cout << "[BabyMode] VSE bootstrap complete: " << injected
                  << " examples injected.\n";
    }

    // â”€â”€ Phase 0: Local LLM (neural response generation) â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    local_llm_ = std::make_unique<yuki::LocalLLM>();
    if (coordinator_) {
        coordinator_->setLocalLLM(local_llm_.get());
        std::cout << "[BabyMode] LocalLLM wired to TurnCoordinator.\n";
        if (local_llm_->isAvailable()) {
            std::cout << "[BabyMode] Ollama detected. Neural generation is ACTIVE.\n";
        } else {
            std::cout << "[BabyMode] Ollama not detected. Neural generation DISABLED.\n"
                         "    Run: ollama serve    (in another terminal)\n"
                         "    Then: ollama pull qwen3:1.7b\n";
        }
    }

    sentence_builder_ = std::make_unique<yuki::language::SentenceBuilder>();
    if (coordinator_) {
        coordinator_->setSentenceBuilder(sentence_builder_.get());
        std::cout << "[BabyMode] SentenceBuilder wired to TurnCoordinator.\n";
    }

    if (coordinator_ && presence_shell_) {
        coordinator_->setPresenceShell(presence_shell_);
    }

}

namespace {
struct DmcEvalResult {
    bool has_dmc = false;
    yuki::memory::DecisionToken token;
};

DmcEvalResult evaluateDmc(
    yuki::inference::VariationalStateEstimator* vse,
    yuki::memory::CognitiveMemoryFabric* cmf,
    const std::string& input)
{
    if (!cmf) return {};
    auto* dmc = cmf->differentialMemoryController();
    if (!dmc) return {};

    std::array<float, 24> posterior{};
    if (vse) {
        const auto& belief = vse->currentBelief();
        for (size_t i = 0; i < 8; ++i) posterior[i] = belief.q_intent[i];
        for (size_t i = 0; i < 3; ++i) posterior[8 + i] = belief.q_engagement[i];
        for (size_t i = 0; i < 2; ++i) posterior[11 + i] = belief.q_urgency[i];
        posterior[13] = belief.safety_mass;
        posterior[14] = belief.surprise_budget;
    }

    std::array<float, 24> context{};
    context[0] = (std::min)(1.0f, static_cast<float>(input.size()) / 100.0f);
    context[1] = (std::min)(1.0f, static_cast<float>(cmf->totalEpisodes()) / 1000.0f);
    if (vse) {
        context[2] = vse->currentBelief().entropy();
    }

    auto [decision, token] = dmc->evaluate(posterior, context);

    if (decision.action == yuki::memory::DMCDecision::PROMOTE && !decision.safety_override) {
        if (auto* episodic = cmf->episodicStore()) {
            auto snaps = episodic->queryRecentSnapshots(1, false);
            if (!snaps.empty() && cmf->hdcSemanticGraph()) {
                std::string ep_id = "ep_" + std::to_string(snaps[0].episode_id);
                cmf->hdcSemanticGraph()->ingestProposition(ep_id, "promoted_from_episodic");
                episodic->resetReinforcement(ep_id);
                std::cout << "[DMC] Dynamically promoted " << ep_id << " to T2.\n";
            }
        }
    }

    DmcEvalResult res;
    res.has_dmc = true;
    res.token = token;
    return res;
}

void recordDmcOutcome(
    yuki::memory::CognitiveMemoryFabric* cmf,
    const yuki::memory::DecisionToken& token,
    bool success,
    float precision)
{
    if (!cmf) return;
    if (auto* dmc = cmf->differentialMemoryController()) {
        dmc->recordOutcome(token, success, precision);
    }
}
} // namespace

// â”€â”€ Public Turn API â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

BabyOutputState BabyMode::process(const std::string& input) {
    bumpDistillerActivity();
    // Step 3.5: SelfModel correction hook
    if (coordinator_ && coordinator_->getSelfModel()) {
        std::string lower = input;
        std::transform(lower.begin(), lower.end(), lower.begin(),
            [](unsigned char c){ return std::tolower(c); });
            
        std::vector<std::pair<std::string, float>> dummy;
        std::vector<std::pair<std::string, std::string>> patterns = {
            {"wrong", "prefix"}, {"no", "prefix"}, {"incorrect", "prefix"}
        };
        // Load dynamically from config file if present
        std::ifstream file("data/correction_patterns.txt");
        if (file.is_open()) {
            patterns.clear();
            std::string line;
            while (std::getline(file, line)) {
                if (line.empty() || line[0] == '#') continue;
                size_t p = line.find('|');
                if (p != std::string::npos) {
                    patterns.emplace_back(line.substr(0, p), line.substr(p + 1));
                }
            }
        }

        bool isCorrection = false;
        for (const auto& [pat, type] : patterns) {
            if (type == "prefix" && lower.find(pat) == 0) { isCorrection = true; break; }
            if (type == "contains" && lower.find(pat) != std::string::npos) { isCorrection = true; break; }
            if (type == "exact" && lower == pat) { isCorrection = true; break; }
        }

        if (isCorrection) {
            std::array<float, 11> empty_comp{};
            coordinator_->getSelfModel()->update(empty_comp, 1.0f, {0,0,0,0}, false, 0.5f);
        }
    }

    // === Normal turn processing (Tier 2/3 reached via shape_response) =========
    DmcEvalResult dmc_res = evaluateDmc(vse_.get(), cmf_.get(), input);

    yuki::MultiModalInput mmi;
    mmi.text = input;
    mmi.speech_transcript = "";

    yuki::TurnResult result;
    if (coordinator_) {
        {
            std::lock_guard<std::mutex> lock(*coordinator_->current_state().state_mutex);
            coordinator_->current_state().last_raw_input = input;
            coordinator_->current_state().last_normalized_input = input;
        }
        result = coordinator_->run_turn(mmi);
    } else {
        result.template_family = "system_error";
        result.template_slot = "engine_uninitialized";
        result.response_text = ResponseResolver::instance().resolve(result);
        result.response_tone = "neutral";
        result.can_act = true;
    }
    std::cout << "[BABY] veto=" << result.veto 
              << " can_act=" << result.can_act 
              << " requires_clarification: " << result.requires_clarification << "\n";

    if (dmc_res.has_dmc) {
        bool success = !result.veto && result.turn_committed;
        recordDmcOutcome(cmf_.get(), dmc_res.token, success, 1.0f);
    }

    {
        std::lock_guard<std::mutex> lock(session_.historyMutex);
        session_.history.push_back({"You", input, false});
    }

    if (presence_shell_) {
        if (result.veto) {
            presence_shell_->show_safety_warning(result.response_text);
        } else if (result.requires_clarification) {
            presence_shell_->show_clarification(result.clarification_question);
        } else if (!result.response_text.empty()) {
            // P0 FIX: LLM runs before can_act check; display neural response even when can_act=0
            presence_shell_->show_response(result.response_text, result.response_tone);
        } else {
            presence_shell_->show_response(ResponseResolver::instance().resolve("fallback.not_sure"), "neutral");
        }
    } else {
        if (result.veto) {
            std::cout << "\nYuki: [VETO] " << result.response_text << "\n";
        } else if (result.requires_clarification) {
            std::cout << "\nYuki: " << result.clarification_question << "\n";
        } else if (!result.response_text.empty()) {
            // P0 FIX: Display LLM response even when can_act=0
            std::cout << "\nYuki: " << result.response_text << "\n";
        } else {
            std::cout << "\nYuki: " << ResponseResolver::instance().resolve("fallback.not_sure") << "\n";
        }
    }

    deliverResponse(result.requires_clarification ? result.clarification_question : result.response_text);

    // ── SystemExecutor bridge ─────────────────────────────────────────────
    // When can_act=true and VSE MAP is COMMAND(2) or ABORT(7), queue raw
    // input for SkillRegistry / TaskDecomposer dispatch. Previously
    // tool_calls_queued was always empty — Yuki talked but never executed.
    if (result.can_act && !result.veto && !result.requires_clarification) {
        int map_intent = 0;
        if (vse_) {
            map_intent = static_cast<int>(
                vse_->currentBelief().getMAP().intent);
        }
        if (map_intent == 2 || map_intent == 7) { // COMMAND or ABORT
            std::cout << "[Executor] COMMAND intent -- queuing: " << input << "\n";
            result.tool_calls_queued.push_back(input);
        }
    }

    // Execute queued tools (SkillRegistry -> TaskDecomposer -> fallback)
    yuki::ToolAdapter adapter;
    for (const auto& tool : result.tool_calls_queued) {
        adapter.execute(tool, result);
    }

    BabyOutputState out;
    out.reaction = result.requires_clarification ? result.clarification_question : result.response_text;
    return out;
}

void BabyMode::processVoice(const std::string& transcript) {
    bumpDistillerActivity();
    DmcEvalResult dmc_res = evaluateDmc(vse_.get(), cmf_.get(), transcript);

    yuki::MultiModalInput mmi;
    mmi.text = transcript;
    mmi.speech_transcript = transcript;

    yuki::TurnResult result;
    if (coordinator_) {
        {
            std::lock_guard<std::mutex> lock(*coordinator_->current_state().state_mutex);
            coordinator_->current_state().last_raw_input = transcript;
            coordinator_->current_state().last_normalized_input = transcript;
        }
        result = coordinator_->run_turn(mmi);
    } else {
        result.template_family = "system_error";
        result.template_slot = "engine_uninitialized";
        result.response_text = ResponseResolver::instance().resolve(result);
        result.response_tone = "neutral";
        result.can_act = true;
    }

    if (dmc_res.has_dmc) {
        bool success = !result.veto && result.turn_committed;
        recordDmcOutcome(cmf_.get(), dmc_res.token, success, 1.0f);
    }

    {
        std::lock_guard<std::mutex> lock(session_.historyMutex);
        session_.history.push_back({"Voice", transcript, true});
    }

    if (presence_shell_) {
        if (result.veto) {
            presence_shell_->show_safety_warning(result.response_text);
        } else if (result.requires_clarification) {
            presence_shell_->show_clarification(result.clarification_question);
        } else if (!result.response_text.empty()) {
            // P0 FIX: Display neural response even when can_act=0 (LLM runs before early returns)
            presence_shell_->show_response(result.response_text, result.response_tone);
        } else {
            presence_shell_->show_response(ResponseResolver::instance().resolve("fallback.not_sure"), "neutral");
        }
    }

    deliverResponse(result.requires_clarification ? result.clarification_question : result.response_text);

    // ── SystemExecutor bridge ─────────────────────────────────────────────
    // When can_act=true and VSE MAP is COMMAND(2) or ABORT(7), queue raw
    // input for SkillRegistry / TaskDecomposer dispatch. Previously
    // tool_calls_queued was always empty — Yuki talked but never executed.
    if (result.can_act && !result.veto && !result.requires_clarification) {
        int map_intent = 0;
        if (vse_) {
            map_intent = static_cast<int>(
                vse_->currentBelief().getMAP().intent);
        }
        if (map_intent == 2 || map_intent == 7) { // COMMAND or ABORT
            std::cout << "[Executor] COMMAND intent -- queuing: " << transcript << "\n";
            result.tool_calls_queued.push_back(transcript);
        }
    }

    // Execute queued tools (SkillRegistry -> TaskDecomposer -> fallback)
    yuki::ToolAdapter adapter;
    for (const auto& tool : result.tool_calls_queued) {
        adapter.execute(tool, result);
    }
}

TurnResult BabyMode::processUserTurn(const UserTurnInput& input) {
    // Publish to Global Workspace so all GW subscribers observe this turn
    if (use_global_workspace_) {
        bool is_voice = (input.source == InputSourceKind::VOICE_FINAL ||
                         input.source == InputSourceKind::VOICE_PARTIAL);
        publishTurnToGW(input.text, is_voice);
    }

    BabyOutputState out = process(input.text);
    TurnResult turnResult;
    turnResult.handled      = true;
    turnResult.responseText = out.reaction;
    turnResult.commandName  = input.text;

    // â”€â”€ Phase 4: Feed turn to BackgroundLearningEngine â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    if (ble_)       ble_->ingestUserTurn(input.text);
    // â”€â”€ Phase 4.5: Reset idle timer on every turn â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    if (distiller_) distiller_->bumpActivity();

    // Introspection queries: user asks what Yuki knows / its gaps
    if (coordinator_ && coordinator_->getSelfModel()) {
        std::string lower = input.text;
        for (auto& c : lower)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (lower.find("what do you know")    != std::string::npos ||
            lower.find("your expertise")      != std::string::npos ||
            lower.find("your confidence")     != std::string::npos ||
            lower.find("your gaps")           != std::string::npos ||
            lower.find("how good are you")    != std::string::npos) {
            std::string summary = coordinator_->getSelfModel()->toString();
            yuki::gw::Message smsg;
            smsg.topic         = yuki::gw::Topic::META_COGNITIVE;
            smsg.source_module = "BabyMode";
            smsg.salience      = 0.7f;
            // Escape quotes in summary before embedding in JSON
            std::string safe_summary = summary;
            for (size_t i = 0; i < safe_summary.size(); ++i) {
                if (safe_summary[i] == '"' || safe_summary[i] == '\\') {
                    safe_summary.insert(i, 1, '\\'); ++i;
                } else if (safe_summary[i] == '\n') {
                    safe_summary.replace(i, 1, "\\n");
                }
            }
            smsg.payload_json  = "{\"type\":\"self_summary\",\"content\":\"" + safe_summary + "\"}";
            yuki::gw::CoreBus::instance().publish(smsg);
            turnResult.responseText = summary; // surface to user
        }
    }

    // â”€â”€ Phase 0.5: Extract user facts from this turn â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    // UserMemory handles extraction + storage. LLM prompt injection happens
    // in TurnCoordinator via buildPrompt() â†’ [Memory: ...] block.
    // NO hardcoded response override â€” LLM answers naturally from context.
    if (user_memory_) {
        user_memory_->extractAndStore(input.text);
    }

    return turnResult;
}

void BabyMode::publishTurnToGW(const std::string& text, bool is_voice) {
    yuki::gw::Message msg;
    msg.topic          = yuki::gw::Topic::USER_TURN;
    msg.source_module = "BabyMode";
    msg.salience       = 0.8f;
    // Minimal JSON payload â€” escape backslash and quote for safety
    std::string safe = text;
    for (size_t i = 0; i < safe.size(); ++i) {
        if (safe[i] == '"' || safe[i] == '\\') { safe.insert(i, 1, '\\'); ++i; }
    }
    msg.payload_json = "{\"text\":\"" + safe + "\",\"is_voice\":" +
                       (is_voice ? "true" : "false") + "}";

    // Compete in GlobalWorkspace (winner-take-all) AND direct publish for subscribers
    yuki::gw::Coalition coalition;
    coalition.module_id = "BabyMode";
    coalition.topic     = yuki::gw::Topic::USER_TURN;
    coalition.salience  = msg.salience;
    coalition.message   = msg;
    yuki::gw::GlobalWorkspace::instance().compete(coalition);
    yuki::gw::CoreBus::instance().publish(msg);

    yuki::infra::ModuleRegistry::instance().heartbeat("BabyMode");
}

void BabyMode::routeViaGlobalWorkspace(const UserTurnInput& input) {
    bool is_voice = (input.source == InputSourceKind::VOICE_FINAL ||
                     input.source == InputSourceKind::VOICE_PARTIAL);
    publishTurnToGW(input.text, is_voice);
}

// â”€â”€ Runtime Sync â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

void BabyMode::syncRuntimesWithSubsystems() {
    // Atomic exchange: only one caller proceeds at a time.
    bool expected = false;
    if (!isSyncingRuntimes_.compare_exchange_strong(expected, true)) return;

    // EAR (Mic + STT)
    if (subsystems_.isActive(SubsystemName::EAR)) {
        if (micRuntime_.reportState() == SubsystemRuntimeState::STOPPED)
            micRuntime_.start();
        if (sttRuntime_.getState() == SttState::STOPPED ||
            sttRuntime_.getState() == SttState::FAILED) {
            bool ok = sttRuntime_.start();
            if (!ok) {
                std::cout << "[BabyMode] STT start failed (model status: "
                          << whisperModelStatusStr(sttRuntime_.getModelStatus())
                          << "). Typed input remains active.\n";
            }
        } else {
            sttRuntime_.setListening(true);
        }
    } else {
        if (micRuntime_.reportState() != SubsystemRuntimeState::STOPPED)
            micRuntime_.stop();
        sttRuntime_.setListening(false);
    }

    // MOUTH
    if (subsystems_.isActive(SubsystemName::MOUTH)) {
        if (speakerRuntime_.reportState() == SubsystemRuntimeState::STOPPED)
            speakerRuntime_.start();
    } else {
        if (speakerRuntime_.reportState() != SubsystemRuntimeState::STOPPED)
            speakerRuntime_.stop();
    }

    // WORLD_EYE (Camera)
    if (subsystems_.isActive(SubsystemName::WORLD_EYE)) {
        if (cameraRuntime_.reportState() == SubsystemRuntimeState::STOPPED)
            cameraRuntime_.start();
    } else {
        if (cameraRuntime_.reportState() != SubsystemRuntimeState::STOPPED)
            cameraRuntime_.stop();
    }

    // SCREEN_EYE
    if (subsystems_.isActive(SubsystemName::SCREEN_EYE)) {
        if (screenRuntime_.reportState() == SubsystemRuntimeState::STOPPED)
            screenRuntime_.start();
    } else {
        if (screenRuntime_.reportState() != SubsystemRuntimeState::STOPPED)
            screenRuntime_.stop();
    }

    isSyncingRuntimes_ = false;
}

void BabyMode::runMassCurriculumIfNeeded() {
    if (yuki::learning::MassCurriculumLoader::isCompleted()) {
        std::cout << "[BabyMode] Mass curriculum already ingested.\n";
        return;
    }
    if (!cmf_) {
        std::cerr << "[BabyMode] CMF not initialized. Cannot run mass curriculum.\n";
        return;
    }
    // Self-destructing scope: loader destroyed after execute()
    {
        auto loader = std::make_unique<yuki::learning::MassCurriculumLoader>(cmf_);
        loader->execute();
    }
    std::cout << "[BabyMode] Mass curriculum loader self-destructed.\n";
}

void BabyMode::bumpDistillerActivity() {
    if (distiller_)    distiller_->bumpActivity();
    if (sleep_thread_) sleep_thread_->signalActivity();  // reset idle timer
}
