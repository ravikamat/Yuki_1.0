// NeuralSpine.cpp
// Yuki_1.0 — Master Cognitive Coordinator

#include "NeuralSpine.h"
#include <iostream>
#include <chrono>
#include <algorithm>
#include <cctype>

// Tick interval — refresh the world model every 2 seconds
static constexpr int TICK_INTERVAL_MS = 2000;

// ── Constructor / Destructor ───────────────────────────────────────────────

NeuralSpine::NeuralSpine(SubsystemControl& control,
                          EarRuntime&       mic,
                          MouthRuntime&     mouth,
                          ScreenRuntime*    screen,
                          CameraRuntime*    camera)
    : control_(control), mic_(mic), mouth_(mouth),
      screen_(screen), camera_(camera),
      memory_(40)  // keep last 40 turns (~20 exchange pairs)
{}

NeuralSpine::~NeuralSpine() {
    stop();
}

// ── Lifecycle ──────────────────────────────────────────────────────────────

void NeuralSpine::start() {
    if (running_.load()) return;
    running_ = true;

    // Prime the world model immediately before the tick thread starts
    {
        auto snap = std::make_shared<WorldSnapshot>(snapshotBuilder_.build(control_, mic_, mouth_, screen_, camera_));
        std::lock_guard<std::mutex> lock(worldMutex_);
        latestWorld_ = snap;
    }

    tickThread_ = std::thread([this]() { tickLoop(); });
}

void NeuralSpine::stop() {
    running_ = false;
    if (tickThread_.joinable()) tickThread_.join();
}

void NeuralSpine::tickLoop() {
    while (running_.load()) {
        // Sleep in 100ms increments so we can exit promptly
        for (int i = 0; i < (TICK_INTERVAL_MS / 100) && running_.load(); ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        if (!running_.load()) break;

        auto snap = std::make_shared<WorldSnapshot>(snapshotBuilder_.build(control_, mic_, mouth_, screen_, camera_));
        {
            std::lock_guard<std::mutex> lock(worldMutex_);
            latestWorld_ = snap;
        }

        if (worldUpdateCb_) {
            worldUpdateCb_(*snap);
        }
    }
}

WorldSnapshot NeuralSpine::refreshWorld() {
    auto snap = std::make_shared<WorldSnapshot>(snapshotBuilder_.build(control_, mic_, mouth_, screen_, camera_));
    {
        std::lock_guard<std::mutex> lock(worldMutex_);
        latestWorld_ = snap;
    }
    return *snap;
}

// ── Accessors ──────────────────────────────────────────────────────────────

WorldSnapshot NeuralSpine::latestWorld() const {
    std::lock_guard<std::mutex> lock(worldMutex_);
    if (latestWorld_) return *latestWorld_;
    return WorldSnapshot{};
}

ConversationMemory& NeuralSpine::memory() {
    return memory_;
}

void NeuralSpine::setWorldUpdateCallback(WorldUpdateCallback cb) {
    worldUpdateCb_ = std::move(cb);
}

// ── Core Process ──────────────────────────────────────────────────────────

SpineOutput NeuralSpine::process(const SpineInput& input) {
    SpineOutput out;

    // 1. Record user turn into memory
    memory_.recordUser(input.text, input.isVoice);

    // 2. Get freshest world snapshot (use cached; background thread keeps it warm)
    WorldSnapshot world;
    {
        std::lock_guard<std::mutex> lock(worldMutex_);
        if (latestWorld_) world = *latestWorld_;
    }

    // If the snapshot is stale (>5s), force a fresh capture inline
    if (world.ageMs() > 5000) {
        world = refreshWorld();
    }

    // 3. Normalize input for scoring (lowercase, trimmed)
    std::string norm = input.text;
    std::transform(norm.begin(), norm.end(), norm.begin(), ::tolower);
    // trim
    size_t s = norm.find_first_not_of(" \t\r\n");
    size_t e = norm.find_last_not_of(" \t\r\n");
    norm = (s == std::string::npos) ? "" : norm.substr(s, e - s + 1);

    // 4. Score intent
    IntentResult intent = scorer_.score(norm, world);
    out.detectedIntent  = intent.kind;
    out.confidence      = intent.confidence;
    out.wasCommand      = intent.isCommand;

    // Log intent classification
    std::cout << "[NeuralSpine] Intent: " << static_cast<int>(intent.kind)
              << " confidence=" << intent.confidence
              << " keyword=" << intent.matchedKeyword << "\n";

    // 5. Generate response
    std::string response = engine_.generate(intent, world, memory_, input.text);
    out.responseText     = response;

    // 6. Record Yuki's response into memory
    memory_.recordYuki(response, false);

    return out;
}

void NeuralSpine::recordCommandResult(const std::string& userText,
                                       const std::string& responseText) {
    memory_.recordUser(userText, false);
    memory_.recordYuki(responseText, true);
}
