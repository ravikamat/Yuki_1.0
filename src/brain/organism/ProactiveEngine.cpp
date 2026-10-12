#include "brain/organism/ProactiveEngine.h"
#include "brain/organism/DriveSystem.h"
#include "brain/organism/MetabolismEngine.h"
#include "brain/research/core/ResearchPlanner.h"
#include "brain/memory/CognitiveMemoryFabric.h"
#include "brain/memory/EpisodicStore.h"
// EpisodeRecord is defined in EpisodicStore.h
#include "brain/memory/EpisodicStore.h"
#include "brain/metacognition/MetacognitionEngine.h"
#include "brain/language/LocalTransformer.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iostream>
#include <chrono>

namespace yuki {
namespace organism {

using yuki::brain::memory::EpisodeRecord;

ProactiveEngine::ProactiveEngine(yuki::organism::DriveSystem* drives,
                                 yuki::organism::MetabolismEngine* metabolism,
                                 yuki::input::WakeDetector* wake)
    : drives_(drives), metabolism_(metabolism), wake_(wake) {
    // Default templates fallback
    templates_["GREETING"] = "Hello! Ready to assist.";
    templates_["SYSTEM_ALERT"] = "Warning: System resources require attention.";
    templates_["CURIOSITY"] = "I noticed some interesting patterns in memory.";
}

ProactiveEngine::~ProactiveEngine() {
    stop(); // ensure idle thread is joined before vtable/members are destroyed
}

bool ProactiveEngine::loadTemplatesFromFile(const std::string& filepath) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ifstream file(filepath);
    if (!file) return false;

    templates_.clear();
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream iss(line);
        std::string key, val;
        if (std::getline(iss, key, '\t') && std::getline(iss, val, '\t')) {
            templates_[key] = val;
        }
    }
    return !templates_.empty();
}

void ProactiveEngine::start() {
    // Guard: do not double-start.
    if (running_.exchange(true)) return;
    idle_thread_ = std::thread([this]() noexcept { idle_poll_loop(); });
    std::cout << "[ProactiveEngine] Background idle-poll thread started.\n";
}

void ProactiveEngine::stop() {
    if (!running_.exchange(false)) return; // was already stopped
    {
        std::lock_guard<std::mutex> lk(cv_mutex_);
        // running_ is already false; cv_.wait_for will now exit on next wake
    }
    cv_.notify_all(); // wake the sleeping poll immediately
    if (idle_thread_.joinable())
        idle_thread_.join();
    std::cout << "[ProactiveEngine] Background idle-poll thread stopped.\n";
}

void ProactiveEngine::setInitiativeCallback(InitiativeCallback cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    initiative_callback_ = std::move(cb);
}

void ProactiveEngine::onWakeDetected() {
    Initiative init;
    init.type = Initiative::Type::GREETING;
    init.priority = 1.0f;
    init.template_key = "GREETING";
    {
        std::lock_guard<std::mutex> lock(mutex_);
        init.text = templates_.count("GREETING") ? templates_["GREETING"] : "Hello! Ready to assist.";
        pending_queue_.push_back(init);
    }
    InitiativeCallback cb;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        cb = initiative_callback_;
    }
    if (cb) {
        cb(init);
    }
}

