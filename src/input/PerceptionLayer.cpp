#include "input/PerceptionLayer.h"
#include "input/InputLayer.h"
#include <windows.h>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <algorithm>

std::string toString(PerceptionSourceType type) {
    switch (type) {
        case PerceptionSourceType::TEXT:     return "TEXT";
        case PerceptionSourceType::VOICE:    return "VOICE";
        case PerceptionSourceType::CAMERA:   return "CAMERA";
        case PerceptionSourceType::SCREEN:   return "SCREEN";
        case PerceptionSourceType::INTERNAL: return "INTERNAL";
    }
    return "UNKNOWN";
}

std::string toString(InputSourceKind sourceKind) {
    switch (sourceKind) {
        case InputSourceKind::TYPED:           return "TYPED";
        case InputSourceKind::VOICE_FINAL:     return "VOICE_FINAL";
        case InputSourceKind::VOICE_PARTIAL:   return "VOICE_PARTIAL";
        case InputSourceKind::CAMERA:          return "CAMERA";
        case InputSourceKind::SCREEN:          return "SCREEN";
        case InputSourceKind::SYSTEM_INTERNAL: return "SYSTEM_INTERNAL";
    }
    return "UNKNOWN";
}

std::string toString(PerceptionEventCategory category) {
    switch (category) {
        case PerceptionEventCategory::COMMAND:             return "COMMAND";
        case PerceptionEventCategory::USER_CHAT:           return "USER_CHAT";
        case PerceptionEventCategory::SENSORY_OBSERVATION: return "SENSORY_OBSERVATION";
        case PerceptionEventCategory::SYSTEM_EVENT:        return "SYSTEM_EVENT";
        case PerceptionEventCategory::STATUS_UPDATE:       return "STATUS_UPDATE";
        case PerceptionEventCategory::USER_TEXT:           return "USER_TEXT";
        case PerceptionEventCategory::USER_VOICE:          return "USER_VOICE";
        case PerceptionEventCategory::BODY_STATE:          return "BODY_STATE";
        case PerceptionEventCategory::SCREEN_CONTEXT:      return "SCREEN_CONTEXT";
        case PerceptionEventCategory::CAMERA_CONTEXT:      return "CAMERA_CONTEXT";
        case PerceptionEventCategory::EAR_STATUS:          return "EAR_STATUS";
        case PerceptionEventCategory::MOUTH_STATUS:        return "MOUTH_STATUS";
        case PerceptionEventCategory::COMMAND_RESULT:      return "COMMAND_RESULT";
    }
    return "UNKNOWN";
}

bool parseSourceType(const std::string& str, PerceptionSourceType& typeOut) {
    std::string upper = str;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
    if (upper == "TEXT") { typeOut = PerceptionSourceType::TEXT; return true; }
    if (upper == "VOICE") { typeOut = PerceptionSourceType::VOICE; return true; }
    if (upper == "CAMERA") { typeOut = PerceptionSourceType::CAMERA; return true; }
    if (upper == "SCREEN") { typeOut = PerceptionSourceType::SCREEN; return true; }
    if (upper == "INTERNAL") { typeOut = PerceptionSourceType::INTERNAL; return true; }
    return false;
}

bool parseEventCategory(const std::string& str, PerceptionEventCategory& catOut) {
    std::string upper = str;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
    if (upper == "COMMAND") { catOut = PerceptionEventCategory::COMMAND; return true; }
    if (upper == "USER_CHAT") { catOut = PerceptionEventCategory::USER_CHAT; return true; }
    if (upper == "SENSORY_OBSERVATION") { catOut = PerceptionEventCategory::SENSORY_OBSERVATION; return true; }
    if (upper == "SYSTEM_EVENT") { catOut = PerceptionEventCategory::SYSTEM_EVENT; return true; }
    if (upper == "STATUS_UPDATE") { catOut = PerceptionEventCategory::STATUS_UPDATE; return true; }
    return false;
}

UnifiedPerceptionLayer& UnifiedPerceptionLayer::instance() {
    static UnifiedPerceptionLayer inst;
    return inst;
}

void UnifiedPerceptionLayer::submitEvent(const PerceptionEvent& event) {
    PerceptionEvent cleaned = event;
    cleaned.confidence = clampConfidence(cleaned.confidence);
    if (cleaned.id.empty()) {
        cleaned.id = generateUniqueId();
    }

    std::vector<EventListener> targets;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        history_.push_back(cleaned);
        while (history_.size() > historyLimit_) {
            history_.erase(history_.begin());
        }

        for (const auto& pair : listeners_) {
            targets.push_back(pair.second);
        }
    }
    
    // Dispatch listeners safely outside the mutex lock to prevent deadlock cascades
    for (const auto& listener : targets) {
        if (listener) {
            listener(cleaned);
        }
    }
}

