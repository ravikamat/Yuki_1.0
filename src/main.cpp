// main.cpp
// Entry point for Yuki_1.0 Stage 1.
//
// Responsibilities:
//   - Print startup banner
//   - Load feature flags
//   - Launch Presence Shell (Stage A.2)
//   - Share Yuki processing path with shell via thread
//   - Maintain globally shared chat history
//   - Handle graceful bidirectional shutdown with UI polish

#include "YukiUtils.h"
#include "BabyMode.h"
#include "PresenceShell.h"
#include "DetailView.h"
#include "AvatarBody.h"
#include "brain/core/ResponseResolver.h"
#include <windows.h>
#include <iostream>
#include <string>
#include <thread>
#include <mutex>
#include <atomic>
#include <vector>
#include <chrono>

#include "SessionState.h"
#include "brain/database/DatabaseManager.h"
#include "brain/memory/UserMemory.h"
#include "brain/predictive/predictive_turn_engine.h"
#include "brain/predictive/stream_workers.h"
#include "brain/predictive/sqlite_memory_store.h"
#include "input/conditioning/SignalConditioningLayer.h"
#include "infrastructure/CoreBus.h"
#include "infrastructure/GlobalWorkspace.h"
#include "infrastructure/ModuleRegistry.h"
#include "infrastructure/ControlPlane.h"

// Helper to gracefully unblock std::getline when quitting from another thread
static void injectEnterToConsole() {
    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
    if (hStdin == INVALID_HANDLE_VALUE) return;
    
    INPUT_RECORD ir[2] = {};
    DWORD written = 0;
    
    ir[0].EventType = KEY_EVENT;
    ir[0].Event.KeyEvent.bKeyDown = TRUE;
    ir[0].Event.KeyEvent.wVirtualKeyCode = VK_RETURN;
    ir[0].Event.KeyEvent.uChar.AsciiChar = '\r';
    
    ir[1].EventType = KEY_EVENT;
    ir[1].Event.KeyEvent.bKeyDown = FALSE;
    ir[1].Event.KeyEvent.wVirtualKeyCode = VK_RETURN;
    ir[1].Event.KeyEvent.uChar.AsciiChar = '\r';
    
    WriteConsoleInputA(hStdin, ir, 2, &written);
}

static void performShutdown(SessionState& session, PresenceShell& shell, bool shellRunning, DetailView& detailView, bool detailRunning, AvatarBody& avatar, bool avatarRunning, const std::string& source) {
    session.quit = true; 
    std::string goodbyeStr = ResponseResolver::instance().resolve("system.goodbye_graceful");
    
    {
        std::lock_guard<std::mutex> lock(session.historyMutex);
        session.history.push_back({"Yuki", goodbyeStr, false});
    }

    std::cout << "\n[Yuki] " << goodbyeStr << "\n";
    
    if (shellRunning) {
        if (source == "terminal") {
            shell.show(SW_SHOW); 
        }
        shell.setState("SHUTTING DOWN");
        shell.postRefresh();
    }
    if (detailRunning) {
        detailView.setContent(goodbyeStr);
        detailView.postRefresh();
    }
    
    if (avatarRunning) {
        avatar.setState("SPEAKING");
        avatar.setSpeech(ResponseResolver::instance().resolve("system.goodbye_short"));
        avatar.postRefresh();
    }

    // Give user time to read the goodbye message
    std::this_thread::sleep_for(std::chrono::seconds(2));

    if (shellRunning) {
        shell.closeShell(true); 
    }
    if (detailRunning) {
        detailView.closeView(true);
    }
    if (avatarRunning) {
        avatar.closeView(true);
    }

    if (source == "shell" || source == "detail") {
        injectEnterToConsole(); // unblock terminal cin
    }
}

