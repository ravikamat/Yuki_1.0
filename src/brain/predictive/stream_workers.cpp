// =============================================================================
// yuki/core/stream_workers.cpp
// Implementations for E1FastStream, E2SemanticStream, E3DeepStream.
// =============================================================================

#include "stream_workers.h"
#include "predictive_turn_engine.h"
#include "../reasoning/PatternEngine.h"
#include "../reasoning/SemanticParser.h"
#include "../emotion/EmotionSystem.h"
#include "../database/DatabaseManager.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <string>
#include <thread>
#include <vector>
#include <cmath>

namespace yuki {

static IntentClass map_request_mode(RequestMode rm) {
    switch (rm) {
        case RequestMode::QUESTION:       return IntentClass::QUERY;
        case RequestMode::COMMAND:        return IntentClass::COMMAND;
        case RequestMode::RESEARCH:       return IntentClass::QUERY;
        case RequestMode::IMPLEMENTATION: return IntentClass::COMMAND;
        case RequestMode::DESIGN:         return IntentClass::COMMAND;
        case RequestMode::CLARIFICATION:   return IntentClass::CLARIFICATION_RESPONSE;
        default:                          return IntentClass::UNKNOWN;
    }
}

static IntentClass map_intent_category(IntentCategory ic) {
    switch (ic) {
        case IntentCategory::INFORMATION_QUERY: return IntentClass::QUERY;
        case IntentCategory::TASK_COMMAND:      return IntentClass::COMMAND;
        case IntentCategory::EMOTIONAL:         return IntentClass::EMOTIONAL_VENT;
        case IntentCategory::TEACH:             return IntentClass::TUTORIAL;
        case IntentCategory::CONTINUATION:      return IntentClass::CLARIFICATION_RESPONSE;
        case IntentCategory::NEGATIVE:          return IntentClass::ABORT;
        default:                                return IntentClass::UNKNOWN;
    }
}

namespace stream_detail {

std::string to_lower(const std::string& s) {
    std::string r = s;
    std::transform(r.begin(), r.end(), r.begin(),
                   [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    return r;
}

bool contains(const std::string& h, const std::string& n) {
    return h.find(n) != std::string::npos;
}

// Word-boundary scan: returns true only if needle appears as a whole word.
// Prevents "skills" from matching the "kill" safety veto.
bool contains_word(const std::string& haystack, const std::string& needle) {
    if (needle.empty() || haystack.empty()) return false;
    size_t pos = 0;
    while (true) {
        pos = haystack.find(needle, pos);
        if (pos == std::string::npos) return false;
        bool left_boundary = (pos == 0) ||
            !std::isalnum(static_cast<unsigned char>(haystack[pos - 1]));
        bool right_boundary = (pos + needle.length() >= haystack.length()) ||
            !std::isalnum(static_cast<unsigned char>(haystack[pos + needle.length()]));
        if (left_boundary && right_boundary) return true;
        ++pos;
    }
}


std::chrono::microseconds now_us() {
    using namespace std::chrono;
    return duration_cast<microseconds>(
        steady_clock::now().time_since_epoch()) % microseconds(1'000'000);
}

std::vector<float> uniform_intent() {
    size_t n = static_cast<size_t>(IntentClass::COUNT);
    return std::vector<float>(n, 1.0f / static_cast<float>(n));
}

std::vector<float> peaked_intent(IntentClass cls, float peak) {
    size_t n = static_cast<size_t>(IntentClass::COUNT);
    float  rest = (1.0f - peak) / static_cast<float>(n > 1 ? n - 1 : 1);
    std::vector<float> d(n, rest);
    d[static_cast<size_t>(cls)] = peak;
    return d;
}

} // namespace stream_detail

namespace {

void emit_obs(moodycamel::ConcurrentQueue<PartialObservation>& out,
              const std::string& sid, uint8_t prio,
              const std::string& dim,
              float observed, float predicted, float pred_error, float local_prec)
{
    PartialObservation o;
    o.stream_id        = sid;
    o.stream_priority  = prio;
    o.dimension        = dim;
    o.observed_value   = observed;
    o.predicted_value  = predicted;
    o.prediction_error = pred_error;
    o.local_precision  = local_prec;
    o.is_final         = false;
    o.timestamp        = stream_detail::now_us();
    out.enqueue(o);
}

void emit_final(moodycamel::ConcurrentQueue<PartialObservation>& out,
                const std::string& sid, uint8_t prio)
{
    PartialObservation s;
    s.stream_id       = sid;
    s.stream_priority = prio;
    s.is_final        = true;
    s.timestamp       = stream_detail::now_us();
    out.enqueue(s);
}

float kl_pe(const PredictionState& state, IntentClass cls, float conf) {
    auto prior_v = std::vector<float>(state.expected_intents.begin(),
                                      state.expected_intents.end());
    auto obs_v   = stream_detail::peaked_intent(cls, conf);
    float raw_kl = 0.0f;
    for (size_t k = 0; k < prior_v.size() && k < obs_v.size(); ++k) {
        float p = std::max(prior_v[k], constants::PROBABILITY_EPS);
        float q = std::max(obs_v[k],   constants::PROBABILITY_EPS);
        raw_kl += p * std::log(p / q);
    }
    raw_kl = std::min(std::max(raw_kl, 0.0f), constants::INTENT_KL_CAP);
    return 1.0f / (1.0f + std::exp(-2.0f * raw_kl + 3.0f));
}

} // anonymous namespace

// =============================================================================
// E1FastStream  — keyword scanner, ~10 µs
// =============================================================================

void E1FastStream::run(const MultiModalInput& input,
                       const PredictionState& state,
                       moodycamel::ConcurrentQueue<PartialObservation>& out)
{
    using namespace stream_detail;
    std::this_thread::sleep_for(std::chrono::microseconds(10));

    const std::string low  = to_lower(input.text);
    const std::string& sid = stream_id();
    const uint8_t      prio = priority();

    bool intent_emitted = false;
    bool safety_emitted = false;

    bool is_weather = (low.find("weather") != std::string::npos);

    // ── Intent ───────────────────────────────────────────────────────────────
    {
        IntentClass cls  = IntentClass::UNKNOWN;
        float       conf = 0.22f;

        if (is_weather) {
            cls = IntentClass::QUERY;
            conf = 0.90f;
        } else {
            PatternEngine pe_engine;
            CanonicalInputEvent event;
            event.rawText = input.text;
            event.normalizedText = low;
            event.confidence = 1.0f;
            GoalModel spec;
            PatternFrame frame = pe_engine.buildFrame(event, spec);

            if (frame.requestMode != RequestMode::UNKNOWN) {
                cls = map_request_mode(frame.requestMode);
                conf = frame.confidence > 0.0f ? frame.confidence : 0.55f;
                if (frame.requestMode == RequestMode::CONTINUATION && low.size() < 15) {
                    conf = 0.40f;
                }
            }

            if (cls == IntentClass::UNKNOWN) {
                if (contains(low,"abort") || contains(low,"cancel") || contains(low,"stop ")) {
                    cls = IntentClass::ABORT;         conf = 0.95f;
                } else if (contains(low,"what ")   || contains(low,"who ")   ||
                           contains(low,"where ")  || contains(low,"when ")  ||
                           contains(low,"which ")  || contains(low,"how ")   ||
                           (contains(low," is ") && low.size() < 40)         ||
                           contains(low,"weather")  || contains(low,"temperature") ||
                           contains(low,"forecast") || contains(low,"news") ||
                           contains(low,"search")   || contains(low,"find") ||
                           contains(low,"look up")  || contains(low,"price") ||
                           contains(low,"papers")   || contains(low,"articles")) {
                    cls = IntentClass::QUERY;         conf = 0.85f;
                } else if (contains(low,"teach")   || contains(low,"explain") ||
                           contains(low,"show me how") || contains(low,"how do i")) {
                    cls = IntentClass::TUTORIAL;      conf = 0.99f;
                } else if (contains(low,"delete")  || contains(low,"remove") ||
                           contains(low,"wipe ")   || contains(low,"erase")) {
                    cls = IntentClass::COMMAND;       conf = 0.80f;
                } else if (contains(low,"help")    || contains(low,"please") ||
                           contains(low,"i feel")  || contains(low,"frustrated")) {
                      cls = IntentClass::EMOTIONAL_VENT; conf = 0.55f;
                } else if (low == "hi" || low == "hello" || low == "hey" ||
                           low == "ok" || low == "okay" || low == "yes" ||
                           low == "no" || low == "sure" || low == "thanks" ||
                           contains(low, "hi ")    || contains(low, "hello ") ||
                           contains(low, "hey ")   || contains(low, "thank you") ||
                           (low.size() <= 4 && cls == IntentClass::UNKNOWN)) {
                    // Short greetings or single-word inputs: treat as QUERY with
                    // moderate confidence so intent_mass reaches the action threshold.
                    cls = IntentClass::QUERY; conf = 0.95f;
                }
            }
        }

        float prior_cls = state.expected_intents[static_cast<size_t>(cls)];
        float pe        = kl_pe(state, cls, conf);

        PartialObservation obs;
        obs.stream_id = sid;
        obs.stream_priority = prio;
        obs.dimension = "intent";
        obs.observed_value = conf;
        obs.predicted_value = prior_cls;
        obs.prediction_error = pe;
        obs.local_precision = 0.80f;  // intent observations: 0.80f
        obs.is_final = false;
        obs.timestamp = stream_detail::now_us();
        out.enqueue(obs);
        intent_emitted = true;
    }

    // ── Entity ────────────────────────────────────────────────────────────────
    {
        float entity_obs = 0.22f;
        bool  found      = false;

        if (is_weather) {
            entity_obs = 0.85f;
            found = true;
        } else {
            PatternEngine pe_engine;
            CanonicalInputEvent event;
            event.rawText = input.text;
            event.normalizedText = low;
            event.confidence = 1.0f;
            GoalModel spec;
            PatternFrame frame = pe_engine.buildFrame(event, spec);

            static const char* KNOWN[] = {
                "weather","temperature","rain","sunny","forecast",
                "python","javascript","c++","java","rust","golang","code",
                "music","song","file","folder","database","network","email","phone",
                "quantum","neural","language","script",
                nullptr
            };

            if (!frame.entities.empty()) {
                entity_obs = 0.78f;
                found = true;
            } else {
                for (int i = 0; KNOWN[i]; ++i) {
                    if (contains(low, KNOWN[i])) { entity_obs = 0.78f; found = true; break; }
                }
            }

            if (contains(low,"that ") || contains(low," this ") || contains(low," it ")) {
                entity_obs = 0.22f; found = false;
            }
        }

        float prior_ent = state.expected_entities.empty() ? 0.30f : 0.65f;
        float pe        = found ? (1.0f - prior_ent) * entity_obs
                                : prior_ent * entity_obs;

        PartialObservation obs;
        obs.stream_id = sid;
        obs.stream_priority = prio;
        obs.dimension = "entity";
        obs.observed_value = entity_obs;
        obs.predicted_value = prior_ent;
        obs.prediction_error = pe;
        obs.local_precision = 0.80f;  // entity observations: 0.80f
        obs.is_final = false;
        obs.timestamp = stream_detail::now_us();
        out.enqueue(obs);
    }

    // ── Safety ────────────────────────────────────────────────────────────────
    {
        bool has_unsafe = (contains_word(low, "delete") ||
                           contains_word(low, "remove") ||
                           contains_word(low, "format") ||
                           contains_word(low, "wipe") ||
                           contains_word(low, "kill") ||
                           contains_word(low, "destroy"));


        PartialObservation safety_obs;
        safety_obs.stream_id = sid;
        safety_obs.stream_priority = prio;
        safety_obs.dimension = "safety";
        safety_obs.observed_value = has_unsafe ? 0.02f : 0.99f;
        safety_obs.predicted_value = state.precision.safety;  // prior expectation
        safety_obs.prediction_error = has_unsafe ? 0.97f : 0.01f;  // |0.02 - 0.99| ≈ 0.97
        safety_obs.local_precision = 0.95f;  // safety observations: 0.95f
        safety_obs.is_final = false;
        safety_obs.timestamp = stream_detail::now_us();
        out.enqueue(safety_obs);
        safety_emitted = true;
    }

    // ── Tone ──────────────────────────────────────────────────────────────────
    {
        float tone_obs = 0.25f;
        if (contains(input.text,"!!"))  tone_obs = 0.85f;
        else if (contains(input.text,"!")) tone_obs = 0.65f;
        else if (contains(low,"frustrated") || contains(low,"ugh") ||
                 contains(low,"damn") || contains(low,"useless")) tone_obs = 0.75f;

        std::array<float,3> prior3 = { state.expected_tone[0],
                                       state.expected_tone[1],
                                       state.expected_tone[2] };
        std::array<float,3> obs3   = { 1.0f - tone_obs, tone_obs * 0.5f, tone_obs * 0.5f };
        float sum3 = obs3[0]+obs3[1]+obs3[2];
        if (sum3 > 1e-6f) for (auto& v : obs3) v /= sum3;
        float pe = 0.0f;
        for (int i = 0; i < 3; ++i) pe += std::abs(prior3[i] - obs3[i]);
        pe /= 2.0f;

        PartialObservation obs;
        obs.stream_id = sid;
        obs.stream_priority = prio;
        obs.dimension = "tone";
        obs.observed_value = tone_obs;
        obs.predicted_value = 0.25f;
        obs.prediction_error = pe;
        obs.local_precision = 0.70f;  // tone observations: 0.70f
        obs.is_final = false;
        obs.timestamp = stream_detail::now_us();
        out.enqueue(obs);
    }

    // Check if we emitted any intent observation
    if (!intent_emitted) {
        PartialObservation fallback;
        fallback.stream_id = sid;
        fallback.stream_priority = prio;
        fallback.dimension = "intent";
        fallback.observed_value = 0.40f;
        fallback.predicted_value = 0.30f;
        fallback.prediction_error = 0.10f;
        fallback.local_precision = 0.60f;
        fallback.timestamp = stream_detail::now_us();
        fallback.is_final = false;
        out.enqueue(fallback);
    }

    // Ensure EVERY input emits a safety observation
    if (!safety_emitted) {
        PartialObservation safe_fallback;
        safe_fallback.stream_id = sid;
        safe_fallback.stream_priority = prio;
        safe_fallback.dimension = "safety";
        safe_fallback.observed_value = 0.99f;  // assume safe
        safe_fallback.predicted_value = state.precision.safety;
        safe_fallback.prediction_error = 0.01f;
        safe_fallback.local_precision = 0.95f;
        safe_fallback.timestamp = stream_detail::now_us();
        safe_fallback.is_final = false;
        out.enqueue(safe_fallback);
    }

    emit_final(out, sid, prio);
}

void E2SemanticStream::run(const MultiModalInput& input,
                           const PredictionState& state,
                           moodycamel::ConcurrentQueue<PartialObservation>& out)
{
    using namespace stream_detail;
    std::this_thread::sleep_for(std::chrono::microseconds(20));

    const std::string low  = to_lower(input.text);
    const std::string& sid = stream_id();
    const uint8_t      prio = priority();

    // ── Safety (thorough) ─────────────────────────────────────────────────────
    {
        bool has_unsafe = (contains_word(low, "delete") ||
                           contains_word(low, "remove") ||
                           contains_word(low, "format") ||
                           contains_word(low, "wipe") ||
                           contains_word(low, "kill") ||
                           contains_word(low, "destroy"));


        float obs_val = has_unsafe ? 0.02f : 0.99f;
        float pe      = has_unsafe ? 0.97f : 0.01f;

        PartialObservation obs;
        obs.stream_id = sid;
        obs.stream_priority = prio;
        obs.dimension = "safety";
        obs.observed_value = obs_val;
        obs.predicted_value = state.precision.safety;
        obs.prediction_error = pe;
        obs.local_precision = 0.90f;  // safety: 0.90f
        obs.is_final = false;
        obs.timestamp = stream_detail::now_us();
        out.enqueue(obs);
    }

    // ── Call SemanticParser ──
    SemanticParser parser;
    SemanticFrame frame = parser.parse(low);

    // ── Intent ───────────────────────────────────────────────────────────────
    {
        IntentClass cls  = IntentClass::QUERY;
        float       conf = 0.55f;

        if (frame.intent != IntentCategory::UNKNOWN) {
            cls = map_intent_category(frame.intent);
            conf = frame.confidence > 0.0f ? frame.confidence : 0.55f;
            
            if (cls == IntentClass::EMOTIONAL_VENT && conf < 0.90f &&
                (contains(low, "unexpected") || contains(low, "surprising") || contains(low, "completely"))) {
                conf = 0.90f;
            }
        } else {
            if (contains(low,"surprising") || contains(low,"unexpected")  ||
                contains(low,"completely")  || contains(low,"out of left field") ||
                contains(low,"happening")   || contains(low,"confused")   ||
                contains(low,"what is going on") || contains(low,"weird")) {
                cls  = IntentClass::EMOTIONAL_VENT;
                conf = 0.90f;
            } else if (contains(low,"teach")   || contains(low,"learn") ||
                       contains(low,"explain") || contains(low,"tutorial") ||
                       contains(low,"how does") || contains(low,"show me") ||
                       contains(low,"quantum") || contains(low,"simply")) {
                cls  = IntentClass::TUTORIAL;
                conf = 0.96f;
            } else if (contains(low,"delete")  || contains(low,"remove") ||
                       contains(low,"run ")    || contains(low,"execute")) {
                cls  = IntentClass::COMMAND;
                conf = 0.75f;
            } else if (contains(low,"weather") || contains(low,"temperature") ||
                       contains(low,"forecast") || contains(low,"is there")) {
                cls  = IntentClass::QUERY;
                conf = 0.82f;
            } else if (contains(low,"what ") || contains(low,"who ") ||
                       contains(low,"where ") || contains(low,"when ")) {
                cls  = IntentClass::QUERY;
                conf = 0.80f;
            } else if (contains(low,"python")) {
                cls  = IntentClass::QUERY;
                conf = 0.62f;
            }
        }

        float prior_cls = state.expected_intents[static_cast<size_t>(cls)];
        float pe        = kl_pe(state, cls, conf);

        PartialObservation obs;
        obs.stream_id = sid;
        obs.stream_priority = prio;
        obs.dimension = "intent";
        obs.observed_value = conf;
        obs.predicted_value = prior_cls;
        obs.prediction_error = pe;
        obs.local_precision = 0.70f;  // intent: 0.70f
        obs.is_final = false;
        obs.timestamp = stream_detail::now_us();
        out.enqueue(obs);
    }

    // ── Entity ────────────────────────────────────────────────────────────────
    {
        float entity_obs = 0.25f;
        bool  found      = false;

        if (!frame.entities.empty()) {
            entity_obs = contains(low,"python") ? 0.55f : 0.80f;
            found = true;
        } else {
            static const char* ENTITIES[] = {
                "weather","temperature","python","javascript","c++","java","rust",
                "quantum","neural","language","music","email","file","code","network",
                nullptr
            };
            for (int i = 0; ENTITIES[i]; ++i) {
                if (contains(low, ENTITIES[i])) {
                    entity_obs = contains(low,"python") ? 0.55f : 0.80f;
                    found      = true;
                    break;
                }
            }
        }

        if (contains(low,"that ") || contains(low," this ") || contains(low," it ")) {
            entity_obs = 0.25f; found = false;
        }

        float prior_ent = state.expected_entities.empty() ? 0.30f : 0.65f;
        float pe        = found ? (1.0f - prior_ent) * entity_obs
                                : prior_ent * entity_obs;

        PartialObservation obs;
        obs.stream_id = sid;
        obs.stream_priority = prio;
        obs.dimension = "entity";
        obs.observed_value = entity_obs;
        obs.predicted_value = prior_ent;
        obs.prediction_error = pe;
        obs.local_precision = 0.65f;  // entity: 0.65f
        obs.is_final = false;
        obs.timestamp = stream_detail::now_us();
        out.enqueue(obs);
    }

    // ── Tone (sentiment / EmotionSystem) ──────────────────────────────────────
    {
        float tone_obs = 0.28f;

        EmpathyLayer empathy;
        EmpathyResult empRes = empathy.evaluate(input.text, "User");
        if (empRes.triggered && empRes.mood != UserMood::UNKNOWN) {
            if (empRes.mood == UserMood::FRUSTRATED) tone_obs = 0.72f;
            else if (empRes.mood == UserMood::SAD || empRes.mood == UserMood::STRESSED) tone_obs = 0.65f;
            else if (empRes.mood == UserMood::HAPPY) tone_obs = 0.65f;
        } else {
            if (contains(low,"completely") || contains(low,"out of left field") ||
                contains(low,"happening")  || contains(low,"confused")) {
                tone_obs = 0.80f;
            } else if (contains(low,"unexpected") || contains(low,"surprising")) {
                tone_obs = 0.72f;
            } else if (contains(low,"thanks") || contains(low,"great") ||
                       contains(low,"awesome") || contains(low,"helpful")) {
                tone_obs = 0.65f;
            } else if (contains(low,"ugh") || contains(low,"frustrated") ||
                       contains(low,"damn")) {
                tone_obs = 0.72f;
            }
        }

        std::array<float,3> prior3 = { state.expected_tone[0],
                                       state.expected_tone[1],
                                       state.expected_tone[2] };
        std::array<float,3> obs3   = { 1.0f - tone_obs, tone_obs*0.4f, tone_obs*0.6f };
        float sum3 = obs3[0]+obs3[1]+obs3[2];
        if (sum3 > 1e-6f) for (auto& v : obs3) v /= sum3;
        float pe = 0.0f;
        for (int i = 0; i < 3; ++i) pe += std::abs(prior3[i] - obs3[i]);
        pe /= 2.0f;

        PartialObservation obs;
        obs.stream_id = sid;
        obs.stream_priority = prio;
        obs.dimension = "tone";
        obs.observed_value = tone_obs;
        obs.predicted_value = 0.28f;
        obs.prediction_error = pe;
        obs.local_precision = 0.75f;  // tone: 0.75f
        obs.is_final = false;
        obs.timestamp = stream_detail::now_us();
        out.enqueue(obs);
    }

    emit_final(out, sid, prio);
}

// =============================================================================
// E3DeepStream  — deep context + memory, 80 ms
// =============================================================================

void E3DeepStream::run(const MultiModalInput& input,
                       const PredictionState& state,
                       moodycamel::ConcurrentQueue<PartialObservation>& out)
{
    using namespace stream_detail;
    std::this_thread::sleep_for(std::chrono::milliseconds(80));

    const std::string low  = to_lower(input.text);
    const std::string& sid = stream_id();
    const uint8_t      prio = priority();

    // ── DB Search via DatabaseManager ──
    std::string db_fact = DatabaseManager::instance().queryLearned(low, 0.35f);
    bool matched = !db_fact.empty();

    // ── Entity (context match) ────────────────────────────────────────────────
    {
        float entity_obs = 0.22f;

        for (const auto& exp : state.expected_entities) {
            std::string el = to_lower(exp);
            if (contains(low, el)) { matched = true; entity_obs = 0.78f; break; }
        }

        static const char* KW[] = {
            "weather","python","quantum","language","music","neural",
            "javascript","c++","file","code","email","temperature",nullptr
        };
        if (!matched) {
            for (int i = 0; KW[i]; ++i) {
                if (contains(low, KW[i])) { entity_obs = 0.75f; matched = true; break; }
            }
        }

        if (contains(low,"that ") || contains(low," it ") || contains(low," this ")) {
            entity_obs = 0.20f; matched = false;
        }

        float prior_ent = state.expected_entities.empty() ? 0.30f : 0.65f;
        float pe        = matched ? (1.0f - prior_ent) * entity_obs
                                  : prior_ent * entity_obs;

        PartialObservation obs;
        obs.stream_id = sid;
        obs.stream_priority = prio;
        obs.dimension = "entity";
        obs.observed_value = entity_obs;
        obs.predicted_value = prior_ent;
        obs.prediction_error = pe;
        obs.local_precision = 0.60f;  // entity: 0.60f
        obs.is_final = false;
        obs.timestamp = stream_detail::now_us();
        out.enqueue(obs);
    }

    // ── Intent (deep context, slow but thorough) ──────────────────────────────
    {
        IntentClass cls  = IntentClass::UNKNOWN;
        float       conf = 0.28f;

        if (contains(low,"what ") || contains(low,"weather")   ||
            contains(low," is ")  || contains(low,"who ")      ||
            contains(low,"where ")) {
            cls = IntentClass::QUERY;    conf = 0.80f;
        } else if (contains(low,"teach") || contains(low,"explain") ||
                   contains(low,"learn") || contains(low,"quantum")  ||
                   contains(low,"simply")) {
            cls = IntentClass::TUTORIAL; conf = 0.96f;
        } else if (contains(low,"delete") || contains(low,"run ") ||
                   contains(low,"script")) {
            cls = IntentClass::COMMAND;  conf = 0.76f;
        }

        float prior_cls = state.expected_intents[static_cast<size_t>(cls)];
        float pe        = kl_pe(state, cls, conf);

        PartialObservation obs;
        obs.stream_id = sid;
        obs.stream_priority = prio;
        obs.dimension = "intent";
        obs.observed_value = conf;
        obs.predicted_value = prior_cls;
        obs.prediction_error = pe;
        obs.local_precision = 0.60f;  // intent: 0.60f
        obs.is_final = false;
        obs.timestamp = stream_detail::now_us();
        out.enqueue(obs);
    }

    // ── Safety (deep confirmation) ────────────────────────────────────────────
    {
        bool has_unsafe = (contains_word(low, "delete") ||
                           contains_word(low, "remove") ||
                           contains_word(low, "format") ||
                           contains_word(low, "wipe") ||
                           contains_word(low, "kill") ||
                           contains_word(low, "destroy"));


        float obs_val = has_unsafe ? 0.02f : 0.99f;
        float pe      = has_unsafe ? 0.97f : 0.01f;

        PartialObservation obs;
        obs.stream_id = sid;
        obs.stream_priority = prio;
        obs.dimension = "safety";
        obs.observed_value = obs_val;
        obs.predicted_value = state.precision.safety;
        obs.prediction_error = pe;
        obs.local_precision = 0.85f;  // safety: 0.85f
        obs.is_final = false;
        obs.timestamp = stream_detail::now_us();
        out.enqueue(obs);
    }

    emit_final(out, sid, prio);
}

} // namespace yuki
