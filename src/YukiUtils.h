#pragma once
// YukiUtils.h — Feature flags + checkpoint tracing (merged from FeatureFlags + CheckpointTracer)
#include <string>
#include <vector>

// ── §FeatureFlags ─────────────────────────────────────────────────────────────
struct FeatureFlags {
    bool baby_mode_only       = true;
    bool show_terminal_trace  = true;
    bool show_checkpoint_hits = true;
};
FeatureFlags& flags();
void loadFeatureFlags();

// ── §CheckpointTracer ─────────────────────────────────────────────────────────
enum class TraceMode { FULL, SMART, MINIMAL, HIDDEN };

struct CheckpointEvent {
    std::string stage;
    std::string detail;
};

class CheckpointTracer {
public:
    void hit(const std::string& stage, const std::string& detail = "");
    void clear();
    std::string renderFull() const;
    std::string renderSmart() const;
    const std::vector<CheckpointEvent>& events() const;
private:
    std::vector<CheckpointEvent> events_;
};
