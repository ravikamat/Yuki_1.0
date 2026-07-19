#pragma once
#include <string>
#include <vector>
#include "ExecutionTypes.h"

class LanguageSynthesizer {
public:
    LanguageSynthesizer();

    // Primary render path: converts a validated UtterancePlan to final user-facing text.
    // Trims trailing whitespace. Returns "" for unknown act types.
    std::string render(const UtterancePlan& plan);

    // Legacy pass-through stub — kept for any old call sites; returns input unchanged.
    // Do not add new callers. Use render() instead.
    std::string synthesize(const std::string& actPlan);

    // generateResponse() and generateFactual() were dead inline stubs — removed.
    // They leaked GoalModel.h into callers unnecessarily. Use render() instead.
};
