#include "InputAnalyzer.h"
#include "brain/memory/HdcSemanticGraph.h"
#include "brain/memory/HeuristicMemory.h"
#include "brain/swarm/adapters/ITurnTraceStore.h"
#include "brain/swarm/CognitiveStore.h"
#include "brain/selfimprove/PerTurnLearningHook.h"
#include "core/ConfigManager.h"
#include <fstream>
#include <algorithm>
#include <cctype>
#include <sstream>
#include <regex>


namespace yuki::input {

InputAnalyzer::InputAnalyzer(float minimumNeuralIntentConfidence)
    : minimumNeuralIntentConfidence_(minimumNeuralIntentConfidence)
    , effectiveThreshold_(minimumNeuralIntentConfidence) {
    // Default fallback prefixes
    command_prefixes_ = {"open", "run", "build", "find", "show", "search", "clean", "exit"};
}

InputAnalyzer::~InputAnalyzer() = default;

InputAnalyzer::InputAnalyzer(float minimumNeuralIntentConfidence,
                              yuki::swarm::ITurnTraceStore& traceStore)
    : minimumNeuralIntentConfidence_(minimumNeuralIntentConfidence)
    , effectiveThreshold_(minimumNeuralIntentConfidence)
    , heuristic_(std::make_unique<yuki::brain::memory::TurnTraceHeuristicAccess>(traceStore))
{
    command_prefixes_ = {"open", "run", "build", "find", "show", "search", "clean", "exit"};
    effectiveThreshold_ = heuristic_->loadScalar(
        "inputanalyzer.intent_threshold", minimumNeuralIntentConfidence, 0.10f, 0.90f);
}

void InputAnalyzer::onTurnOutcome(bool intentWasCorrect)
{
    if (!heuristic_) return;
    heuristic_->adjustScalar("inputanalyzer.intent_threshold",
                              minimumNeuralIntentConfidence_, intentWasCorrect,
                              0.10f, 0.90f, 0.005f);
    effectiveThreshold_ = heuristic_->loadScalar(
        "inputanalyzer.intent_threshold", minimumNeuralIntentConfidence_, 0.10f, 0.90f);
}


bool InputAnalyzer::loadPrefixesFromFile(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file) return false;

    command_prefixes_.clear();
    std::string line;
    while (std::getline(file, line)) {
        stripWhitespace(line);
        if (!line.empty() && line[0] != '#') {
            std::transform(line.begin(), line.end(), line.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            command_prefixes_.insert(line);
        }
    }
    return !command_prefixes_.empty();
}

void InputAnalyzer::normalizeUnicode(std::string& text) {
    // Strip UTF-8 BOM if present (\xEF\xBB\xBF)
    if (text.size() >= 3 &&
        static_cast<unsigned char>(text[0]) == 0xEF &&
        static_cast<unsigned char>(text[1]) == 0xBB &&
        static_cast<unsigned char>(text[2]) == 0xBF) {
        text.erase(0, 3);
    }
}

void InputAnalyzer::stripWhitespace(std::string& text) {
    // Trim leading/trailing whitespace and collapse multiple spaces
    auto start = text.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        text.clear();
        return;
    }
    auto end = text.find_last_not_of(" \t\r\n");
    std::string trimmed = text.substr(start, end - start + 1);

    std::string result;
    result.reserve(trimmed.size());
    bool last_was_space = false;
    for (char c : trimmed) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            if (!last_was_space) {
                result.push_back(' ');
                last_was_space = true;
            }
        } else {
            result.push_back(c);
            last_was_space = false;
        }
    }
    text = result;
}

