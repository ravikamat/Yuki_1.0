// BabyMode.cpp
// Yuki_1.0 — Central Processing Gateway

#include "BabyMode.h"
#include "brain/knowledge/WebKnowledgeExtractor.h"
#include "brain/knowledge/AtomicAdapter.h"
#include "brain/causality/CausalWorldModel.h"
#include "brain/learning/FastText.h"
#include "brain/language/GenerationRouter.h"
#include "brain/language/NativeGenerationBackend.h"
#include "brain/language/GroundedGenerationBackend.h"
#include "brain/language/GeneratorSelector.h"
#include "brain/memory/HdcSemanticGraph.h"
#include "brain/inference/PrecisionPredictor.h"
#include "input/VisionSystem.h"
#include "input/PerceptionLayer.h"
#include "PresenceShell.h"
#include "AutoSensor.h"
#include "brain/memory/ActiveInferenceRetrieval.h"
#include "brain/memory/InformationGainEngine.h"
#include <iostream>
#include "input/encoding/ObservationEncoder.h"
#include <fstream>
#include <chrono>
#include <algorithm>
#include <cctype>
#include <filesystem>

#include "brain/database/DatabaseManager.h"
#include "brain/database/UniversalCache.h"
#include "brain/core/ResponseResolver.h"
#include "brain/core/ProjectPaths.h"
#include "brain/learning/LearningIngestor.h"
#include "brain/predictive/tool_adapter.h"
#include "brain/learning/BackgroundLearningEngine.h"
#include "brain/memory/EpisodicStore.h"
#include "brain/memory/DifferentialMemoryController.h"
#include "brain/language/SentenceBuilder.h"
#include "brain/system/CognitiveEventBus.h"
#include "brain/memory/TranscriptMirror.h"
#include "brain/system/ToolPermissionEngine.h"
#include "brain/truth/TruthGate.h"
#include "brain/memory/MemoryReconsolidationEngine.h"
#include "brain/bootstrap/GgufManifoldExtractor.h"
#include "brain/bootstrap/PriorsInjector.h"
#include "brain/field/CognitiveField.h"
#include "brain/field/FieldScheduler.h"
#include "brain/core/HierarchicalPredictiveCoding.h"
#include "brain/core/CognitiveManifold.h"
#include "brain/inference/GenerativeModel.h"
#include "brain/policy/ActiveInferencePolicyEngine.h"
#include "brain/memory/AwakeConsolidationEngine.h"
#include "brain/organism/ContinuousDriveSystem.h"
#include "brain/organism/ProactiveEngine.h"
#include "input/WakeDetector.h"
#include "brain/autonomy/CuriosityEngine.h"
#include "brain/logic/LogicVerificationLayer.h"
#include "brain/language/GrammarEngine.h"
#include "brain/language/NativeSentenceSynthesizer.h"
#include "brain/language/LanguageBootstrap.h"
#include "brain/language/EpisodicLanguageGrounding.h"
#include "brain/predictive/TurnCoordinatorAdapter.h"
#include "brain/language/BrocaTranslator.h"
#include "brain/language/WernickeComprehender.h"
#include "brain/language/PhonologicalLoop.h"
#include "brain/self/RichSelfModel.h"
#include <stdexcept>


// ── Constructor ─────────────────────────────────────────────────────────────────────────────

BabyMode::BabyMode(SessionState& session)
    : router_(subsystems_),
      micRuntime_(subsystems_),
      speakerRuntime_(subsystems_),
      cameraRuntime_(subsystems_),
      screenRuntime_(subsystems_),
      sttRuntime_(micRuntime_, subsystems_),
      session_(session)   // declared last in the class: keep this last
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

    // ── Wire mobile URL provider into CommandRouter ───────────────────────
    router_.setMobileUrlProvider([this]() -> std::string {
        return mobileServer_.localUrl();
    });

    // ── Start self-learning knowledge daemon ────────────────────────────────
    std::cout << "[Knowledge] Starting self-learning daemon...\n";

    // NOTE: start() returns false only if already running; it does not probe
    // internet/Python. Curriculum topics are queued and drained by the worker.
    knowledge_.start();

    {
        static const char* const CURRICULUM[] = {
            "photosynthesis", "gravity", "evolution", "quantum physics",
            "climate change", "human brain", "solar system", "internet",
            "artificial intelligence", "machine learning", "python programming",
            "world war 2", "water", "dna", "democracy", "economics",
            "algorithm", "computer", "noun", "verb", nullptr
        };
        for (int i = 0; CURRICULUM[i]; ++i)
            knowledge_.learnTopic(CURRICULUM[i], yuki::brain::learning::KnowledgeDaemon::LearnPriority::P2_NORMAL);
        std::cout << "[Knowledge] Background curriculum queued (" << 20 << " topics).\n";
    }

    // ── Start Mobile/Browser server ──────────────────────────────────────────
    mobileServer_.setMessageHandler([this](const std::string& text) -> std::string {
        UserTurnInput in;
        in.text   = text;
        in.source = InputSourceKind::TYPED;
        BabyTurnResult result = processUserTurn(in);
        return result.responseText.empty() ? ResponseResolver::instance().resolve("system.no_response") : result.responseText;
    });
    
    mobileServer_.setStatusHandler([]() -> std::string {
        return ResponseResolver::instance().resolve("system.session_active");
    });
    
    mobileServer_.setSkillsHandler([]() -> std::string {
        return ResponseResolver::instance().resolve("system.skills_active");
    });
    
    mobileServer_.setConceptsHandler([](int maxItems) -> std::string {
        (void)maxItems;
        return ResponseResolver::instance().resolve("system.concepts_loaded");
    });

    mobileServer_.start(8765);

    // ── Auto-start all sensors ────────────────────────────────────────────────────────
    autoStartAllSensors(*this);

    cmf_ = std::make_shared<yuki::brain::memory::CognitiveMemoryFabric>();
    if (!cmf_->init()) {
        std::cerr << "[BabyMode] CMF init failed.\n";
        // Fail fast: every downstream subsystem dereferences cmf_-backed stores.
        // Constructing in a half-initialized state is a guaranteed crash later.
        throw std::runtime_error("[BabyMode] CognitiveMemoryFabric init failed — cannot start");
    } else {
        cmf_->start();

        // Give ResponseResolver the weighted-context assembler dependency so
        // knowledge queries can fall back to ranked HDC triples.
        ResponseResolver::instance().setHdcGraph(cmf_->hdcSemanticGraph());

        // Startup tamper-evidence check on episode chain
        if (auto* es = cmf_->episodicStore()) {
            bool cv = es->verifyChain(0);
            if (!cv) {
                std::cerr << "[BabyMode] TAMPER WARNING: episode chain verification failed\n";
            } else {
                std::cout << "[BabyMode] Episode chain integrity OK (session 0)\n";
            }
        }
    }

    // ── UserMemory: persistent personal facts ───────────────────────────────
    // Created in main and injected via setUserMemory() to avoid double instantiation.
    // setPredictiveEngine() provides a fallback if not injected.

    // ── Phase 0: Local LLM (neural response generation) ───────────────────────────
    std::cout << "[BabyMode] DEBUG: Starting LocalLLM initialization...\n" << std::flush;
    local_llm_ = std::make_unique<yuki::LocalLLM>();
    std::cout << "[BabyMode] DEBUG: LocalLLM constructed successfully.\n" << std::flush;
    std::cout << "[BabyMode] LocalLLM instance created.\n" << std::flush;
    
    // Load LocalTransformer model (primary backend)
    if (local_llm_ && local_llm_->getLocalTransformer()) {
        std::cout << "[BabyMode] LocalTransformer pointer obtained.\n" << std::flush;
        std::filesystem::path model_path = yuki::paths::dataPath("models/Qwen3.8-27B-UD-IQ1_S.gguf");
        if (const char* env_model = std::getenv("YUKI_MODEL_PATH")) {
            if (*env_model) model_path = env_model;
        }
        std::cout << "[BabyMode] Checking model path: " << model_path << "\n" << std::flush;
        if (std::filesystem::exists(model_path)) {
            std::cout << "[BabyMode] Model file exists: " << model_path << "\n" << std::flush;
            std::cout << "[BabyMode] Loading local model into memory (can take 30-45s for 27B on CPU)... Please wait.\n" << std::flush;
            bool load_result = local_llm_->getLocalTransformer()->loadModel(model_path);
            std::cout << "[BabyMode] loadModel() returned: " << (load_result ? "true" : "false") << "\n" << std::flush;
            if (load_result) {
                std::cout << "[BabyMode] LocalTransformer loaded successfully.\n" << std::flush;
            } else {
                std::cerr << "[BabyMode] Failed to load LocalTransformer model. Falling back to NativeMind.\n" << std::flush;
            }
        } else {
            std::cout << "[BabyMode] LocalTransformer model not found at: " << model_path << " — running in pure NativeMind mode.\n" << std::flush;
        }
    } else {
        std::cerr << "[BabyMode] LocalTransformer pointer is null!\n" << std::flush;
    }
    std::cout << "[BabyMode] LocalLLM initialization complete.\n" << std::flush;

    // ── Phase 1: Continuous Cognition Substrate ─────────────────────────────────
    std::cout << "[BabyMode] Initializing Phase 1: Continuous Cognition Substrate...\n" << std::flush;
    
    // VSE must exist before anything wires setVSE(vse_.get()) below.
    vse_ = std::make_unique<yuki::brain::inference::VariationalStateEstimator>();
    std::cout << "[BabyMode] VariationalStateEstimator created.\n";
    
    // FieldScheduler
    field_scheduler_ = std::make_unique<yuki::brain::field::FieldScheduler>();
    std::cout << "[BabyMode] FieldScheduler created.\n";
    
    // Hierarchical Predictive Coding
    hpc_ = std::make_unique<yuki::brain::core::HierarchicalPredictiveCoding>();
    std::cout << "[BabyMode] HPC created.\n";
    
    // Cognitive Manifold (512D shared latent)
    cognitive_manifold_ = std::make_unique<yuki::brain::core::CognitiveManifold>();
    std::cout << "[BabyMode] CognitiveManifold (512D) created.\n";
    
    // Generative Model (forward pass for HPC + Active Inference)
    generative_model_ = std::make_unique<yuki::brain::inference::GenerativeModel>();
    std::cout << "[BabyMode] GenerativeModel created.\n";
    
