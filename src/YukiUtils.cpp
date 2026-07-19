// YukiUtils.cpp — Feature flags + checkpoint tracing (merged)
#include "YukiUtils.h"
#include <sstream>
#include <iomanip>

// ── FeatureFlags ──────────────────────────────────────────────────────────────
static FeatureFlags g_flags;
FeatureFlags& flags() { return g_flags; }
void loadFeatureFlags() {
    g_flags.baby_mode_only       = true;
    g_flags.show_terminal_trace  = true;
    g_flags.show_checkpoint_hits = true;
}

// ── CheckpointTracer ──────────────────────────────────────────────────────────
void CheckpointTracer::hit(const std::string& stage, const std::string& detail) {
    events_.push_back({ stage, detail });
}
void CheckpointTracer::clear() { events_.clear(); }
const std::vector<CheckpointEvent>& CheckpointTracer::events() const { return events_; }

std::string CheckpointTracer::renderFull() const {
    if (events_.empty()) return "[TRACE] (no checkpoints recorded)\n";
    std::ostringstream out;
    out << "\n+------------------------------------------+\n";
    out << "|         CHECKPOINT TRACE (FULL)          |\n";
    out << "+------------------------------------------+\n";
    for (std::size_t i = 0; i < events_.size(); ++i) {
        const auto& ev = events_[i];
        out << "| [" << std::setw(2) << std::right << (i+1) << "]  ";
        out << std::left << std::setw(26) << ev.stage;
        if (!ev.detail.empty()) out << "-> " << ev.detail;
        out << "\n";
    }
    out << "+------------------------------------------+\n";
    return out.str();
}

std::string CheckpointTracer::renderSmart() const {
    if (events_.empty()) return "[TRACE] (empty)\n";
    std::ostringstream out;
    out << "[TRACE] ";
    for (std::size_t i = 0; i < events_.size(); ++i) {
        if (i!=0) out << " -> ";
        out << events_[i].stage;
    }
    out << "\n";
    return out.str();
}