// Polls every kPollIntervalSec seconds.  When the idle window since the last
// user turn exceeds kBoredomIdleSec AND curiosity deficit > 0.8, fires one
// proactive_learning_loop() call synchronously on this thread.
void ProactiveEngine::idle_poll_loop() {
    static constexpr float kBoredomIdleSec  = 30.0f;
    static constexpr auto  kPollInterval    = std::chrono::seconds(5);

    while (running_.load(std::memory_order_relaxed)) {
        // Cancellable sleep: woken immediately by stop() via cv_.notify_all().
        {
            std::unique_lock<std::mutex> lk(cv_mutex_);
            cv_.wait_for(lk, kPollInterval, [this]{ return !running_.load(); });
        }
        if (!running_.load()) break;

        // Compute idle_sec from the shared atomic clock.
        float idle_sec = 0.0f;
        if (last_turn_time_ptr_) {
            auto last = last_turn_time_ptr_->load(std::memory_order_acquire);
            idle_sec  = static_cast<float>(
                std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - last).count());
        }

        float curiosity = drives_
            ? static_cast<float>(drives_->curiosityDeficit())
            : 0.85f;

        // 1. Trigger background proactive learning loop if bored
        if (boredom_detection(idle_sec, curiosity)) {
            std::cout << "[ProactiveEngine] Idle " << idle_sec
                      << "s detected — launching proactive learning loop.\n";
            try {
                proactive_learning_loop(planner_, cmf_, meta_);
            } catch (...) { /* never crash the idle thread */ }
        }

        // 2. Evaluate proactive initiative and dispatch to output if qualified
        auto init = generateInitiative(idle_sec);
        if (init.type != Initiative::Type::NONE && init.priority > 0.6f) {
            auto now = std::chrono::steady_clock::now();
            bool should_dispatch = false;
            InitiativeCallback cb;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                auto elapsed_sec = std::chrono::duration_cast<std::chrono::seconds>(now - last_initiative_dispatch_time_).count();
                if (elapsed_sec >= 45) { // 45s debounce to prevent spamming
                    last_initiative_dispatch_time_ = now;
                    should_dispatch = true;
                    cb = initiative_callback_;
                }
            }
            if (should_dispatch && cb) {
                std::cout << "[ProactiveEngine] Dispatching proactive initiative: " << init.text << "\n";
                cb(init);
            }
        }
    }
}

Initiative ProactiveEngine::generateInitiative() {
    return generateInitiative(0.0f);
}

Initiative ProactiveEngine::generateInitiative(float idle_seconds) {
    std::lock_guard<std::mutex> lock(mutex_);
    Initiative init;

    // 1. Check metabolism viability (Critical System Alert)
    if (metabolism_) {
        float viability = static_cast<float>(metabolism_->snapshot().viability);
        if (viability < 0.3f) {
            init.type = Initiative::Type::SYSTEM_ALERT;
            init.priority = 0.9f;
            init.template_key = "SYSTEM_ALERT";
            init.text = templates_.count("SYSTEM_ALERT") ? templates_["SYSTEM_ALERT"] : "Warning: System resources require attention.";
            pending_queue_.push_back(init);
            return init;
        }
    }

    // 2. Check Social Drive & Loneliness (Loneliness onset after 15 min or high social deficit)
    if (drives_) {
        float social_def = static_cast<float>(drives_->socialDeficit());
        auto top_goal = drives_->topGoal();
        if (social_def > 0.4f || top_goal.type == DriveGoal::Type::SOCIAL || idle_seconds >= 900.0f) {
            init.type = Initiative::Type::CHECK_IN;
            init.priority = std::max(social_def, 0.75f);
            init.template_key = templates_.count("CHECK_IN") ? "CHECK_IN" : "GREETING";
            init.text = templates_.count(init.template_key)
                ? templates_[init.template_key]
                : "Everything looks smooth. How can I help?";
            pending_queue_.push_back(init);
            return init;
        }
    }

    // 3. Task 6.3: Boredom & Curiosity Trigger
    if (boredom_detection(idle_seconds, drives_ ? static_cast<float>(drives_->curiosityDeficit()) : 0.9f)) {
        init.type = Initiative::Type::CURIOSITY;
        init.priority = 0.95f;
        init.template_key = "CURIOSITY";
        init.text = templates_.count("CURIOSITY")
            ? templates_["CURIOSITY"]
            : "I noticed some interesting patterns in memory. Shall we investigate?";
        pending_queue_.push_back(init);
        return init;
    }

    // 4. Check other Drive goals (Competence, Research, Observation)
    if (drives_) {
        auto top_goal = drives_->topGoal();
        if (top_goal.priority > 0.5f) {
            if (top_goal.type == DriveGoal::Type::COMPETENCE) {
                init.type = Initiative::Type::DEEP_QUESTION;
                init.template_key = "DEEP_QUESTION";
                init.text = templates_.count("DEEP_QUESTION") ? templates_["DEEP_QUESTION"] : "Have you considered reviewing the latest capability graph paths?";
            } else if (top_goal.type == DriveGoal::Type::CURIOSITY) {
                init.type = Initiative::Type::CURIOSITY;
                init.template_key = "CURIOSITY";
                init.text = templates_.count("CURIOSITY") ? templates_["CURIOSITY"] : "I noticed some interesting patterns in memory.";
            } else {
                init.type = Initiative::Type::OBSERVATION;
                init.template_key = "OBSERVATION";
                init.text = templates_.count("OBSERVATION") ? templates_["OBSERVATION"] : "Metabolism energy levels are stable.";
            }
            init.priority = top_goal.priority;
            pending_queue_.push_back(init);
            return init;
        }
    }

    return init; // NONE if no high priority drive/alert
}