// Active Inference Policy Engine
    policy_engine_ = std::make_unique<yuki::brain::policy::ActiveInferencePolicyEngine>();
    std::cout << "[BabyMode] ActiveInferencePolicyEngine created.\n";
    
    // ── Phase 1-2: Latent World Model + Causal + MPC ──────────────────────
    latent_world_model_ = std::make_unique<yuki::brain::predictive::LatentWorldModel>();
    std::cout << "[BabyMode] LatentWorldModel created.\n";
    
    causal_world_model_ = std::make_unique<yuki::brain::causality::CausalWorldModel>();
    std::cout << "[BabyMode] CausalWorldModel created.\n";
    
    latent_mpc_planner_ = std::make_unique<yuki::brain::planning::LatentMPCPlanner>();
    std::cout << "[BabyMode] LatentMPCPlanner created.\n";
    
    // Awake Consolidation Engine
    awake_consolidation_ = std::make_unique<yuki::brain::memory::AwakeConsolidationEngine>();
    std::cout << "[BabyMode] AwakeConsolidationEngine created.\n";
    
    // Continuous Drive System (for CognitiveField)
    continuous_drives_ = std::make_unique<yuki::brain::organism::ContinuousDriveSystem>();
    std::cout << "[BabyMode] ContinuousDriveSystem created.\n";
    
    // Broca Translator (belief → language)
    broca_translator_ = std::make_unique<yuki::brain::language::BrocaTranslator>(
        local_llm_ ? local_llm_->getLocalTransformer().get() : nullptr);
    std::cout << "[BabyMode] BrocaTranslator created.\n";

    // NativeMind: GrammarEngine + NativeSentenceSynthesizer + LanguageBootstrap + EpisodicLanguageGrounding
    // Ground the grammar engine with a real embedding model (was nullptr, so
    // lexical choice had no vector signal) and initialise the bootstrap whose
    // word↔concept bindings the synthesizer relies on.
    fast_text_ = std::make_unique<yuki::learning::FastText>();
    // Load pretrained FastText word vectors (Simple English Wikipedia, 300-d)
    // in a background thread so startup is not blocked. GrammarEngine holds a
    // raw const FastText* — once loadPretrainedVectors() populates the
    // word_vectors_ map, lexical selection quality improves immediately.
    {
        const std::string ft_path = "data/fasttext/wiki.simple.vec";
        if (std::filesystem::exists(ft_path)) {
            std::cout << "[BabyMode] FastText vectors found at " << ft_path
                      << " — loading in background...\n" << std::flush;
            auto* ft_raw = fast_text_.get();
            std::thread([ft_raw, ft_path]() {
                auto start = std::chrono::steady_clock::now();
                bool ok = ft_raw->loadPretrainedVectors(ft_path);
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::steady_clock::now() - start).count();
                if (ok) {
                    std::cout << "[FastText] Loaded " << ft_raw->vocabSize()
                              << " word vectors in " << elapsed << "s — "
                              << "lexical selection now grounded.\n" << std::flush;
                } else {
                    std::cerr << "[FastText] Failed to load vectors from "
                              << ft_path << " after " << elapsed << "s.\n"
                              << std::flush;
                }
            }).detach();
        } else {
            std::cout << "[BabyMode] FastText vectors not found at " << ft_path
                      << " — running with seed vocabulary only ("
                      << fast_text_->vocabSize() << " words).\n" << std::flush;
        }
    }
    grammar_engine_ = std::make_unique<yuki::language::GrammarEngine>(fast_text_.get(), nullptr);
    
    // Create LogicVerificationLayer for research plan verification (ASP/Clingo integration)
    auto logic_verification = std::make_shared<yuki::logic::LogicVerificationLayer>(
        std::shared_ptr<yuki::language::GrammarEngine>(grammar_engine_.get(), [](auto*){}),
        nullptr,  // CausalGraph - not available yet
        nullptr,  // SafetyGovernor - not available yet
        nullptr   // CapabilityIntrospector - not available yet
    );
    logic_verification_layer_ = logic_verification;
    std::cout << "[BabyMode] LogicVerificationLayer created.\n";
    
    native_synthesizer_ = std::make_unique<yuki::language::NativeSentenceSynthesizer>(grammar_engine_.get());
    language_bootstrap_ = std::make_unique<yuki::brain::language::LanguageBootstrap>(cmf_ ? cmf_->hdcSemanticGraph() : nullptr);
    if (!language_bootstrap_->initialize()) {
        std::cout << "[BabyMode] LanguageBootstrap initialized with builtin defaults.\n";
    }
    episodic_grounding_ = std::make_unique<yuki::brain::language::EpisodicLanguageGrounding>(
        cmf_ ? cmf_->episodicStore() : nullptr, language_bootstrap_.get());

    // Wire NativeMind components to BrocaTranslator for LLM-free fallback
    broca_translator_->setGrammarEngine(grammar_engine_.get());
    broca_translator_->setNativeSentenceSynthesizer(native_synthesizer_.get());
    broca_translator_->setWernickeComprehender(wernicke_comprehender_.get());
    broca_translator_->setHdcGraph(cmf_ ? cmf_->hdcSemanticGraph() : nullptr);

    // Register the native synthesizer as a first-class GenerationRouter
    // backend. No external/SYCL/CPU LLM slots are provided, so the router's
    // chain terminates at native grammar synthesis and externalFallbacks stays
    // at 0 — i.e. Yuki can answer with no LLM loaded.
    native_backend_ = std::make_shared<yuki::brain::language::NativeGenerationBackend>(
        native_synthesizer_.get(), grammar_engine_.get());
    generation_router_ = std::make_shared<yuki::brain::language::GenerationRouter>(
        nullptr, nullptr, nullptr, native_backend_, nullptr,
        std::make_shared<yuki::language::GeneratorSelector>());
    std::cout << "[BabyMode] GenerationRouter registered with NativeGenerationBackend (LLM-free).\n";
    // NOTE: BrocaTranslator has no setLanguageBootstrap/setEpisodicGrounding —
    // those hooks exist only on TurnCoordinator.
    std::cout << "[BabyMode] NativeMind components wired to BrocaTranslator.\n";
    
    // ── Phase 3: Grounded Language Interface ────────────────────────────────
    latent_token_projector_ = std::make_unique<yuki::brain::language::LatentTokenProjector>();
    latent_token_projector_->setLatentWorldModel(latent_world_model_.get());
    latent_token_projector_->setGenerativeModel(generative_model_.get());
    std::cout << "[BabyMode] LatentTokenProjector created.\n";

    // Grounded Generation Backend
    auto grounded_backend = std::make_shared<yuki::brain::language::GroundedGenerationBackend>(
        std::shared_ptr<yuki::brain::language::LatentTokenProjector>(
            latent_token_projector_.get(), [](auto*){}));
    grounded_backend->setClarificationCallback(
        [this](const std::string& reason) {
            if (presence_shell_) {
                presence_shell_->show_clarification(reason);
            }
            return "Could you clarify? " + reason;
        });
    // Register grounded backend with GenerationRouter
    // (would need GenerationRouter API extension)
    std::cout << "[BabyMode] GroundedGenerationBackend created.\n";

    // Create the text encoder FIRST — Wernicke and TurnCoordinator capture it below.
    text_encoder_ = std::make_shared<yuki::perception::TextEncoder>();

    // Wernicke Comprehender (language → belief)
    wernicke_comprehender_ = std::make_unique<yuki::brain::language::WernickeComprehender>(
        local_llm_ ? local_llm_->getLocalTransformer().get() : nullptr,
        text_encoder_.get());
    std::cout << "[BabyMode] WernickeComprehender created.\n";
    
    // Phonological Loop (inner speech / verbal working memory)
    phonological_loop_ = std::make_unique<yuki::brain::language::PhonologicalLoop>(
        broca_translator_.get(), wernicke_comprehender_.get(), 
        cmf_ ? cmf_->hdcSemanticGraph() : nullptr);
    std::cout << "[BabyMode] PhonologicalLoop created.\n";
    
    // Cognitive Field (unifies everything)
    cognitive_field_ = std::make_unique<yuki::brain::field::CognitiveField>();
    std::cout << "[BabyMode] CognitiveField created.\n";
    
    // Wire all components into CognitiveField
    if (vse_) cognitive_field_->setVSE(vse_.get());
    cognitive_field_->setGenerativeModel(generative_model_.get());
    cognitive_field_->setMemoryFabric(cmf_.get());
    cognitive_field_->setEpisodicStore(cmf_->episodicStore());
    cognitive_field_->setHdcGraph(cmf_->hdcSemanticGraph());
    cognitive_field_->setBrocaTranslator(broca_translator_.get());
    cognitive_field_->setWernickeComprehender(wernicke_comprehender_.get());
    cognitive_field_->setPhonologicalLoop(phonological_loop_.get());
    cognitive_field_->setPolicyEngine(policy_engine_.get());
    cognitive_field_->setHPC(hpc_.get());
    cognitive_field_->setAwakeConsolidation(awake_consolidation_.get());
    cognitive_field_->setDriveSystem(continuous_drives_.get());
    cognitive_field_->setSelfModel(nullptr);  // wired later via loadFromCMF
    // YNC (NeuromorphicSimulator) is deliberately not wired: it is a CPU
    // showpiece with no consumer of its output, so leaving it null is a
    // decision, not an unfinished TODO (audit §6).
    cognitive_field_->setYNC(nullptr);
    
    // Create and wire CuriosityEngine
    auto curiosity_engine = std::make_unique<yuki::brain::autonomy::CuriosityEngine>();
    cognitive_field_->setCuriosityEngine(curiosity_engine.get());
    curiosity_engine_ = std::move(curiosity_engine);
    std::cout << "[BabyMode] CuriosityEngine created and wired.\n";
    
    // Create and wire RichSelfModel
    rich_self_model_ = std::make_unique<yuki::brain::self::RichSelfModel>();
    cognitive_field_->setSelfModel(rich_self_model_.get());
    std::cout << "[BabyMode] RichSelfModel created and wired to CognitiveField.\n";
    
    // Wire components into their dependencies
    hpc_->setGenerativeModel(generative_model_.get());
    hpc_->setLatentWorldModel(latent_world_model_.get());
    policy_engine_->setGenerativeModel(generative_model_.get());
    policy_engine_->setVSE(vse_.get());
    policy_engine_->setLatentWorldModel(latent_world_model_.get());
    policy_engine_->setLatentMPCPlanner(latent_mpc_planner_.get());
    
    // Phase 1-2: Wire LatentWorldModel, CausalWorldModel, MPC Planner
    latent_world_model_->setGenerativeModel(generative_model_.get());
    causal_world_model_->setGenerativeModel(generative_model_.get());
    causal_world_model_->setLatentWorldModel(latent_world_model_.get());
    latent_mpc_planner_->setLatentWorldModel(latent_world_model_.get());
    latent_mpc_planner_->setCausalWorldModel(causal_world_model_.get());
    latent_mpc_planner_->setPolicyEngine(policy_engine_.get());
    
    awake_consolidation_->setEpisodicStore(cmf_->episodicStore());
    awake_consolidation_->setHdcGraph(cmf_->hdcSemanticGraph());
    if (auto* dmc = cmf_->differentialMemoryController()) {
        awake_consolidation_->setDMC(dmc);
    }
    awake_consolidation_->setVSE(vse_.get());
    
    // CognitiveManifold wiring
    if (vse_) cognitive_manifold_->setVSE(vse_.get());
    cognitive_manifold_->setHDC(cmf_->hdcSemanticGraph());
    cognitive_manifold_->setYNC(nullptr);  // see YNC note above — intentionally off
    
    // Start the cognitive field (begins multi-timescale processing)
    cognitive_field_->start();
    std::cout << "[BabyMode] CognitiveField started.\n";
    
    // Start awake consolidation
    awake_consolidation_->start();
    std::cout << "[BabyMode] AwakeConsolidationEngine started.\n";
    
    // Continuous drives don't have a start/stop - they're passive ODEs
    std::cout << "[BabyMode] ContinuousDriveSystem ready.\n";
    
    // TurnCoordinatorAdapter (replaces old TurnCoordinator pipeline)
    turn_adapter_ = std::make_unique<yuki::predictive::TurnCoordinatorAdapter>();
    turn_adapter_->setCognitiveField(cognitive_field_.get());
    turn_adapter_->setBrocaTranslator(broca_translator_.get());
    turn_adapter_->setWernickeComprehender(wernicke_comprehender_.get());
    turn_adapter_->setSelfModel(rich_self_model_.get());
    std::cout << "[BabyMode] TurnCoordinatorAdapter created.\n";
    
    std::cout << "[BabyMode] Phase 1 initialization complete.\n" << std::flush;

    // ── Scrapling runtime: SmartScraper + KnowledgeExtractor ────────────────────────
    smart_scraper_ = std::make_unique<SmartScraper>();

    // Give the knowledge daemon a real fetcher: bare topics resolve through a
    // stable lookup URL, explicit URLs are fetched as-is, and the page text is
    // distilled into facts. Without this the daemon could only register topics
    // while logging success (audit §1: stub daemon).
    knowledge_.setFetcher([this](const std::string& request) -> std::string {
        if (!smart_scraper_ || request.empty()) return {};
        std::string url = request;
        if (url.rfind("http://", 0) != 0 && url.rfind("https://", 0) != 0) {
            std::string query;
            for (char c : request) {
                if (std::isalnum(static_cast<unsigned char>(c))) query += c;
                else if (c == ' ') query += '+';
            }
            url = "https://en.wikipedia.org/wiki/Special:Search?search=" + query;
        }
        try {
            ScrapedPage page = smart_scraper_->scrape(url, 10000);

            // Autonomous web learning loop: turn the fetched text into HDC
            // triples so future turns retrieve it. Two extractors run:
            //   1. pattern rules (WebKnowledgeExtractor) - fast, low weight
            //   2. OpenIE subprocess (KnowledgeExtractor) - higher precision
            // Both ingest at low provenance weight; confirmation raises it.
            if (web_extractor_ && !page.clean_text.empty()) {
                size_t n = web_extractor_->extractAndIngest(page.clean_text, url);
                if (n) std::cout << "[BabyMode] Web extractor ingested " << n
                                 << " triples from " << url << "\n";
            }
            if (knowledge_extractor_ && cmf_ && !page.clean_text.empty()) {
                auto ek = knowledge_extractor_->extract_from_text(page.clean_text, request);
                size_t n = 0;
                for (const auto& t : ek.spo_triples) {
                    yuki::memory::TripleWeight tw;
                    tw.base_weight = t.confidence > 0.0f ? t.confidence : 0.3f;
                    tw.sources.push_back("web_openie");
                    if (cmf_->hdcSemanticGraph()->ingestProposition(
                            t.subject, t.predicate, t.object, tw)) {
                        ++n;
                    }
                }
                if (n) std::cout << "[BabyMode] OpenIE ingested " << n
                                 << " triples from " << url << "\n";
            }

            return page.clean_text;
        } catch (...) {
            return {};
        }
    });
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
    web_extractor_ = std::make_unique<yuki::knowledge::WebKnowledgeExtractor>(
        cmf_ ? cmf_->hdcSemanticGraph() : nullptr);

    // ATOMIC-2020: if a dataset is present, load its causal/social if-then triples
    // into the HDC graph asynchronously in the background so startup is instant.
    atomic_adapter_ = std::make_unique<yuki::knowledge::AtomicAdapter>();
    {
        const std::string atomic_dir =
            yuki::paths::generic(yuki::paths::dataPath("atomic2020"));
        const std::string atomic_tsv =
            yuki::paths::generic(yuki::paths::dataPath("atomic2020.tsv"));
        std::string dataset;
        if (std::filesystem::exists(atomic_tsv)) dataset = atomic_tsv;
        else if (std::filesystem::exists(atomic_dir)) dataset = atomic_dir;

        if (!dataset.empty() && cmf_) {
            auto* graph = cmf_->hdcSemanticGraph();
            size_t current_edges = graph ? graph->getEdgeCount() : 0;
            if (graph && current_edges < 1000000) {
                std::cout << "[BabyMode] Background worker seeding ATOMIC-2020 ("
                          << current_edges << " edges present)...\n" << std::flush;
                std::thread([this, dataset, graph]() {
                    size_t ingested = 0;
                    graph->beginTransaction();
                    auto stats = atomic_adapter_->parseStream(
                        dataset, [graph, &ingested](const yuki::knowledge::AtomicAssertion& a) {
                            yuki::memory::TripleWeight tw;
                            tw.base_weight = a.confidence > 0.0f ? a.confidence : 0.8f;
                            tw.sources.push_back(a.source.empty() ? "atomic2020" : a.source);
                            if (graph->ingestProposition(a.subject, a.relation, a.object, tw)) {
                                if (++ingested % 25000 == 0) {
                                    graph->commitTransaction();
                                    graph->beginTransaction();
                                    std::cout << "[Background Knowledge] Ingested " << ingested
                                              << " ATOMIC-2020 triples...\n" << std::flush;
                                }
                            }
                            return true;
                        });
                    graph->commitTransaction();
                    std::cout << "[Background Knowledge] ATOMIC-2020 complete: parsed " << stats.parsed
                              << ", ingested " << ingested << " triples (HDC edges now: "
                              << graph->getEdgeCount() << ")\n" << std::flush;
                    atomic_adapter_->loadFromFile(dataset);
                }).detach();
            } else if (graph) {
                std::cout << "[BabyMode] ATOMIC-2020 already fully seeded (" << current_edges << " edges present)\n" << std::flush;
                std::thread([this, dataset]() {
                    atomic_adapter_->loadFromFile(dataset);
                }).detach();
            }
        }
    }

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



    // ── Phase 3: SleepThread (idle cognitive maintenance) ──────────────────────────
    sleep_thread_ = std::make_unique<yuki::sleep::SleepThread>();
    sleep_thread_->setEpisodicStore(cmf_->episodicStore());
    sleep_thread_->setSemanticGraph(cmf_->hdcSemanticGraph());
    sleep_thread_->setVSE(vse_.get());
    sleep_thread_->setCMF(cmf_.get());   // wire DMC + T3 access
    
    // Phase 1-2: Wire CausalWorldModel to SleepThread for counterfactual replay
    sleep_thread_->setCausalWorldModel(causal_world_model_.get());
    sleep_thread_->start();

    // ── Wire DMC to SleepThread ──
    if (auto* dmc = cmf_->differentialMemoryController()) {
        sleep_thread_->setDifferentialMemoryController(dmc);
        std::cout << "[BabyMode] DMC wired to SleepThread.\n";
    }

    // ── MemoryReconsolidationEngine (Stage 3.3: Sleep consolidation) ──────────
    memory_reconsolidation_ = std::make_unique<yuki::memory::MemoryReconsolidationEngine>();
    yuki::memory::ReconsolidationConfig mre_cfg;
    mre_cfg.nmda_threshold = 0.5f;
    mre_cfg.learning_rate = 0.1f;
    mre_cfg.decay_rate = 0.01f;
    mre_cfg.interference_window = 1000.0f;
    mre_cfg.max_trace_strength = 100;
    mre_cfg.enable_extinction = true;
    mre_cfg.max_traces = 100000;
    mre_cfg.promotion_threshold = 0.7f;
    memory_reconsolidation_->set_config(mre_cfg);
    bool mre_ok = memory_reconsolidation_->initialize(
        cmf_->episodicStore(),
        cmf_->hdcSemanticGraph(),
        cmf_->differentialMemoryController()
    );
    if (mre_ok) {
        std::cout << "[BabyMode] MemoryReconsolidationEngine initialized.\n";
    } else {
        std::cerr << "[BabyMode] MemoryReconsolidationEngine initialization failed.\n";
    }

    // Wire MemoryReconsolidationEngine to SleepThread
    sleep_thread_->setMemoryReconsolidationEngine(memory_reconsolidation_.get());
    std::cout << "[BabyMode] MemoryReconsolidationEngine wired to SleepThread.\n";

    // ── DistillationExtractor (Phase 0.5: Training data export during sleep) ───────
    distillation_extractor_ = std::make_unique<yuki::sleep::DistillationExtractor>(cmf_->episodicStore());
    sleep_thread_->setDistillationExtractor(distillation_extractor_.get());
    
    // Phase 0.2: Post-distillation callback - trigger LoRA training and hot-reload
    sleep_thread_->setPostDistillationCallback(
        [this](const std::string& sft_path, const std::string& dpo_path) {
            // Do not stack trainings: one run at a time.
            bool expected = false;
            if (!trainingActive_->compare_exchange_strong(expected, true)) {
                std::cout << "[BabyMode] Training already in progress; skipping this request.\n";
                return;
            }

            std::cout << "[BabyMode] Post-distillation callback triggered\n";
            std::cout << "[BabyMode] SFT: " << sft_path << "\n";
            std::cout << "[BabyMode] DPO: " << dpo_path << "\n";

            // Resolve every input now, on the caller's thread, and hand the
            // worker a self-contained job. The transformer is held through a
            // shared_ptr so the background thread can never touch a destroyed
            // BabyMode/LocalLLM (audit §1: the old lambda captured `this` and
            // detached, so it could outlive the object and race inference).
            auto transformer = local_llm_ ? local_llm_->getLocalTransformer() : nullptr;
            const std::string train_script =
                yuki::paths::generic(yuki::paths::scriptsPath("train_local_transformer.py"));
            const std::string base_model =
                yuki::paths::generic(yuki::paths::dataPath("models/Qwen3.8-27B-UD-IQ1_S.gguf"));
            const std::string model_dir =
                yuki::paths::generic(yuki::paths::dataPath("models/"));

            auto trainingActive = trainingActive_;   // own the flag, don't capture `this`
            std::thread([transformer, trainingActive, sft_path, dpo_path, train_script,
                         base_model, model_dir]() {
                try {
                    std::string cmd = "python \"" + train_script + "\" "
                                     "--sft \"" + sft_path + "\" "
                                     "--dpo \"" + dpo_path + "\" "
                                     "--model \"" + base_model + "\" "
                                     "--output \"" + model_dir + "\" "
                                     "--merge";

                    std::cout << "[BabyMode] Starting training: " << cmd << "\n";
                    int result = std::system(cmd.c_str());

                    if (result == 0) {
                        std::cout << "[BabyMode] Training completed successfully\n";
                        // Find the latest merged model
                        std::string latest_model;

                        for (const auto& entry : std::filesystem::directory_iterator(model_dir)) {
                            if (entry.is_regular_file() && entry.path().filename().string().find("Qwen3.8-27B-Yuki-") == 0) {
                                if (latest_model.empty() ||
                                    std::filesystem::last_write_time(entry.path()) >
                                    std::filesystem::last_write_time(latest_model)) {
                                    latest_model = entry.path().string();
                                }
                            }
                        }

                        if (!latest_model.empty() && transformer) {
                            std::cout << "[BabyMode] Hot-reloading model: " << latest_model << "\n";
                            // reloadModel() takes the model lock, so it waits for
                            // any in-flight inference instead of freeing weights
                            // out from under it.
                            bool reload_ok = transformer->reloadModel(latest_model);
                            if (reload_ok) {
                                std::cout << "[BabyMode] Model hot-reload SUCCESS\n";
                            } else {
                                std::cerr << "[BabyMode] Model hot-reload FAILED\n";
                            }
                        }
                    } else {
                        std::cerr << "[BabyMode] Training failed with code: " << result << "\n";
                    }
                } catch (const std::exception& e) {
                    std::cerr << "[BabyMode] Post-distillation callback error: " << e.what() << "\n";
                }
                trainingActive->store(false);
            }).detach();
        });
    
    std::cout << "[BabyMode] DistillationExtractor wired to SleepThread.\n";

    // ── CognitiveEventBus (Grok Bot pattern) ────────────────────────────────────
    yuki::system::CognitiveEventBus::Config bus_cfg;
    bus_cfg.max_queue_size = 10000;
    bus_cfg.max_subscribers_per_topic = 64;
    bus_cfg.enable_async_delivery = true;
    bus_cfg.flush_interval = std::chrono::milliseconds(10);
    yuki::system::CognitiveEventBus::instance().initialize(bus_cfg);
    std::cout << "[BabyMode] CognitiveEventBus initialized.\n";

    // ── TranscriptMirror (Grok Bot pattern) ────────────────────────────────────
    yuki::memory::TranscriptMirror::Config tm_cfg;
    tm_cfg.file_path = "data/brain/transcript_mirror.json";
    tm_cfg.max_entries = 10000;
    tm_cfg.flush_on_append = true;
    transcript_mirror_ = std::make_unique<yuki::memory::TranscriptMirror>(tm_cfg);
    if (!transcript_mirror_->initialize()) {
        std::cerr << "[BabyMode] TranscriptMirror initialization failed.\n";
    } else {
        std::cout << "[BabyMode] TranscriptMirror initialized.\n";
    }

    // ── ToolPermissionEngine (Grok Bot pattern) ──────────────────────────────
    yuki::system::ToolPermissionEngine::Config tp_cfg;
    tp_cfg.config_file = "data/brain/tool_permissions.json";
    tp_cfg.auto_persist = true;
    tp_cfg.admin_ceiling = yuki::system::ToolPermission::ASK; // Constitutional default
    tool_permission_engine_ = std::make_unique<yuki::system::ToolPermissionEngine>(tp_cfg);
    if (!tool_permission_engine_->initialize()) {
        std::cerr << "[BabyMode] ToolPermissionEngine initialization failed.\n";
    } else {
        std::cout << "[BabyMode] ToolPermissionEngine initialized.\n";
    }

    // ── TruthGate (Stage 2.3: Anti-hallucination) ────────────────────────────
    yuki::truth::TruthGateConfig tg_cfg;
    tg_cfg.min_verification_confidence = 0.7f;
    tg_cfg.partial_threshold = 0.4f;
    tg_cfg.emit_uncertain = true;
    tg_cfg.suppress_unverified = false;
    tg_cfg.suppress_unverifiable = false;
    truth_gate_ = std::make_unique<yuki::truth::TruthGate>(tg_cfg);
    if (!truth_gate_->initialize(cmf_.get())) {
        std::cerr << "[BabyMode] TruthGate initialization failed.\n";
    } else {
        std::cout << "[BabyMode] TruthGate initialized.\n";
    }

    // Set avatar prompt callback for ASK permissions
    // Safe default: without a shell there is no way to ask, so refuse.
    // With a shell, the question is surfaced and the user's typed y/n answer
    // resolves the pending permission via ToolPermissionEngine's pending queue.
    tool_permission_engine_->set_avatar_prompt_callback(
        [this](const std::string& tool, const std::string& action, const std::string& context,
               yuki::system::ToolPermissionEngine::PermissionCallback callback) {
            std::string prompt = "[Permission] " + tool + " wants to " + action;
            if (!context.empty()) prompt += " (" + context + ")";
            prompt += ". Allow? [y/n]";

            // Queue the prompt; the user's next clear yes/no utterance resolves
            // it via resolvePendingPermission(). The callback must outlive this
            // call, so it is moved into the queue rather than invoked here.
            {
                std::lock_guard<std::mutex> lock(permMutex_);
                pendingPermissions_.push_back({prompt, std::move(callback)});
            }

            if (presence_shell_) {
                presence_shell_->show_clarification(prompt);
            } else {
                std::cout << "\n" << prompt << "\n";
            }
        });

    // ── Phase 3: IdleLearningPipeline (sequential child-learning while idle) ─────────
    idle_learning_pipeline_ = std::make_unique<yuki::organism::IdleLearningPipeline>();
    
    // Create OrganismController for IdleLearningPipeline
    auto organism_controller = std::make_unique<yuki::organism::OrganismController>();
    
    // Create ResearchPlanner, ToolRegistry, SandboxExecuteTool
    auto research_planner = std::make_unique<yuki::research::ResearchPlanner>();
    auto tool_registry = std::make_unique<yuki::research::ToolRegistry>();
    auto sandbox_execute = std::make_unique<yuki::research::SandboxExecuteTool>();
    
    // Create DevelopmentalEngine
    auto dev_engine = std::make_unique<ync::DevelopmentalEngine>();
    
    // Create SemanticBinder
    auto semantic_binder = std::make_unique<yuki::brain::memory::SemanticBinder>();
    semantic_binder->setSemanticGraph(cmf_->hdcSemanticGraph());
    
    // Initialize IdleLearningPipeline
    bool ilp_ok = idle_learning_pipeline_->initialize(
        organism_controller.get(),
        research_planner.get(),
        tool_registry.get(),
        sandbox_execute.get(),
        cmf_.get(),
        cmf_->hdcSemanticGraph(),
        dev_engine.get(),
        semantic_binder.get()
    );
    
    if (ilp_ok) {
        idle_learning_pipeline_->start();
        std::cout << "[BabyMode] IdleLearningPipeline initialized and started.\n";
    } else {
        std::cerr << "[BabyMode] IdleLearningPipeline initialization failed.\n";
    }
    
    // Wire IdleLearningPipeline to OrganismController (ownership taken below, before the tick thread)
    organism_controller->setIdleLearningPipeline(idle_learning_pipeline_.get());
    
    // Take ownership BEFORE starting the tick thread so the thread never sees a null pointer.
    organism_controller_ = std::move(organism_controller);
    
    // Store ownership of remaining dependencies (they must outlive the pipeline)
    research_planner_ = std::move(research_planner);
    tool_registry_ = std::move(tool_registry);
    sandbox_execute_ = std::move(sandbox_execute);
    dev_engine_ = std::move(dev_engine);
    semantic_binder_ = std::move(semantic_binder);
    
    // Wire ResearchPlanner with ToolRegistry
    research_planner_->setToolRegistry(tool_registry_.get());
    
    // Create and wire ProactiveEngine with ResearchPlanner and ToolRegistry
    proactive_engine_ = std::make_unique<yuki::organism::ProactiveEngine>(
        &organism_controller_->drives(),
        &organism_controller_->metabolism(),
        nullptr  // WakeDetector - optional, can be set later
    );
    proactive_engine_->setResearchPlanner(research_planner_.get());
    proactive_engine_->setToolRegistry(tool_registry_.get());
    proactive_engine_->setCMF(cmf_.get());
    proactive_engine_->setMetacognition(nullptr);  // MetacognitionEngine not yet available here
    proactive_engine_->start();
    std::cout << "[BabyMode] ProactiveEngine created, wired, and started.\n";
    
    // Wire LogicVerificationLayer to ResearchPlanner (ASP/Clingo integration for deduction)
    if (logic_verification_layer_) {
        research_planner_->setLogicVerificationLayer(logic_verification_layer_);
        std::cout << "[BabyMode] LogicVerificationLayer wired to ResearchPlanner.\n";
    }
    
    // Start OrganismController tick thread (~100ms cadence). The dt passed to
    // tick() is measured from the wall clock rather than hardcoded to 0.1, so
    // scheduling jitter and any leniency in sleep_for do not accumulate into
    // drift between the simulated organism time and real time.
    organism_tick_running_ = true;
    organism_tick_thread_ = std::thread([this]() {
        using namespace std::chrono_literals;
        auto last = std::chrono::steady_clock::now();
        while (organism_tick_running_.load()) {
            std::this_thread::sleep_for(100ms);
            const auto now = std::chrono::steady_clock::now();
            double dt = std::chrono::duration<double>(now - last).count();
            last = now;
            if (dt <= 0.0 || dt > 1.0) dt = 0.1; // clamp pathological gaps
            if (organism_controller_) organism_controller_->tick(dt);
        }
    });
    std::cout << "[BabyMode] OrganismController tick thread started.\n";
}

