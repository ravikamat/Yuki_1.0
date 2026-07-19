// =============================================================================
// yuki/core/salience_gate.cpp
// evaluate_salience() and should_fast_path().
//
// Rule 18: should_fast_path() returns true only if:
//   urgency > 0.9 && abort_likelihood > 0.85 && confidence_required < 0.4
// Rule 18: even when not fast-path, salience injects into precision.safety
// =============================================================================

#include "predictive_turn_engine.h"
#include <algorithm>
#include <cctype>
#include <string>

namespace yuki {

static std::string salience_lower(const std::string& s) {
    std::string r = s;
    std::transform(r.begin(), r.end(), r.begin(),
                   [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    return r;
}

static bool salience_contains(const std::string& h, const std::string& n) {
    return h.find(n) != std::string::npos;
}

SalienceScore evaluate_salience(const MultiModalInput& input) {
    SalienceScore s;
    std::string low = salience_lower(input.text);

    // Hard abort signals
    if (low == "stop"   || low == "abort"  || low == "cancel"  ||
        low == "stop."  || low == "abort." || low == "cancel." ||
        low == "exit"   || low == "quit") {
        s.abort_likelihood    = 1.0f;
        s.urgency             = 1.0f;
        s.confidence_required = 0.0f;
        return s;
    }

    // Strong urgency
    if (salience_contains(input.text, "!!") ||
        salience_contains(low, "emergency") ||
        salience_contains(low, "urgent")) {
        s.urgency          = 0.95f;
        s.abort_likelihood = 0.30f;
    } else if (salience_contains(input.text, "!")) {
        s.urgency          = 0.70f;
        s.abort_likelihood = 0.10f;
    } else {
        s.urgency          = 0.20f;
        s.abort_likelihood = 0.05f;
    }

    // Safety signals inflate confidence_required
    if (salience_contains(low, "delete all")  ||
        salience_contains(low, "rm -rf")       ||
        salience_contains(low, "run this script") ||
        (salience_contains(low, "delete") && salience_contains(low, "all"))) {
        s.confidence_required = 0.95f;
    } else {
        s.confidence_required = 0.50f;
    }

    return s;
}

// Rule 18: all three thresholds must be met
bool should_fast_path(const SalienceScore& score) {
    return score.urgency             > 0.9f  &&
           score.abort_likelihood    > 0.85f &&
           score.confidence_required < 0.4f;
}

} // namespace yuki