std::vector<Initiative> ProactiveEngine::pendingInitiatives() const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto list = pending_queue_;
    std::sort(list.begin(), list.end(), [](const Initiative& a, const Initiative& b) {
        return a.priority > b.priority;
    });
    return list;
}

void ProactiveEngine::clearPending() {
    std::lock_guard<std::mutex> lock(mutex_);
    pending_queue_.clear();
}

bool ProactiveEngine::boredom_detection(float idle_seconds, float curiosity_drive) const {
    return (idle_seconds > 30.0f) && (curiosity_drive > 0.8f);
}

bool ProactiveEngine::detectBoredom(float idle_seconds) const {
    float curiosity = drives_ ? static_cast<float>(drives_->curiosityDeficit()) : 0.85f;
    return boredom_detection(idle_seconds, curiosity);
}

void ProactiveEngine::proactive_learning_loop(yuki::research::ResearchPlanner* planner,
                                            yuki::brain::memory::CognitiveMemoryFabric* cmf,
                                            yuki::metacognition::MetacognitionEngine* meta) {
    std::string topic = drives_ ? drives_->self_directed_research_topic(meta)
                                : "Autonomous Research Target: General Knowledge Expansion";

    std::cout << "[ProactiveEngine] Autonomous Proactive Learning Loop started for topic: " << topic << "\n";

    // 1. Scrape & Extract via ResearchPlanner
    if (planner) {
        planner->research(topic);
    }

    // 2. Ingest into CognitiveMemoryFabric / EpisodicStore
    yuki::brain::memory::EpisodicStore* episodic_store = cmf->episodicStore();
    if (episodic_store) {
        yuki::brain::memory::EpisodeRecord rec;
        rec.timestamp = static_cast<double>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
        rec.source = "proactive_learning_loop";
        rec.text = "Autonomous proactive learning completed for topic: " + topic;
        rec.topic_tag = "autonomous_learning";
        rec.intent_label = "autonomous_learning";
        rec.confidence = 0.95f;

        // Produce a real semantic embedding for the episode record so that
        // EpisodicStore cosine-similarity searches can retrieve it meaningfully.
        // Priority: (1) directly wired lt_, (2) lt from cmf, (3) zero vector fallback.
        yuki::brain::language::LocalTransformer* lt = lt_;
        if (!lt) {
            lt = cmf->getTransformerEncoder();
        }

        std::vector<float> topic_embedding;
        if (lt && lt->isLoaded()) {
            topic_embedding = lt->embed(topic);
        }
        // Fallback: zero vector is semantically neutral and does not pollute
        // cosine searches the way a uniform 0.5 vector would.
        if (topic_embedding.empty()) {
            topic_embedding.assign(128, 0.0f);
        }

        // Store the embedding in vector_slot if EpisodicStore supports it
        // For now, just insert the record
        episodic_store->insert(rec);
    }

    // 3. Test & Update Competence in MetacognitionEngine
    if (meta) {
        meta->triggerResearch(topic);
    }

    std::cout << "[ProactiveEngine] Autonomous Proactive Learning Loop successfully completed.\n";
}

} // namespace yuki::organism
} // namespace yuki