BabyMode::~BabyMode() {
    // Stop Phase 1: Continuous Cognition Substrate
    if (awake_consolidation_) awake_consolidation_->stop();
    std::cout << "[BabyMode] AwakeConsolidationEngine stopped.\n";
    
    if (cognitive_field_) cognitive_field_->stop();
    std::cout << "[BabyMode] CognitiveField stopped.\n";
    
    if (phonological_loop_) phonological_loop_->stop();
    std::cout << "[BabyMode] PhonologicalLoop stopped.\n";
    
    // Stop ProactiveEngine
    if (proactive_engine_) {
        proactive_engine_->stop();
        std::cout << "[BabyMode] ProactiveEngine stopped.\n";
    }
    
    // Stop OrganismController tick thread
    organism_tick_running_ = false;
    if (organism_tick_thread_.joinable()) {
        organism_tick_thread_.join();
    }
    
    // Stop CognitiveEventBus
    yuki::system::CognitiveEventBus::instance().shutdown();
    std::cout << "[BabyMode] CognitiveEventBus shutdown.\n";
    
    // Stop ToolPermissionEngine
    if (tool_permission_engine_) {
        tool_permission_engine_->shutdown();
        std::cout << "[BabyMode] ToolPermissionEngine shutdown.\n";
    }
    
    // Stop TranscriptMirror
    if (transcript_mirror_) {
        transcript_mirror_->shutdown();
        std::cout << "[BabyMode] TranscriptMirror shutdown.\n";
    }
    
    // Stop TruthGate
    if (truth_gate_) {
        // TruthGate doesn't have explicit shutdown, just log
        std::cout << "[BabyMode] TruthGate shutdown.\n";
    }
    
    // Stop Phase 3 SleepThread first (pure cognitive maintenance, no disk flush)
    if (sleep_thread_) sleep_thread_->stop();
    // Stop Phase 4.5 first (may flush index to disk)
    // Scrapling components (no background thread, just cleanup)
    knowledge_extractor_.reset();
    smart_scraper_.reset();
    LearningIngestor::instance().stop();  // drain queue before DB closes
    mobileServer_.stop();
    vision().initialize(nullptr, nullptr, nullptr);
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
CommandRouter&       BabyMode::router()     { return router_; }    SpeechToTextRuntime& BabyMode::stt()        { return sttRuntime_; }


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

void BabyMode::setPredictiveEngine(std::shared_ptr<yuki::MemoryStore> memory_store,
                                    std::shared_ptr<yuki::UserModel> user_model) {
    memory_store_ = memory_store;
    user_model_ = user_model;

    // UserMemory: persistent personal facts
    if (!user_memory_) {
        user_memory_ = std::make_shared<UserMemory>();
    }

    // Per-turn self-improvement hook
    if (turn_adapter_) {
        if (cmf_) per_turn_hook_.setHdcGraph(cmf_->hdcSemanticGraph());
        if (vse_) per_turn_hook_.setVSE(vse_.get());
        per_turn_hook_.setWeightCalibratorRegistry(&calibrator_registry_);
        turn_adapter_->setPerTurnLearningHook(&per_turn_hook_);
        turn_adapter_->setWeightCalibratorRegistry(&calibrator_registry_);
        yuki::core::Logger::instance().info("[BabyMode] PerTurnLearningHook + WeightCalibratorRegistry wired.
";
    }

    if (turn_adapter_ && cmf_) {
        turn_adapter_->setMemoryFabric(cmf_.get());
        yuki::core::Logger::instance().info("[BabyMode] CMF wired to TurnCoordinatorAdapter.
";
    }
    if (turn_adapter_ && text_encoder_) {
        turn_adapter_->setTextEncoder(text_encoder_.get());
        yuki::core::Logger::instance().info("[BabyMode] TextEncoder wired to TurnCoordinatorAdapter.
";
    }
    if (turn_adapter_ && cmf_ && vse_) {

        ige_ = std::make_unique<yuki::memory::InformationGainEngine>(&vse_->generativeModel());
        air_ = std::make_unique<yuki::memory::ActiveInferenceRetrieval>(ige_.get(), cmf_->episodicStore(), cmf_->hdcSemanticGraph());
        turn_adapter_->setActiveInferenceRetrieval(air_.get());
        yuki::core::Logger::instance().info("[BabyMode] AIR and IGE wired to TurnCoordinatorAdapter.
";
    }

    // Phase 0: VSE Bootstrap Training
    if (vse_) {
        vse_trainer_ = std::make_unique<yuki::brain::inference::VseBootstrapTrainer>(vse_.get());
        int injected = vse_trainer_->injectAll();
        yuki::core::Logger::instance().info("[BabyMode] VSE bootstrap complete: " + std::to_string(injected) + " examples injected.
";
    }

    sentence_builder_ = std::make_unique<yuki::language::SentenceBuilder>();
    if (turn_adapter_) {
        turn_adapter_->setSentenceBuilder(sentence_builder_.get());
        yuki::core::Logger::instance().info("[BabyMode] SentenceBuilder wired to TurnCoordinatorAdapter.
";
    }

    if (turn_adapter_ && presence_shell_) {
        turn_adapter_->setPresenceShell(presence_shell_);
    }

    // Wire OrganismController for metabolism/economy
    if (turn_adapter_ && organism_controller_) {
        turn_adapter_->setOrganismController(organism_controller_.get());
        yuki::core::Logger::instance().info("[BabyMode] OrganismController wired to TurnCoordinatorAdapter.
";
    }

    // Wire KnowledgeDaemon relevance filter to SelfModel gaps
    if (turn_adapter_ && turn_adapter_->getSelfModel()) {
        auto gaps = turn_adapter_->getSelfModel()->getGaps(0.5f);
        knowledge_.setRelevanceTopics(gaps);
        yuki::core::Logger::instance().info("[BabyMode] KnowledgeDaemon relevance filter wired with " + std::to_string(gaps.size()) + " gap topics
";
    }

}


void BabyMode::setLocalLLM(std::unique_ptr<yuki::LocalLLM> llm) {
    local_llm_ = std::move(llm);
    // Rewire components that depend on LocalTransformer
    if (local_llm_ && local_llm_->getLocalTransformer()) {
        if (broca_translator_) {
            broca_translator_->setLocalTransformer(local_llm_->getLocalTransformer().get());
        }
        if (wernicke_comprehender_) {
            wernicke_comprehender_->setLocalTransformer(local_llm_->getLocalTransformer().get());
        }
    }
}



namespace {
struct DmcEvalResult {
    bool has_dmc = false;
    yuki::brain::memory::DecisionToken token;
};

DmcEvalResult evaluateDmc(
    yuki::brain::inference::VariationalStateEstimator* vse,
    yuki::brain::memory::CognitiveMemoryFabric* cmf,
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

    if (decision == yuki::brain::memory::DMCDecision::PROMOTE) {
        if (auto* episodic = cmf->episodicStore()) {
            auto snaps = episodic->queryRecentSnapshots(1, false);
            if (!snaps.empty() && cmf->hdcSemanticGraph()) {
                std::string ep_id = "ep_" + std::to_string(snaps[0].id);
                cmf->hdcSemanticGraph()->ingestProposition(ep_id, "promoted_from_episodic");
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
    yuki::brain::memory::CognitiveMemoryFabric* cmf,
    const yuki::brain::memory::DecisionToken& token,
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

bool BabyMode::resolvePendingPermission(const std::string& utterance, std::string* reaction) {
    PendingPermission pending;
    bool granted = false;
    {
        std::lock_guard<std::mutex> lock(permMutex_);
        if (pendingPermissions_.empty()) return false;

        // Normalize: strip whitespace and lowercase so "Y", "yes!", " No " work.
        std::string answer;
        for (char c : utterance) {
            if (!std::isspace(static_cast<unsigned char>(c)))
                answer += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        static const char* const kYes[] = {"y", "yes", "yeah", "yep", "ok", "okay",
                                           "sure", "allow", "granted", nullptr};
        static const char* const kNo[]  = {"n", "no", "nope", "deny", "denied",
                                           "never", "reject", "rejected", nullptr};
        bool is_yes = false, is_no = false;
        for (int i = 0; kYes[i]; ++i) if (answer == kYes[i]) is_yes = true;
        for (int i = 0; kNo[i];  ++i) if (answer == kNo[i])  is_no  = true;
        if (!is_yes && !is_no) return false;  // not an answer: leave the prompt queued

        pending = std::move(pendingPermissions_.front());
        pendingPermissions_.pop_front();
        granted = is_yes;
    }

    if (pending.resolve) {
        pending.resolve(granted ? yuki::system::ToolPermission::ALWAYS
                                : yuki::system::ToolPermission::NEVER);
    }
    std::cout << "[Permission] " << (granted ? "granted" : "denied")
              << ": " << pending.prompt << "\n";
    if (reaction) *reaction = granted ? "Permission granted." : "Permission denied.";
    return true;
}

// Shared typed/voice pipeline. Both public turn entry points used to carry a
// near-identical ~90-line copy of this logic (audit §2).
yuki::predictive::AdapterTurnResult BabyMode::runTurnPipeline(const std::string& input,
                                                              bool is_voice) {
    DmcEvalResult dmc_res = evaluateDmc(vse_.get(), cmf_.get(), input);

    yuki::predictive::AdapterTurnResult result;
    if (turn_adapter_ && cognitive_field_ && cognitive_field_->isRunning()) {
        result = turn_adapter_->processTurn(input, std::chrono::seconds(5));
    } else {
        // Fallback to the legacy coordinator if the adapter is not ready.
        yuki::MultiModalInput mmi;
        mmi.text = input;
        // Typed input carries no speech transcript; voice input is its own.
        mmi.speech_transcript = is_voice ? input : "";

        yuki::TurnResult old_result;
        if (coordinator_) {
            {
                std::lock_guard<std::mutex> lock(*coordinator_->current_state().state_mutex);
                coordinator_->current_state().last_raw_input = input;
                coordinator_->current_state().last_normalized_input = input;
            }
            old_result = coordinator_->run_turn(mmi);

            result.turn_committed         = old_result.turn_committed;
            result.can_act                = old_result.can_act;
            result.requires_clarification = old_result.requires_clarification;
            result.response_text          = old_result.response_text;
            result.response_tone          = old_result.response_tone;
            result.confidence             = old_result.confidence;
            result.clarify_dimensions     = old_result.clarify_dimensions;
            result.veto                   = old_result.veto;
            result.template_family        = old_result.template_family;
            result.template_slot          = old_result.template_slot;
        }
    }

    // ── TruthGate: Verify claims before emission (Stage 2.3) ────────────────
    // LLM-free generation fallback: if the cognitive pipeline produced no
    // response text, route through GenerationRouter. With no LLM slots wired
    // its chain ends at NativeGenerationBackend (GrammarEngine +
    // NativeSentenceSynthesizer), so externalFallbacks never increments.
    if (result.response_text.empty() && !result.veto && generation_router_) {
        yuki::brain::language::GenerationRequest req;
        req.prompt = input;
        req.allowExternalFallback = false;
        req.taskType = yuki::brain::language::GenerationTaskType::CHAT;
        auto gen = generation_router_->generateWithSelector(req);
        if (gen.success && !gen.text.empty()) {
            result.response_text = gen.text;
            result.response_tone = "neutral";
        }
    }

    if (truth_gate_ && !result.response_text.empty() && !result.veto &&
        !result.requires_clarification) {
        result.response_text = truth_gate_->process_response(result.response_text);
    }

    if (dmc_res.has_dmc) {
        bool success = !result.veto && result.turn_committed;
        recordDmcOutcome(cmf_.get(), dmc_res.token, success, 1.0f);
    }

    {
        std::lock_guard<std::mutex> lock(session_.historyMutex);
        session_.history.push_back({is_voice ? "Voice" : "You", input, is_voice});
    }

    if (transcript_mirror_) {
        transcript_mirror_->append(is_voice ? yuki::memory::TurnSource::USER_VOICE
                                            : yuki::memory::TurnSource::USER_TYPED,
                                   input);
    }

    if (presence_shell_) {
        if (result.veto) {
            presence_shell_->show_safety_warning(result.response_text);
        } else if (result.requires_clarification) {
            presence_shell_->show_clarification(result.response_text);
        } else if (!result.response_text.empty()) {
            presence_shell_->show_response(result.response_text, result.response_tone);
        } else {
            presence_shell_->show_response(
                ResponseResolver::instance().resolve("fallback.not_sure"), "neutral");
        }
    } else {
        if (result.veto) {
            std::cout << "\nYuki: [VETO] " << result.response_text << "\n";
        } else if (result.requires_clarification || !result.response_text.empty()) {
            std::cout << "\nYuki: " << result.response_text << "\n";
        } else {
            std::cout << "\nYuki: "
                      << ResponseResolver::instance().resolve("fallback.not_sure") << "\n";
        }
    }

    deliverResponse(result.response_text);

    // ── SystemExecutor bridge ───────────────────────────────────────────────
    // can_act + VSE MAP COMMAND(2)/ABORT(7) dispatches through ToolAdapter for
    // BOTH typed and voice turns (the voice path previously dropped these).
    if (result.can_act && !result.veto && !result.requires_clarification) {
        int map_intent = 0;
        if (vse_) {
            map_intent = static_cast<int>(vse_->currentBelief().getMAP().intent);
        }
        if (map_intent == 2 || map_intent == 7) { // COMMAND or ABORT
            std::cout << "[Executor] COMMAND intent -- executing: " << input << "\n";
            yuki::TurnResult tool_ctx;
            tool_ctx.turn_committed = result.turn_committed;
            tool_ctx.response_text  = result.response_text;
            yuki::ToolAdapter adapter;
            adapter.execute(input, tool_ctx);
        }
    }

    if (transcript_mirror_ && !result.response_text.empty()) {
        transcript_mirror_->append(yuki::memory::TurnSource::YUKI_RESPONSE,
                                   result.response_text);
    }

    // Episodic language grounding: record what this turn said and how it went
    // so word↔concept confidence can be calibrated over time. This subsystem
    // was previously constructed but never fed.
    if (episodic_grounding_) {
        yuki::brain::language::LanguageEpisode ep;
        ep.id      = "turn-" + std::to_string(
                         std::chrono::system_clock::now().time_since_epoch().count());
        ep.text    = input;
        ep.context = is_voice ? "voice" : "typed";
        ep.outcome = (result.veto || result.requires_clarification || result.response_text.empty())
                         ? "failure" : "success";
        ep.timestamp = std::chrono::system_clock::now();
        episodic_grounding_->recordEpisode(ep);
    }

    return result;
}

BabyOutputState BabyMode::process(const std::string& input) {
    // A pending ASK-permission prompt consumes clear yes/no answers before any
    // cognitive processing, so the answer cannot be misinterpreted as a turn.
    std::string perm_reaction;
    if (resolvePendingPermission(input, &perm_reaction)) {
        if (presence_shell_) presence_shell_->show_response(perm_reaction, "neutral");
        else                 std::cout << "\nYuki: " << perm_reaction << "\n";
        BabyOutputState perm_out;
        perm_out.reaction = perm_reaction;
        return perm_out;
    }

    // Step 3.5: SelfModel correction hook
    if (coordinator_ && coordinator_->getSelfModel()) {
        std::string lower = input;
        std::transform(lower.begin(), lower.end(), lower.begin(),
            [](unsigned char c){ return std::tolower(c); });
            
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

    // === Phase 1: shared cognitive turn (audit §2 dedup) ==================
    yuki::predictive::AdapterTurnResult result = runTurnPipeline(input, /*is_voice=*/false);

    std::cout << "[BABY] veto=" << result.veto 
              << " can_act=" << result.can_act 
              << " requires_clarification: " << result.requires_clarification << "\n";

    BabyOutputState out;
    out.reaction = result.response_text;
    return out;
}

BabyOutputState BabyMode::processVoice(const std::string& transcript) {
    // Voice answers to a pending permission prompt take the same short-circuit
    // as typed answers.
    std::string perm_reaction;
    if (resolvePendingPermission(transcript, &perm_reaction)) {
        if (presence_shell_) presence_shell_->show_response(perm_reaction, "neutral");
        else                 std::cout << "\nYuki: " << perm_reaction << "\n";
        BabyOutputState perm_out;
        perm_out.reaction = perm_reaction;
        return perm_out;
    }

    // Shared typed/voice pipeline (audit §2 dedup).
    yuki::predictive::AdapterTurnResult result = runTurnPipeline(transcript, /*is_voice=*/true);

    BabyOutputState out;
    out.reaction = result.response_text;
    return out;
}

BabyTurnResult BabyMode::processUserTurn(const UserTurnInput& input) {
    // Publish to Global Workspace so all GW subscribers observe this turn
    if (use_global_workspace_) {
        bool is_voice = (input.source == InputSourceKind::VOICE_FINAL ||
                         input.source == InputSourceKind::VOICE_PARTIAL);
        publishTurnToGW(input.text, is_voice);
    }

    BabyOutputState out = process(input.text);
    BabyTurnResult turnResult;
    turnResult.handled      = true;
    turnResult.responseText = out.reaction;
    turnResult.commandName  = input.text;

    // â”€â”€ Phase 4: Feed turn to BackgroundLearningEngine â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    if (ble_)       ble_->ingestUserTurn(input.text);
    // ── Phase 4.5: Reset idle timer on every turn ───────────────────────────
    // (Replaces the retired distiller_->bumpActivity() hook: SleepThread owns
    // the idle timer now, and without this it would consolidate mid-conversation.)
    if (sleep_thread_) sleep_thread_->signalActivity();
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
    }    std::cout << "[BabyMode] Mass curriculum loader self-destructed.\n";
}