std::vector<PerceptionEvent> UnifiedPerceptionLayer::getEventHistory() {
    std::lock_guard<std::mutex> lock(mutex_);
    return history_;
}

void UnifiedPerceptionLayer::clearHistory() {
    std::lock_guard<std::mutex> lock(mutex_);
    history_.clear();
}

size_t UnifiedPerceptionLayer::getHistorySize() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return history_.size();
}

void UnifiedPerceptionLayer::setHistoryLimit(size_t limit) {
    std::lock_guard<std::mutex> lock(mutex_);
    historyLimit_ = limit;
    while (history_.size() > historyLimit_) {
        history_.erase(history_.begin());
    }
}

size_t UnifiedPerceptionLayer::getHistoryLimit() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return historyLimit_;
}

void UnifiedPerceptionLayer::registerListener(const std::string& name, EventListener listener) {
    std::lock_guard<std::mutex> lock(mutex_);
    listeners_[name] = listener;
}

void UnifiedPerceptionLayer::unregisterListener(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    listeners_.erase(name);
}

bool UnifiedPerceptionLayer::hasListener(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return listeners_.find(name) != listeners_.end();
}

size_t UnifiedPerceptionLayer::listenerCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return listeners_.size();
}

std::string UnifiedPerceptionLayer::generateUniqueId() {
    unsigned long long count;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        count = ++eventCounter_;
    }
    return "evt_" + std::to_string(GetTickCount64()) + "_" + std::to_string(count);
}

// -------------------------------------------------
// Private Dry Helper Builders
// -------------------------------------------------

PerceptionEvent UnifiedPerceptionLayer::makeBaseEvent(InputSourceKind sourceKind, PerceptionSourceType source, PerceptionEventCategory category, double confidence, const std::string& snapshot) {
    PerceptionEvent e;
    e.id = instance().generateUniqueId();
    e.sourceKind = sourceKind;
    e.source = source;
    e.category = category;
    e.timestamp = GetTickCount64();
    e.created_at = GetTickCount64();
    e.confidence = clampConfidence(confidence);
    e.subsystemSnapshotSummary = snapshot;
    e.subsystem_snapshot = snapshot;
    return e;
}

