#include "YukiSelfModel.h"
#include "infrastructure/CoreBus.h"
#include "infrastructure/ModuleRegistry.h"
#include <sstream>
#include <fstream>
#include <iostream>
#include <cstdio>

using namespace yuki::self;
using yuki::gw::CoreBus;
using yuki::gw::Topic;
using yuki::gw::Message;

YukiSelfModel::YukiSelfModel() = default;
YukiSelfModel::~YukiSelfModel() { stop(); }

void YukiSelfModel::init(const std::string& db_path) {
    db_path_ = db_path;
    loadFromDb();
    std::cout << "[YukiSelfModel] Loaded " << domains_.size() << " domain records.\n";
}

void YukiSelfModel::subscribeToBus() {
    CoreBus::instance().subscribe(Topic::ACTION_COMPLETED, "YukiSelfModel",
        [this](const Message& msg) { onActionCompleted(msg); });
    CoreBus::instance().subscribe(Topic::BELIEF_UPDATE, "YukiSelfModel",
        [this](const Message& msg) { onBeliefUpdate(msg); });
    CoreBus::instance().subscribe(Topic::USER_TURN, "YukiSelfModel",
        [this](const Message& msg) { onUserTurn(msg); });
    yuki::infra::ModuleRegistry::instance().heartbeat("YukiSelfModel");
}

void YukiSelfModel::start() {
    running_     = true;
    save_thread_ = std::thread(&YukiSelfModel::saveLoop, this);
}

void YukiSelfModel::stop() {
    running_ = false;
    if (save_thread_.joinable()) save_thread_.join();
    if (dirty_.load()) saveToDb();
}

// ─────────────────────────────────────────────────────────────────────────────
// Queries
// ─────────────────────────────────────────────────────────────────────────────

std::string YukiSelfModel::getSelfSummary() const {
    std::lock_guard<std::mutex> lock(mtx_);
    std::ostringstream oss;
    size_t gaps = 0, strengths = 0;
    for (const auto& [t, d] : domains_) {
        if (d.is_gap) ++gaps;
        else if (d.expertise_score > 0.7f) ++strengths;
    }
    oss << "Yuki Self-Model: Domains=" << domains_.size()
        << " | Gaps=" << gaps << " | Strong=" << strengths << "\n";
    for (const auto& [t, d] : domains_) {
        oss << "  [" << t << "] exp=" << d.expertise_score
            << " conf=" << d.confidence
            << " n=" << d.interaction_count
            << (d.is_gap ? " [GAP]" : "") << "\n";
    }
    return oss.str();
}

std::vector<DomainExpertise> YukiSelfModel::getGaps() const {
    std::lock_guard<std::mutex> lock(mtx_);
    std::vector<DomainExpertise> r;
    for (const auto& [t, d] : domains_) if (d.is_gap) r.push_back(d);
    return r;
}

std::vector<DomainExpertise> YukiSelfModel::getStrengths() const {
    std::lock_guard<std::mutex> lock(mtx_);
    std::vector<DomainExpertise> r;
    for (const auto& [t, d] : domains_) if (d.expertise_score > 0.7f) r.push_back(d);
    return r;
}

float YukiSelfModel::getExpertise(const std::string& topic) const {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = domains_.find(topic);
    return (it == domains_.end()) ? 0.0f : it->second.expertise_score;
}

float YukiSelfModel::getConfidence(const std::string& topic) const {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = domains_.find(topic);
    return (it == domains_.end()) ? 0.0f : it->second.confidence;
}

bool YukiSelfModel::shouldLearn(const std::string& topic) const {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = domains_.find(topic);
    if (it == domains_.end()) return true;
    return it->second.is_gap || it->second.expertise_score < 0.4f;
}

// ─────────────────────────────────────────────────────────────────────────────
// Updates
// ─────────────────────────────────────────────────────────────────────────────

void YukiSelfModel::recordInteraction(const std::string& topic, bool success, float confidence) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto& d = domains_[topic];
    d.topic = topic;
    ++d.interaction_count;
    if (success) ++d.success_count;
    float success_rate = static_cast<float>(d.success_count) /
                         static_cast<float>(d.interaction_count);
    d.expertise_score = 0.9f * d.expertise_score + 0.1f * success_rate;
    d.confidence      = 0.9f * d.confidence      + 0.1f * confidence;
    d.last_updated    = std::chrono::system_clock::now();
    d.is_gap          = (d.confidence < 0.3f && d.interaction_count < 5);
    dirty_.store(true);
}

