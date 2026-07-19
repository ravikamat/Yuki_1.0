#include "IntentClassifier.h"
#include <algorithm>
#include <cctype>

IntentClassifier& IntentClassifier::instance() {
    static IntentClassifier inst;
    return inst;
}

GroundedIntentResult IntentClassifier::classify(const std::string& normalizedInput) {
    GroundedIntentResult res;
    
    std::string text = normalizedInput;
    auto start = std::find_if_not(text.begin(), text.end(), [](unsigned char c) { return std::isspace(c); });
    auto end = std::find_if_not(text.rbegin(), text.rend(), [](unsigned char c) { return std::isspace(c); }).base();
    if (start < end) {
        text = std::string(start, end);
    } else {
        text = "";
    }

    if (text.empty()) { 
        res.intent = GroundedIntent::EMPTY_NOISE; 
        res.confidenceStr = "HIGH"; 
        res.routing = IntentRoutingDecision::LEGACY_PATH;
        return res; 
    }

    std::string lowerText = text;
    for (char& c : lowerText) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    if (lowerText == "hi" || lowerText == "hello" || lowerText == "hey" || lowerText == "namaste" || lowerText == "helo") {
        res.intent = GroundedIntent::GREETING; 
        res.confidenceStr = "HIGH"; 
        res.routing = IntentRoutingDecision::LEGACY_PATH;
        return res; 
    }

    if (lowerText.find("who are you") != std::string::npos || lowerText.find("who r u") != std::string::npos || lowerText.find("aap kaun") != std::string::npos || lowerText.find("tum kaun") != std::string::npos) {
        res.intent = GroundedIntent::IDENTITY_QUERY; 
        res.confidenceStr = "HIGH"; 
        res.routing = IntentRoutingDecision::LEGACY_PATH;
        return res; 
    }

    if (lowerText.find("what is") != std::string::npos || lowerText.find("how to") != std::string::npos || lowerText.find("why did") != std::string::npos || text.back() == '?') {
        res.intent = GroundedIntent::QUESTION; 
        res.confidenceStr = "MEDIUM"; 
        res.routing = IntentRoutingDecision::LEGACY_PATH;
        return res; 
    }

    if (lowerText.find("run") == 0 || lowerText.find("stop") == 0 || lowerText.find("system") == 0 || lowerText.find("turn on") == 0) {
        res.intent = GroundedIntent::COMMAND; 
        res.confidenceStr = "HIGH"; 
        res.routing = IntentRoutingDecision::DIRECT_COMMAND;
        return res; 
    }

    if (lowerText.find("can you") != std::string::npos || lowerText.find("please") != std::string::npos || lowerText.find("build") != std::string::npos) {
        res.intent = GroundedIntent::REQUEST; 
        res.confidenceStr = "MEDIUM"; 
        res.routing = IntentRoutingDecision::CLARIFICATION;
        return res; 
    }

    res.intent = GroundedIntent::UNKNOWN; 
    res.confidenceStr = "LOW";
    res.routing = IntentRoutingDecision::LEGACY_PATH;
    return res;
}
