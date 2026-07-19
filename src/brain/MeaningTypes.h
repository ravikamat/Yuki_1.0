#pragma once
// MeaningTypes.h
// Yuki_1.0 - Data structures for the 13-stage Meaning Pipeline

#include <string>
#include <vector>
#include <map>
#include <cstdint>

enum class ConfidenceBehavior {
    SILENT_PROCEED,
    PROCEED_WITH_DISCLOSURE,
    SINGLE_CLARIFY,
    CONTEXT_RESCUE,
    HONEST_UNKNOWN
};

enum class DetectedLanguage {
    ENGLISH,
    HINDI_DEVANAGARI,
    HINGLISH,
    UNKNOWN
};

struct LanguageResult {
    DetectedLanguage detected = DetectedLanguage::ENGLISH;
    std::string code;
    std::string detected_text;
    std::string translated_english;
    std::string normalizedEnglish;
    
    // Legacy fields for MotherCore compatibility
    std::string languageCode;
    std::string responseStyle;
    bool needsTranslation = false;
};

struct NormalizedInput {
    std::string raw_text;
    std::string clean_text;
    std::string canonical_text;
    std::string lang_code;
    
    // Legacy fields
    std::string input_source;
    int64_t timestamp_ms = 0;
};

struct VocabInspection {
    std::vector<std::string> known_words;
    std::vector<std::string> unknown_words;
};

struct UncertaintyReport {
    struct TokenFlag {
        int         token_index;
        std::string token;
        std::string flag_type;
        float       confidence;
    };
    std::vector<TokenFlag> token_flags;
    std::string sentence_flag;
    float overall_certainty = 1.0f;
};

struct Candidate {
    std::string original_token;
    std::string candidate_text;
    float editScore = 0.0f;
    float trigramScore = 0.0f;
    float phoneticScore = 0.0f;
    float patternScore = 0.0f;
    float contextScore = 0.0f;
    float finalScore = 0.0f;
    std::string generation_method;
};

struct CandidateSet {
    std::string original_token;
    std::vector<Candidate> candidates;
};

struct QueryHypothesis {
    std::string rewritten_query;
    float       hypothesis_score = 0.0f;
    std::string pattern_label;
    std::string substitution_notes;
};

struct RewriteSet {
    std::string original_query;
    std::vector<QueryHypothesis> hypotheses;
    std::string bestText;
};

enum class EntityType {
    PERSON, APP, FILE_TARGET, PLACE, TIME_REF, CONDITION,
    WEATHER_STATE, DIGITAL_OBJECT, SYSTEM_RESOURCE,
    UNKNOWN_ENTITY,
    OBJECT, CONCEPT, ACTION, AMBIGUOUS
};

struct EntitySpan {
    std::string text;
    int start_pos;
    int end_pos;
};

struct LinkedEntity {
    std::string   raw_span;
    std::string   canonical_form;
    EntityType    type = EntityType::UNKNOWN_ENTITY;
    float         link_confidence = 0.0f;
    std::string   link_source;
};

struct RankedCandidateSet {
    std::string bestText;
    float bestConfidence;
};

struct Goal {
    enum class RiskLevel { SAFE, COMMUNICATION, DESTRUCTIVE, SYSTEM_CHANGE, SELF_MODIFY };
    std::string target;
    std::string action;
    RiskLevel risk = RiskLevel::SAFE;
    std::map<std::string, std::string> parameters;
};

struct MeaningState {
    LanguageResult language;
    NormalizedInput normalized;
    std::vector<LinkedEntity> entities;
    RankedCandidateSet ranked;
    ConfidenceBehavior behavior;
    std::string requestType;
    Goal goal;
    
    // Legacy fields for MotherCore / Phase 1 compatibility
    std::string request_type;
    std::string best_hypothesis;
    bool has_multi_intent = false;
    bool needs_clarification = false;
    float overall_confidence = 0.0f;
    std::vector<std::string> unknown_spans_remaining;
    std::string goal_summary;
    std::map<std::string, std::string> known_slots;
    std::string raw_input;
    std::string normalized_input;
    std::vector<std::string> action_verbs;
    std::vector<std::string> objects;
    std::vector<std::string> conditions;
    std::string time_constraint;
    bool has_ambiguity = false;
    std::vector<std::string> ambiguous_spans;
    std::string lang_code;
    std::string input_source;
    int64_t pipeline_start_ms = 0;
};

struct FactBundle {
    std::string summary;
    std::vector<std::string> sources;
};