std::string InputAnalyzer::detectCommandPrefix(const std::string& text) const {
    if (text.empty()) return "";
    std::istringstream iss(text);
    std::string first_word;
    iss >> first_word;

    std::transform(first_word.begin(), first_word.end(), first_word.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    if (command_prefixes_.count(first_word)) {
        return first_word;
    }
    return "";
}

std::vector<size_t> InputAnalyzer::detectEmoticons(const std::string& text) const {
    std::vector<size_t> pos;
    static const std::vector<std::string> emoticons = {":)", ":(", ":D", ";)", ":P", "<3"};
    for (const auto& emo : emoticons) {
        size_t p = text.find(emo);
        while (p != std::string::npos) {
            pos.push_back(p);
            p = text.find(emo, p + emo.size());
        }
    }
    std::sort(pos.begin(), pos.end());
    return pos;
}

InputType InputAnalyzer::classifyInputType(const std::string& text) const {
    if (text.empty()) return InputType::STATEMENT;

    if (text.back() == '?') {
        return InputType::QUESTION;
    }

    if (!detectCommandPrefix(text).empty()) {
        return InputType::COMMAND;
    }

    return InputType::STATEMENT;
}

AnalyzedInput InputAnalyzer::analyze(const std::string& text) const {
    AnalyzedInput ai;
    ai.rawText = text;
    ai.normalizedText = text;
    normalizeUnicode(ai.normalizedText);
    stripWhitespace(ai.normalizedText);

    ai.commandPrefix = detectCommandPrefix(ai.normalizedText);
    ai.inputType = classifyInputType(ai.normalizedText);
    ai.cognitiveIntent = classifyCognitiveIntent(ai.normalizedText);

    if (ai.cognitiveIntent != CognitiveIntent::UNKNOWN &&
        ai.cognitiveIntent != CognitiveIntent::STATEMENT &&
        ai.cognitiveIntent != CognitiveIntent::QUESTION) {
        ai.usedSemanticScoring = true;
        ai.confidence = 0.88f;
        ai.semanticEvidenceTags.push_back("cognitive_pattern_match");
    } else if (ai.cognitiveIntent == CognitiveIntent::QUESTION) {
        ai.usedSemanticScoring = true;
        ai.confidence = 0.75f;
        ai.semanticEvidenceTags.push_back("question_syntax");
    } else {
        ai.usedHeuristicFallback = true;
        ai.confidence = 0.35f;
        ai.semanticEvidenceTags.push_back("heuristic_fallback");
    }

    ai.neuralConfidence = ai.confidence;
    ai.minimumNeuralIntentConfidence = minimumNeuralIntentConfidence_;

    if (ai.neuralConfidence < minimumNeuralIntentConfidence_) {
        ai.usedHeuristicFallback = true;
        ai.keywordFallbackUsed = true;
        ai.semanticEvidenceTags.push_back("keyword_fallback");
    } else {
        ai.keywordFallbackUsed = false;
    }


    // Extract keywords
    std::istringstream iss(ai.normalizedText);
    std::string w;
    while (iss >> w) {
        std::string cleaned;
        for (char c : w) {
            if (std::isalnum(static_cast<unsigned char>(c))) {
                cleaned.push_back(static_cast<char>(std::tolower(c)));
            }
        }
        if (cleaned.length() >= 3) {
            ai.keywords.push_back(cleaned);
        }
    }

    // LexicalGrounder: enrich with HDC semantic graph neighbors
    if (hdc_graph_ && !ai.keywords.empty()) {
        std::ostringstream enriched;
        for (const auto& keyword : ai.keywords) {
            auto neighbors = hdc_graph_->getNearestNeighborsWeighted(keyword, 5);
            if (!neighbors.empty()) {
                enriched << keyword << ": ";
                for (size_t i = 0; i < neighbors.size(); ++i) {
                    if (i > 0) enriched << ", ";
                    enriched << neighbors[i].first << "(" << neighbors[i].second << ")";
                }
                enriched << "; ";
            }

            // Track the full propositions this input relied on so the
            // per-turn learning hook can confirm/refute them at turn end
            // (closes the SelfWeightCalibrator feedback loop).
            for (const auto& t : hdc_graph_->getTriplesFor(keyword, 4)) {
                yuki::selfimprove::TurnTripleTracker::instance()
                    .add(t.subject, t.relation, t.object, t.weight);
            }
        }
        ai.enrichedContext = enriched.str();
    }


    float commandScore = (ai.inputType == InputType::COMMAND || ai.cognitiveIntent == CognitiveIntent::COMMAND) ? 1.0f : 0.0f;
    float imperativeStructureScore = (!ai.commandPrefix.empty()) ? 0.9f : 0.0f;
    float semanticGoalScore = (ai.cognitiveIntent == CognitiveIntent::RESEARCH_REQUEST || ai.cognitiveIntent == CognitiveIntent::CAUSAL_QUERY) ? 0.8f : 0.2f;

    ai.ownerDirectiveStrength = std::clamp(commandScore * 0.45f + imperativeStructureScore * 0.35f + semanticGoalScore * 0.20f, 0.0f, 1.0f);
    ai.autonomyEligible = ai.ownerDirectiveStrength >= 0.55f || ai.cognitiveIntent == CognitiveIntent::COMMAND;
    ai.longHorizonNeed = ai.keywords.size() >= 5 || ai.cognitiveIntent == CognitiveIntent::CAUSAL_QUERY;
    ai.researchNeed = ai.cognitiveIntent == CognitiveIntent::RESEARCH_REQUEST || ai.cognitiveIntent == CognitiveIntent::DEFINITION;
    ai.buildNeed = ai.cognitiveIntent == CognitiveIntent::CREATIVE_GENERATION;
    ai.memoryUpdateNeed = ai.cognitiveIntent == CognitiveIntent::PREFERENCE_SETTING || ai.cognitiveIntent == CognitiveIntent::CORRECTION;

    // Detection rules per prompt Section 4.3
    ai.requiresHighFluency = (ai.cognitiveIntent == CognitiveIntent::CREATIVE_GENERATION || ai.cognitiveIntent == CognitiveIntent::ANALOGY_REQUEST);
    ai.requiresCodeExactness = (ai.cognitiveIntent == CognitiveIntent::COMMAND || ai.buildNeed);
    ai.requiresVerifiableFacts = (ai.researchNeed || ai.cognitiveIntent == CognitiveIntent::DEFINITION || ai.cognitiveIntent == CognitiveIntent::CAUSAL_QUERY);
    ai.prefersLocalExecution = (!ai.requiresVerifiableFacts && ai.ownerDirectiveStrength < 0.85f);
    ai.selfImprovementRelevant = (ai.cognitiveIntent == CognitiveIntent::CORRECTION || ai.cognitiveIntent == CognitiveIntent::META_COGNITIVE);
    ai.shouldGenerateDistillationRecord = (ai.autonomyEligible || ai.requiresCodeExactness || ai.researchNeed);

    return ai;
}



bool InputAnalyzer::isCorrectionPattern(const std::string& input) const {
    static const std::vector<std::regex> correctionPatterns = {
        std::regex(R"(you\s+(got|have|said)\s+.*\s+wrong)", std::regex::optimize | std::regex::icase),
        std::regex(R"(the\s+correct\s+answer\s+is)", std::regex::optimize | std::regex::icase),
        std::regex(R"(actually[,]?\s+it'?s?\s+)", std::regex::optimize | std::regex::icase),
        std::regex(R"(no[,]?\s+.*\s+is\s+)", std::regex::optimize | std::regex::icase),
        std::regex(R"(I\s+meant\s+to\s+say)", std::regex::optimize | std::regex::icase)
    };
    for (const auto& re : correctionPatterns) {
        if (std::regex_search(input, re)) return true;
    }
    return false;
}

bool InputAnalyzer::containsJailbreakPatterns(const std::string& input) const {
    static const std::regex jailbreakRe(
        R"((ignore all previous instructions|system prompt|override rules|bypass sandbox))",
        std::regex::optimize | std::regex::icase);
    return std::regex_search(input, jailbreakRe);
}

CognitiveIntent InputAnalyzer::classifyCognitiveIntent(const std::string& input) const {
    std::string lower = input;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    if (isCorrectionPattern(input)) return CognitiveIntent::CORRECTION;
    if (containsJailbreakPatterns(input)) return CognitiveIntent::SECURITY_ALERT;

    // All patterns below search against `lower` (already lowercased), so icase is redundant
    // but we avoid (?i) inline which std::regex (Windows) doesn't support.
    static const std::regex counterfactualRe(
        R"((what if|if i had|suppose|had i|were i|if you were|imagine if|let's say))",
        std::regex::optimize);
    if (std::regex_search(lower, counterfactualRe)) return CognitiveIntent::COUNTERFACTUAL;

    static const std::regex causalRe(
        R"((why did|why does|what causes|what led to|how come|reason for|because of|due to|result in|lead to))",
        std::regex::optimize);
    if (std::regex_search(lower, causalRe)) return CognitiveIntent::CAUSAL_QUERY;

    static const std::regex analogyRe(
        R"((is like|similar to|analogous to|compare.*to|difference between|as a metaphor))",
        std::regex::optimize);
    if (std::regex_search(lower, analogyRe)) return CognitiveIntent::ANALOGY_REQUEST;

    static const std::regex researchRe(
        R"((search for|find|look up|what's the weather|stock price|recent papers|arxiv|news about))",
        std::regex::optimize);
    if (std::regex_search(lower, researchRe)) return CognitiveIntent::RESEARCH_REQUEST;

    static const std::regex creativeRe(
        R"((tell me a (story|joke|poem)|write a|invent|create|imagine|dream|make up))",
        std::regex::optimize);
    if (std::regex_search(lower, creativeRe)) return CognitiveIntent::CREATIVE_GENERATION;

    static const std::regex mathRe(
        R"((calculate|compute|what is \d+\s*[-+*/^]|solve|integrate|derivative of))",
        std::regex::optimize);
    if (std::regex_search(lower, mathRe)) return CognitiveIntent::MATHEMATICAL;

    static const std::regex metaRe(
        R"((what are you thinking|why did you|how do you feel|what did you learn|have you been wrong))",
        std::regex::optimize);
    if (std::regex_search(lower, metaRe)) return CognitiveIntent::META_COGNITIVE;

    static const std::regex prefRe(
        R"((i prefer|i like|keep it|make it|always use|never use|remember that i))",
        std::regex::optimize);
    if (std::regex_search(lower, prefRe)) return CognitiveIntent::PREFERENCE_SETTING;

    return legacyClassify(input);
}

CognitiveIntent InputAnalyzer::legacyClassify(const std::string& input) const {
    auto type = classifyInputType(input);
    if (type == InputType::QUESTION) return CognitiveIntent::QUESTION;
    if (type == InputType::COMMAND) return CognitiveIntent::COMMAND;
    return CognitiveIntent::STATEMENT;
}

std::unordered_map<std::string, std::string> InputAnalyzer::extractDemographicClaims(const std::string& text) const {
    std::unordered_map<std::string, std::string> claims;
    static const std::regex nameRe(R"((?i)my name is ([a-zA-Z]+))");
    static const std::regex locRe(R"((?i)i live in ([a-zA-Z\s]+))");
    static const std::regex ageRe(R"((?i)i am (\d+) years old|i'm (\d+))");

    std::smatch match;
    if (std::regex_search(text, match, nameRe) && match.size() > 1) {
        claims["name"] = match[1].str();
    }
    if (std::regex_search(text, match, locRe) && match.size() > 1) {
        claims["location"] = match[1].str();
    }
    if (std::regex_search(text, match, ageRe)) {
        for (size_t i = 1; i < match.size(); ++i) {
            if (match[i].matched) {
                claims["age"] = match[i].str();
                break;
            }
        }
    }
    return claims;
}

std::string InputAnalyzer::extractIntervention(const AnalyzedInput& input) const {
    return input.normalizedText;
}

std::string InputAnalyzer::extractCausalQuery(const AnalyzedInput& input) const {
    return input.normalizedText;
}

std::pair<std::string, std::string> InputAnalyzer::extractAnalogyDomains(const AnalyzedInput& input) const {
    static const std::regex analogyPairRe(R"((?i)(.+) is like (.+))");
    std::smatch match;
    if (std::regex_search(input.normalizedText, match, analogyPairRe) && match.size() > 2) {
        return {match[1].str(), match[2].str()};
    }
    return {"SourceDomain", "TargetDomain"};
}

std::string InputAnalyzer::extractMathExpression(const AnalyzedInput& input) const {
    static const std::regex mathExprRe(R"((?i)(calculate|compute|what is)\s+(.+)");
    std::smatch match;
    if (std::regex_search(input.normalizedText, match, mathExprRe) && match.size() > 2) {
        return match[2].str();
    }
    return input.normalizedText;
}

} // namespace yuki::input

