#pragma once
#include <string>

enum class GroundedIntent {
    QUESTION,
    COMMAND,
    REQUEST,
    IDENTITY_QUERY,
    GREETING,
    EMPTY_NOISE,
    UNKNOWN
};

enum class IntentRoutingDecision {
    LEGACY_PATH,
    DIRECT_COMMAND,
    CLARIFICATION
};

struct GroundedIntentResult {
    GroundedIntent intent = GroundedIntent::UNKNOWN;
    std::string confidenceStr = "LOW";
    IntentRoutingDecision routing = IntentRoutingDecision::LEGACY_PATH;
};

class IntentClassifier {
public:
    static IntentClassifier& instance();
    GroundedIntentResult classify(const std::string& normalizedInput);

private:
    IntentClassifier() = default;
};