void YukiSelfModel::recordBeliefEntropy(float entropy) {
    std::lock_guard<std::mutex> lock(mtx_);
    for (auto& [t, d] : domains_) {
        d.confidence = std::clamp(d.confidence * (1.0f - entropy * 0.05f), 0.0f, 1.0f);
        d.is_gap     = (d.confidence < 0.3f && d.interaction_count < 5);
    }
    dirty_.store(true);
}

// ─────────────────────────────────────────────────────────────────────────────
// Bus handlers
// ─────────────────────────────────────────────────────────────────────────────

void YukiSelfModel::onActionCompleted(const Message& msg) {
    // Parse intent index from payload JSON → map to topic name
    static const char* intent_names[] = {
        "unknown","query","command","tutorial",
        "emotional","clarification","meta","abort"
    };
    std::string topic = "general";
    float confidence  = 0.5f;

    size_t ip = msg.payload_json.find("\"intent\":");
    if (ip != std::string::npos) {
        int iv = 0;
        if (std::sscanf(msg.payload_json.c_str() + ip, "\"intent\":%d", &iv) == 1 &&
            iv >= 0 && iv < 8) {
            topic = intent_names[iv];
        }
    }
    size_t cp = msg.payload_json.find("\"intent_conf\":");
    if (cp != std::string::npos) {
        std::sscanf(msg.payload_json.c_str() + cp, "\"intent_conf\":%f", &confidence);
    }
    recordInteraction(topic, true, confidence);
}

void YukiSelfModel::onBeliefUpdate(const Message& /*msg*/) {
    recordBeliefEntropy(0.1f);
}

void YukiSelfModel::onUserTurn(const Message& /*msg*/) {
    recordInteraction("general", true, 0.5f);
}

// ─────────────────────────────────────────────────────────────────────────────
// Persistence
// ─────────────────────────────────────────────────────────────────────────────

void YukiSelfModel::saveLoop() {
    while (running_) {
        std::this_thread::sleep_for(std::chrono::seconds(30));
        if (dirty_.load()) {
            saveToDb();
            dirty_.store(false);
        }
        yuki::infra::ModuleRegistry::instance().heartbeat("YukiSelfModel");
    }
}

void YukiSelfModel::saveToDb() {
    std::lock_guard<std::mutex> lock(mtx_);
    std::ofstream ofs(db_path_ + ".txt", std::ios::trunc);
    if (!ofs) return;
    for (const auto& [t, d] : domains_) {
        ofs << t << "|"
            << d.expertise_score  << "|"
            << d.confidence       << "|"
            << d.interaction_count << "|"
            << d.success_count    << "\n";
    }
}

void YukiSelfModel::loadFromDb() {
    std::lock_guard<std::mutex> lock(mtx_);
    std::ifstream ifs(db_path_ + ".txt");
    if (!ifs) return;
    std::string line;
    while (std::getline(ifs, line)) {
        auto p1 = line.find('|');
        if (p1 == std::string::npos) continue;
        auto p2 = line.find('|', p1 + 1);
        auto p3 = line.find('|', p2 + 1);
        auto p4 = line.find('|', p3 + 1);
        std::string t = line.substr(0, p1);
        auto& d  = domains_[t];
        d.topic  = t;
        try {
            if (p2 != std::string::npos)
                d.expertise_score   = std::stof(line.substr(p1 + 1, p2 - p1 - 1));
            if (p3 != std::string::npos)
                d.confidence        = std::stof(line.substr(p2 + 1, p3 - p2 - 1));
            if (p4 != std::string::npos)
                d.interaction_count = std::stoull(line.substr(p3 + 1, p4 - p3 - 1));
            if (p4 != std::string::npos)
                d.success_count     = std::stoull(line.substr(p4 + 1));
        } catch (...) { /* corrupt line — skip */ }
        d.is_gap = (d.confidence < 0.3f && d.interaction_count < 5);
    }
}
