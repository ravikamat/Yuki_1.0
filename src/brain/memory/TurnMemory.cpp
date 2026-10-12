#include "TurnMemory.h"
#include <algorithm>
#include <nlohmann/json.hpp>

// Lock aliases for yuki::concurrency::shared_mutex nested types.
using UniqueLock = yuki::concurrency::shared_mutex::unique_lock;
using SharedLock = yuki::concurrency::shared_mutex::shared_lock;

TurnMemory& TurnMemory::instance() {
    static TurnMemory inst;
    return inst;
}

void TurnMemory::append(const TurnRecord& record) {
    UniqueLock lock(mutex_);
    history_.push_back(record);
    if (history_.size() > kMaxHistory) {
        history_.pop_front();
    }
}

std::vector<TurnRecord> TurnMemory::last_n(size_t n) const {
    SharedLock lock(mutex_);
    std::vector<TurnRecord> out;
    size_t count = std::min(n, history_.size());
    auto it = history_.end() - count;
    for (; it != history_.end(); ++it) {
        out.push_back(*it);
    }
    return out;
}

void TurnMemory::update_feedback(const std::string& turn_id, FeedbackType feedback) {
    UniqueLock lock(mutex_);
    for (auto& rec : history_) {
        if (rec.turn_id == turn_id) {
            rec.feedback = feedback;
            return;
        }
    }
}

bool TurnMemory::was_corrected(const std::string& turn_id) const {
    SharedLock lock(mutex_);
    bool foundTarget = false;
    for (const auto& rec : history_) {
        if (rec.turn_id == turn_id) {
            foundTarget = true;
            continue;
        }
        if (foundTarget) {
            if (rec.feedback == FeedbackType::CORRECTION ||
                rec.feedback == FeedbackType::EXPLICIT_THUMBS_DOWN) {
                return true;
            }
        }
    }
    return false;
}

// ponytail: L1 hibernation — dump turn buffer to JSON string
std::string TurnMemory::serializeToJson() const {
    SharedLock lock(mutex_);
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& r : history_) {
        nlohmann::json j;
        j["turn_id"]       = r.turn_id;
        j["timestamp"]     = std::chrono::system_clock::to_time_t(r.timestamp);
        j["intent"]        = static_cast<int>(r.intent);
        j["confidence"]    = r.confidence;
        j["policy"]        = static_cast<int>(r.policy);
        j["tool_name"]     = r.tool_name;
        j["tool_status"]   = static_cast<int>(r.tool_status);
        j["exec_ms"]       = r.exec_ms;
        j["response_text"] = r.response_text;
        j["response_type"] = static_cast<int>(r.response_type);
        j["has_feedback"]  = r.feedback.has_value();
        if (r.feedback) j["feedback"] = static_cast<int>(*r.feedback);
        arr.push_back(std::move(j));
    }
    return arr.dump();
}

bool TurnMemory::loadFromJson(const std::string& json_str) {
    try {
        auto arr = nlohmann::json::parse(json_str);
        if (!arr.is_array()) return false;
        UniqueLock lock(mutex_);
        history_.clear();
        for (const auto& j : arr) {
            TurnRecord r;
            r.turn_id       = j.value("turn_id", "");
            r.timestamp     = std::chrono::system_clock::from_time_t(j.value("timestamp", 0));
            r.intent        = static_cast<IntentType>(j.value("intent", 0));
            r.confidence    = j.value("confidence", 0.0f);
            r.policy        = static_cast<Policy>(j.value("policy", 5));
            r.tool_name     = j.value("tool_name", "");
            r.tool_status   = static_cast<ExecutionStatus>(j.value("tool_status", 0));
            r.exec_ms       = j.value("exec_ms", 0);
            r.response_text = j.value("response_text", "");
            r.response_type = static_cast<ResponseType>(j.value("response_type", 0));
            if (j.value("has_feedback", false)) {
                r.feedback = static_cast<FeedbackType>(j.value("feedback", 0));
            }
            history_.push_back(std::move(r));
        }
        // ponytail: trim to max if saved file had more (shouldn't, but safe)
        while (history_.size() > kMaxHistory) history_.pop_front();
        return true;
    } catch (...) {
        return false;
    }
}