std::vector<std::string> UnifiedPerceptionLayer::tokenizeWhitespace(const std::string& text) {
    std::vector<std::string> tokens;
    std::stringstream ss(text);
    std::string token;
    while (ss >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

double UnifiedPerceptionLayer::clampConfidence(double confidence) {
    if (confidence < 0.0) return 0.0;
    if (confidence > 1.0) return 1.0;
    return confidence;
}

void UnifiedPerceptionLayer::appendTagIfMissing(std::vector<std::string>& tags, const std::string& tag) {
    if (std::find(tags.begin(), tags.end(), tag) == tags.end()) {
        tags.push_back(tag);
    }
}

// -------------------------------------------------
// Unified Factory Adapters
// -------------------------------------------------

PerceptionEvent UnifiedPerceptionLayer::fromText(const std::string& text, bool isCommand, const std::string& snapshot) {
    PerceptionEventCategory cat = isCommand ? PerceptionEventCategory::COMMAND : PerceptionEventCategory::USER_CHAT;
    PerceptionEvent e = makeBaseEvent(InputSourceKind::TYPED, PerceptionSourceType::TEXT, cat, 1.0, snapshot);
    e.rawContent = text;
    e.raw_content = text;

    InputPerceptionBuilder builder;
    InputPerception ip = builder.analyze(text);
    e.normalizedContent = ip.normalized_text;
    e.normalized_content = ip.normalized_text;
    e.tokens = ip.chunks;
    appendTagIfMissing(e.tags, isCommand ? "command" : "conversation");
    return e;
}

PerceptionEvent UnifiedPerceptionLayer::fromVoiceText(const std::string& transcript, bool isCommand, double confidence, const std::string& snapshot) {
    PerceptionEventCategory cat = isCommand ? PerceptionEventCategory::COMMAND : PerceptionEventCategory::USER_CHAT;
    PerceptionEvent e = makeBaseEvent(InputSourceKind::VOICE_FINAL, PerceptionSourceType::VOICE, cat, confidence, snapshot);
    e.rawContent = transcript;
    e.raw_content = transcript;

    InputPerceptionBuilder builder;
    InputPerception ip = builder.analyze(transcript);
    e.normalizedContent = ip.normalized_text;
    e.normalized_content = ip.normalized_text;
    e.tokens = ip.chunks;
    appendTagIfMissing(e.tags, "voice_text");
    appendTagIfMissing(e.tags, isCommand ? "command" : "conversation");
    return e;
}

PerceptionEvent UnifiedPerceptionLayer::fromVoiceObservation(const std::string& audioSummary, double confidence, const std::string& snapshot) {
    PerceptionEvent e = makeBaseEvent(InputSourceKind::VOICE_PARTIAL, PerceptionSourceType::VOICE, PerceptionEventCategory::SENSORY_OBSERVATION, confidence, snapshot);
    e.rawContent = audioSummary;
    e.raw_content = audioSummary;

    InputPerceptionBuilder builder;
    InputPerception ip = builder.analyze(audioSummary);
    e.normalizedContent = ip.normalized_text;
    e.normalized_content = ip.normalized_text;
    e.tokens = ip.chunks;
    appendTagIfMissing(e.tags, "audio_stream");
    appendTagIfMissing(e.tags, "observation");
    return e;
}

PerceptionEvent UnifiedPerceptionLayer::fromVoice(const std::string& audioSummary, double confidence, const std::string& snapshot) {
    std::string lower = audioSummary;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    bool isObs = (lower.find("spike") != std::string::npos ||
                  lower.find("boundary") != std::string::npos ||
                  lower.find("captured") != std::string::npos ||
                  lower.find("rms") != std::string::npos ||
                  lower.find("silence") != std::string::npos ||
                  lower.find("volume") != std::string::npos);

    if (isObs) {
        return fromVoiceObservation(audioSummary, confidence, snapshot);
    } else {
        InputPerceptionBuilder builder;
        InputPerception ip = builder.analyze(audioSummary);
        bool isCmd = ip.has_action_cue || 
                     (lower.find("mic ") == 0 || 
                      lower.find("speaker ") == 0 || 
                      lower.find("camera ") == 0 || 
                      lower.find("screen ") == 0 || 
                      lower.find("quit") != std::string::npos);
        return fromVoiceText(audioSummary, isCmd, confidence, snapshot);
    }
}

PerceptionEvent UnifiedPerceptionLayer::fromCamera(const std::string& summary, const std::map<std::string, std::string>& meta, const std::string& snapshot) {
    PerceptionEvent e = makeBaseEvent(InputSourceKind::CAMERA, PerceptionSourceType::CAMERA, PerceptionEventCategory::SENSORY_OBSERVATION, 1.0, snapshot);
    e.rawContent = summary;
    e.raw_content = summary;
    e.normalizedContent = summary;
    e.normalized_content = summary;
    e.tokens = tokenizeWhitespace(summary);
    appendTagIfMissing(e.tags, "vision_frame");
    e.metadata = meta;
    return e;
}

PerceptionEvent UnifiedPerceptionLayer::fromScreen(const std::string& summary, const std::map<std::string, std::string>& meta, const std::string& snapshot) {
    PerceptionEvent e = makeBaseEvent(InputSourceKind::SCREEN, PerceptionSourceType::SCREEN, PerceptionEventCategory::SENSORY_OBSERVATION, 1.0, snapshot);
    e.rawContent = summary;
    e.raw_content = summary;
    e.normalizedContent = summary;
    e.normalized_content = summary;
    e.tokens = tokenizeWhitespace(summary);
    appendTagIfMissing(e.tags, "desktop_frame");
    e.metadata = meta;
    return e;
}

PerceptionEvent UnifiedPerceptionLayer::fromInternal(const std::string& eventName, const std::string& details, const std::string& snapshot) {
    PerceptionEvent e = makeBaseEvent(InputSourceKind::SYSTEM_INTERNAL, PerceptionSourceType::INTERNAL, PerceptionEventCategory::SYSTEM_EVENT, 1.0, snapshot);
    e.rawContent = eventName + ": " + details;
    e.raw_content = eventName + ": " + details;
    e.normalizedContent = eventName;
    e.normalized_content = eventName;
    e.tokens = tokenizeWhitespace(eventName);
    appendTagIfMissing(e.tags, "system_log");
    e.metadata["details"] = details;
    return e;
}