// -------------------------------------------------
// Banner
// -------------------------------------------------
static void printBanner() {
    std::cout << "\n";
    std::cout << "+================================================+\n";
    std::cout << "|     " << ResponseResolver::instance().resolve("system.banner_title") << "          |\n";
    std::cout << "|     " << ResponseResolver::instance().resolve("system.banner_sub") << "          |\n";
    std::cout << "|     " << ResponseResolver::instance().resolve("system.banner_preview") << "       |\n";
    std::cout << "+================================================+\n";
    std::cout << ResponseResolver::instance().resolve("system.launching_shell") << "\n";
    std::cout << ResponseResolver::instance().resolve("system.camera_preview_open") << "\n";
    std::cout << ResponseResolver::instance().resolve("system.quit_instruction") << "\n";
    std::cout << "\n";
}

// -------------------------------------------------
// main
// -------------------------------------------------
int main() {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    loadFeatureFlags();
    printBanner();

    // PHASE 1: Core infrastructure (must complete before any threads spawn)
    auto& db = DatabaseManager::instance();
    if (!db.init("data/brain/yuki.db")) {
        std::cerr << "[FATAL] Database initialization failed\n";
        return 1;
    }

    SessionState session;
    BabyMode baby(session);
    PresenceShell shell(session);

    baby.runMassCurriculumIfNeeded();

    // ── Infrastructure Layer ────────────────────────────────────────────────
    yuki::infra::ControlPlane::instance().init();
    yuki::gw::GlobalWorkspace::instance().init(0.25f, 10);
    yuki::gw::GlobalWorkspace::instance().start();

    auto& reg = yuki::infra::ModuleRegistry::instance();
    reg.registerModule({"BabyMode",              "1.0", {},                         {"USER_TURN","ACTION_COMPLETED","SYSTEM_STATE"}, {"INTENT_CLASSIFIED","POLICY_SELECTED","MEMORY_RETRIEVED","EMOTION_EXTRACTED"}});
    reg.registerModule({"TurnCoordinator",        "1.0", {"BabyMode","VSE"},          {"INTENT_CLASSIFIED","POLICY_SELECTED"},         {"USER_TURN","PERCEPTION_FRAME","BELIEF_UPDATE"}});
    reg.registerModule({"VSE",                    "1.0", {"SignalConditioningLayer"}, {"BELIEF_UPDATE","POLICY_SELECTED"},             {"PERCEPTION_FRAME"}});
    reg.registerModule({"CMF",                    "1.0", {"TurnCoordinator"},         {"MEMORY_RETRIEVED"},                            {"INTENT_CLASSIFIED","ACTION_COMPLETED"}});
    reg.registerModule({"NeuralSpine",            "1.0", {"BabyMode"},               {},                                             {"USER_TURN","INTENT_CLASSIFIED"}});
    reg.registerModule({"EmotionSystem",          "1.0", {"BabyMode"},               {"EMOTION_EXTRACTED"},                           {"PERCEPTION_FRAME","USER_TURN"}});
    reg.registerModule({"SignalConditioningLayer","1.0", {"BabyMode"},               {"PERCEPTION_FRAME"},                            {}});
    reg.registerModule({"ControlPlane",           "1.0", {},                         {"SYSTEM_STATE"},                                {}});
    reg.registerModule({"BackgroundLearningEngine","1.0", {"BabyMode","CMF"},          {"META_COGNITIVE"},                              {"USER_TURN","PERCEPTION_FRAME"}});
    reg.registerModule({"YukiSelfModel",           "1.0", {"BabyMode","TurnCoordinator"},{},                                           {"ACTION_COMPLETED","BELIEF_UPDATE","USER_TURN"}});
    reg.registerModule({"MemoryDistiller",          "1.0", {"BabyMode","CMF","VSE"},     {"META_COGNITIVE"},                              {"SYSTEM_STATE","ACTION_COMPLETED"}});

    // ── Wire up the Predictive Turn Engine ──
    auto userMemory = std::make_shared<UserMemory>();
    auto user_model = std::make_shared<yuki::UserModel>();
    auto memory_store = std::make_shared<yuki::SqliteMemoryStore>(DatabaseManager::instance(), userMemory);

    auto coordinator = std::make_unique<yuki::TurnCoordinator>(user_model);
    coordinator->register_stream(std::make_unique<yuki::E1FastStream>());
    coordinator->register_stream(std::make_unique<yuki::E2SemanticStream>());
    coordinator->register_stream(std::make_unique<yuki::E3DeepStream>());

    // ── Active Inference: Variational State Estimator ──
    auto vse = std::make_unique<yuki::inference::VariationalStateEstimator>();

    yuki::conditioning::SignalConditioningLayer scl(baby.subsystems());
    scl.bindEar(&baby.ear());
    scl.bindCamera(&baby.camera());
    scl.bindScreen(&baby.screen());
    scl.bindPredictiveEngine(coordinator.get());
    scl.bindVariationalEstimator(vse.get());
    coordinator->bindVariationalEstimator(vse.get());
    scl.start();

    // ── CoreBus subscriptions (logging / future decoupling) ─────────────────
    yuki::gw::CoreBus::instance().subscribe(yuki::gw::Topic::PERCEPTION_FRAME, "VSE",
        [](const yuki::gw::Message&) { /* audit path — VSE updated directly by SCL */ });
    yuki::gw::CoreBus::instance().subscribe(yuki::gw::Topic::PERCEPTION_FRAME, "TurnCoordinator",
        [](const yuki::gw::Message&) { /* reserved for future decoupling */ });

    // VSE must outlive coordinator and SCL, so transfer ownership to BabyMode
    baby.setVariationalEstimator(std::move(vse));
    // Wire KnowledgeDaemon → TurnCoordinator BEFORE transferring ownership
    coordinator->setKnowledgeDaemon(&baby.knowledgeDaemon());
    
    // Step 3.5: Load SelfModel state from CMF if available
    if (baby.cmFabric() && coordinator->getSelfModel()) {
        coordinator->getSelfModel()->loadFromCMF(baby.cmFabric());
    }
    
    baby.setPredictiveEngine(std::move(coordinator), memory_store, user_model);
    // Wire UserMemory → TurnCoordinator so it persists personal facts & injects them into LLM prompts
    baby.setUserMemory(userMemory);


    // ── ControlPlane: start monitor + mark system IDLE ──────────────────────
    yuki::infra::ControlPlane::instance().start();
    yuki::infra::ControlPlane::instance().transition(yuki::infra::SystemState::IDLE);
    baby.setPresenceShell(&shell);
    std::atomic<bool> shellRunning{false};

    DetailView detailView;
    std::atomic<bool> detailRunning{false};

    AvatarBody avatar;
    std::atomic<bool> avatarRunning{false};

    shell.setProcessCallback([&](const std::string& input) -> std::string {
        if (input == "quit") {
            std::thread([&]() {
                performShutdown(session, shell, shellRunning, detailView, detailRunning, avatar, avatarRunning, "shell");
            }).detach();
            return "__QUIT_SIGNAL_SENT__";
        }
        
        if (avatarRunning) {
            avatar.setState("THINKING");
            avatar.setSpeech("");
            avatar.postRefresh();
        }
        
        // Process asynchronously on a background worker thread so the UI message thread does not freeze!
        // NOTE: BabyMode.process() internally appends {"You", input} to g_history.
        //       Do NOT push it here — it would appear twice in the shell.
        std::thread([&, input]() {
            BabyOutputState result;
            result = baby.process(input);
            
            std::cout << "\n[Shell Input]: " << input << "\n";
            std::cout << "Yuki: " << result.reaction << "\n";
            if (flags().show_terminal_trace) {
                std::cout << baby.tracer().renderFull();
            }
            std::cout << "You: "; 
            
            if (shellRunning) {
                shell.postRefresh();
            }
            
            if ((result.reaction.size() > 150 || result.reaction.find("vision") != std::string::npos || result.reaction.find("focus") != std::string::npos) && detailRunning) {
                detailView.setContent(result.reaction);
                detailView.postRefresh();
                detailView.show(SW_SHOW);
            }
        }).detach();
        
        return "__REFRESHED_VIA_SYNC__";
    });

    // Set authoritative speech callback
    baby.setAvatarCallback([&](const std::string& state, const std::string& speech) {
        if (avatarRunning) {
            avatar.setState(state);
            avatar.setSpeech(speech);
            avatar.postRefresh();
        }
        if (shellRunning) {
            shell.postRefresh();
        }
    });

    shell.setSyncCallback([&]() {
        std::lock_guard<std::mutex> lock(session.historyMutex);
        shell.rebuildHistory(session.history);
    });

    shell.setFocusCallback([&]() {
        if (avatarRunning && avatar.getState() == "IDLE") {
            avatar.setState("LISTENING");
            avatar.postRefresh();
        }
    });

    shell.setSubsystems(&baby.subsystems());
    shell.setSubsystemCallback([&]() {
        if (detailRunning) {
            detailView.postRefresh();
        }
    });

    baby.router().setUIActionCallback([&](const std::string& action) {
        if (action == "open_chat") {
            if (shellRunning) {
                shell.show(SW_SHOW);
                shell.postRefresh();
            }
        } else if (action == "open_detail") {
            if (detailRunning) {
                detailView.show(SW_SHOW);
                detailView.postRefresh();
            }
        } else if (action == "open_avatar") {
            if (avatarRunning) {
                avatar.show(SW_SHOW);
            }
        } else if (action == "close_avatar") {
            if (avatarRunning) {
                avatar.closeView(false);
            }
        }
    });

    // ── Wire STT real-time voice → shell UI ─────────────────────────────
    // Partial transcript: live "typing" animation in the input box
    baby.stt().setPartialTranscriptCallback([&](const std::string& partial) {
        std::cout << "[Voice PARTIAL]: \"" << partial << "\"\n";
        if (shellRunning) {
            shell.postSetVoiceDraftText(partial);
        }
    });

    // Final transcript: commit draft text and auto-submit as if user pressed Enter
    baby.stt().setTranscriptCallback([&](const std::string& text) {
        std::cout << "[Voice FINAL]: \"" << text << "\"\n";
        if (shellRunning) {
            shell.postSetVoiceDraftText(text);      // show final text
            shell.postCommitVoiceDraftText();        // lock it in
        }
        // Process through BabyMode on a worker thread
        std::thread([&, text]() {
            baby.processVoice(text);
            if (shellRunning) shell.postRefresh();
        }).detach();
    });

    // NOTE: do NOT call baby.subsystems().setChangeCallback() here.
    // BabyMode's constructor already wires that to syncRuntimesWithSubsystems().
    // Overriding it here would break runtime start/stop on button toggle.

    // ── One-shot "all systems ready" announcement ────────────────────────────
    // Fires once when STT is listening AND camera is running AND mic is active.
    // Gives Yuki a natural boot greeting — spoken aloud + shown in the shell.
    std::atomic<bool> readyAnnounced{false};
    std::thread readyWatcher([&]() {
        using namespace std::chrono_literals;

        // Wait for the UI shell to be up first (up to 10s)
        for (int i = 0; i < 200 && !shellRunning; ++i)
            std::this_thread::sleep_for(50ms);

        // Push the MobileServer URL into the shell's IP label as soon as UI is live
        if (shellRunning) {
            std::string url = baby.mobileServer().localUrl();
            if (url.empty()) url = ResponseResolver::instance().resolve("system.server_offline");
            shell.setServerUrl(url);
        }

        // Wait up to 60s for STT to be listening (mic + voice ready).
        // When the Python daemon is active it owns the mic directly,
        // so EarRuntime never starts — don't gate on it.
        auto startTime = std::chrono::steady_clock::now();
        for (int i = 0; i < 1200 && !session.quit; ++i) {
            std::this_thread::sleep_for(50ms);

            bool sttReady = baby.stt().getState() == SttState::LISTENING ||
                            baby.stt().getState() == SttState::CAPTURING_UTTERANCE;

            // Fire as soon as STT is live, OR 15s hard fallback
            auto elapsed = std::chrono::steady_clock::now() - startTime;
            bool timeout = elapsed > std::chrono::seconds(15);

            if (sttReady || timeout) {
                if (!readyAnnounced.exchange(true)) {
                    std::this_thread::sleep_for(600ms);
                    baby.announceReady();
                    if (shellRunning) shell.postRefresh();
                }
                break;
            }
        }
    });

    std::thread uiThread([&]() {
        bool shellOk = shell.create(GetModuleHandleA(nullptr));
        bool detailOk = detailView.create(GetModuleHandleA(nullptr));
        bool avatarOk = avatar.create(GetModuleHandleA(nullptr));
        
        if (shellOk) {
            shellRunning = true;
            shell.show(SW_SHOW);
        }
        if (detailOk) {
            detailRunning = true;
            detailView.show(SW_HIDE); // Hidden by default
        }
        if (avatarOk) {
            avatarRunning = true;
            avatar.show(SW_SHOW);
        }
        
        if (shellOk || detailOk || avatarOk) {
            MSG msg = {0};
            while (GetMessage(&msg, NULL, 0, 0)) {
                if (msg.message == WM_KEYDOWN) {
                    if (msg.wParam == VK_F1) { shell.setState("IDLE"); continue; }
                    if (msg.wParam == VK_F2) { shell.setState("LISTENING"); continue; }
                    if (msg.wParam == VK_F3) { shell.setState("INTERPRETING"); continue; }
                    if (msg.wParam == VK_F4) { shell.setState("SPEAKING"); continue; }
                }
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
        
        shellRunning = false;
        detailRunning = false;
        avatarRunning = false;
    });

    while (!session.quit) {
        if (avatarRunning) {
            avatar.setState("LISTENING");
            avatar.postRefresh();
        }
        
        std::cout << "You: ";
        std::string line;

        if (!std::getline(std::cin, line)) {
            std::cout << "\n[Yuki] " << ResponseResolver::instance().resolve("system.stream_closed") << "\n";
            break;
        }

        if (session.quit) {
            break;
        }

        if (line == "quit") {
            performShutdown(session, shell, shellRunning, detailView, detailRunning, avatar, avatarRunning, "terminal");
            break;
        }



        if (line.empty()) continue;

        std::string reactionText;
        {
            if (avatarRunning) {
                avatar.setState("THINKING");
                avatar.setSpeech("");
                avatar.postRefresh();
            }
            
            BabyOutputState result;
            // NOTE: BabyMode.process() internally appends {"You", line} to g_history.
            //       Do NOT push it here — it would appear twice in the shell.
            result = baby.process(line);
            
            reactionText = result.reaction;
            std::cout << "Yuki: " << result.reaction << "\n";
            if (flags().show_terminal_trace) {
                std::cout << baby.tracer().renderFull();
            }
            std::cout << "\n";
        }
        
        if (shellRunning) {
            shell.postRefresh();
        }
        
        if ((reactionText.size() > 150 || reactionText.find("vision") != std::string::npos || reactionText.find("focus") != std::string::npos) && detailRunning) {
            detailView.setContent(reactionText);
            detailView.postRefresh();
            detailView.show(SW_SHOW);
        }
    } // end of while (!session.quit)

    session.quit = true; // Ensure readyWatcher thread exits its loop
    if (readyWatcher.joinable()) {
        readyWatcher.join();
    }

    if (uiThread.joinable()) {
        uiThread.join();
    }

    return 0;
}
