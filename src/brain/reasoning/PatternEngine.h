#pragma once
// PatternEngine.h
// Yuki_1.0 — § 3.2 Pattern Layer
//
// Converts a CanonicalInputEvent into a fully-populated PatternFrame.
//
// Responsibilities (per spec):
//   1. Detect intent (RequestMode) via multi-signal scoring
//   2. Detect requested output type (OutputMode)
//   3. Extract named entities (proper nouns, quoted phrases, technical terms)
//   4. Infer explicit AND implicit constraints
//   5. Determine whether request depends on conversation history
//   6. Estimate whether freshness / live data is required
//   7. Identify unknown slots (what info is missing to answer well)
//   8. Score overall frame confidence

#include "BrainTypes.h"
#include "brain/reasoning/GoalModel.h"   // GoalModel — enriches PatternFrame with Phase 1 semantic data
#include <string>
#include <vector>

// Scoring signal for RequestMode election
struct ModeSignal {
    RequestMode mode;
    float       weight;   // how strongly this signal votes for the mode
    std::string evidence; // human-readable reason (for trace)
};

class PatternEngine {
public:
    PatternEngine();

    // Build a full PatternFrame from a canonical input event + GoalModel.
    // GoalModel carries language, tone, domain, emotional flag, and semantic slots
    // from LanguageLayer + SemanticParser — enriches PatternFrame so all downstream
    // pipeline steps see the full Phase 1 understanding.
    PatternFrame buildFrame(const CanonicalInputEvent& event, const GoalModel& spec) const;

    // Introspect: get the signals that led to the most recent RequestMode decision.
    const std::vector<ModeSignal>& lastSignals() const { return lastSignals_; }

private:
    // ── Step 1: RequestMode — multi-signal scoring ───────────────────────────
    RequestMode scoreRequestMode(const std::string& raw,
                                  const std::string& lower,
                                  std::vector<ModeSignal>& signals) const;

    // ── Step 2: OutputMode ───────────────────────────────────────────────────
    OutputMode  detectOutputMode(const std::string& lower) const;

    // ── Step 3: Entity extraction ────────────────────────────────────────────
    // Extracts: proper-noun phrases, quoted strings, technical terms, numbers
    std::vector<std::string> extractEntities(const std::string& raw,
                                               const std::string& lower) const;

    // ── Step 4: Constraint extraction ───────────────────────────────────────
    void extractConstraints(const std::string& raw,
                             const std::string& lower,
                             RequestMode mode,
                             std::vector<std::string>& explicit_,
                             std::vector<std::string>& implicit_) const;

    // ── Step 5: History dependence ───────────────────────────────────────────
    bool detectHistoryDependence(const std::string& lower) const;

    // ── Step 6: Freshness requirement ───────────────────────────────────────
    bool detectFreshnessRequired(const std::string& lower) const;

    // ── Step 8: Core intent (distilled phrase) ────────────────────────────────
    std::string extractCoreIntent(const std::string& raw,
                                   const std::string& lower,
                                   RequestMode mode) const;

    // ── Step 9: Confidence ───────────────────────────────────────────────────
    float scoreConfidence(const std::vector<ModeSignal>& signals,
                           RequestMode winner) const;

    // ── Helpers ───────────────────────────────────────────────────────────────
    static bool has(const std::string& hay, const std::string& needle);
    static bool hasWord(const std::string& hay, const std::string& word);
    static std::string stripLeadingQuestionWords(const std::string& lower);

    mutable std::vector<ModeSignal> lastSignals_;
    mutable int interactionCounter_ = 0;
};
