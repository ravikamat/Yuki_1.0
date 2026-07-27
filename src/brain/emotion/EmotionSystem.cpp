// EmotionSystem.cpp — Emotion state + empathy detection (merged from EmotionState + EmpathyLayer)
#define NOMINMAX
#include "brain/emotion/EmotionSystem.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iostream>
#include <cmath>
#include <cctype>
#include <cstdlib>
#include <ctime>

// ══════════════════════════════════════════════════════════════════════════════
// ValenceArousalModel
// ══════════════════════════════════════════════════════════════════════════════

namespace yuki::emotion {

ValenceArousalModel::ValenceArousalModel()
    : valence_(0.0f),
      arousal_(0.0f),
      valence_ema_(0.0f),
      arousal_ema_(0.0f) {
    reset();
}

float ValenceArousalModel::clamp(float v, float lo, float hi) const {
    return (v < lo) ? lo : ((v > hi) ? hi : v);
}

void ValenceArousalModel::reset() {
    valence_ema_ = 0.0f;
    arousal_ema_ = 0.0f;
    valence_.store(0.0f, std::memory_order_release);
    arousal_.store(0.0f, std::memory_order_release);
}

void ValenceArousalModel::update(float outcome_reward,
                                 float user_feedback_signal,
                                 float task_difficulty,
                                 float surprise) {
    float raw_valence = kEmaAlpha * (outcome_reward * 0.4f + user_feedback_signal * 0.4f + (0.5f - task_difficulty) * 0.2f) + (1.0f - kEmaAlpha) * valence_ema_;
    float raw_arousal = kEmaAlpha * (surprise * 0.5f + task_difficulty * 0.3f + std::abs(outcome_reward) * 0.2f) + (1.0f - kEmaAlpha) * arousal_ema_;

    valence_ema_ = clamp(raw_valence, kValenceMin, kValenceMax);
    arousal_ema_ = clamp(raw_arousal, kArousalMin, kArousalMax);

    valence_.store(valence_ema_, std::memory_order_release);
    arousal_.store(arousal_ema_, std::memory_order_release);
}

float ValenceArousalModel::modulateThreshold(float base_threshold) const {
    float arousal_factor = 1.0f + 0.15f * (arousal() - 0.5f);
    float valence_factor = 1.0f - 0.1f * valence();
    return base_threshold * arousal_factor * valence_factor;
}

void ValenceArousalModel::decay() {
    valence_ema_ *= kDecayRate;
    arousal_ema_ *= kDecayRate;
    valence_.store(valence_ema_, std::memory_order_release);
    arousal_.store(arousal_ema_, std::memory_order_release);
}

std::vector<uint8_t> ValenceArousalModel::serialize() const {
    std::vector<uint8_t> out;
    out.reserve(14);

    auto append = [&out](const void* ptr, size_t size) {
        const uint8_t* p = reinterpret_cast<const uint8_t*>(ptr);
        out.insert(out.end(), p, p + size);
    };

    uint32_t magic = kSerializationMagic;
    uint16_t ver = kSerializationVersion;

    append(&magic, sizeof(magic));
    append(&ver, sizeof(ver));
    append(&valence_ema_, sizeof(valence_ema_));
    append(&arousal_ema_, sizeof(arousal_ema_));

    return out;
}

bool ValenceArousalModel::deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < 14) return false;

    uint32_t magic = 0;
    uint16_t ver = 0;
    std::memcpy(&magic, data.data(), sizeof(magic));
    std::memcpy(&ver, data.data() + 4, sizeof(ver));

    if (magic != kSerializationMagic || ver != kSerializationVersion) return false;

    size_t offset = 6;
    std::memcpy(&valence_ema_, data.data() + offset, sizeof(valence_ema_));
    offset += sizeof(valence_ema_);

    std::memcpy(&arousal_ema_, data.data() + offset, sizeof(arousal_ema_));

    valence_.store(valence_ema_, std::memory_order_release);
    arousal_.store(arousal_ema_, std::memory_order_release);

    return true;
}

} // namespace yuki::emotion

// ══════════════════════════════════════════════════════════════════════════════
// EmotionState
// ══════════════════════════════════════════════════════════════════════════════

static const char* EMOTION_FILE = "data/brain/emotion.json";

EmotionState::EmotionState() { load(); }

void EmotionState::update(bool gotGoodAnswer, bool userFrustrated,
                           bool taskCompleted, bool newTopicLearned, float answerConfidence)
{
    std::lock_guard<std::mutex> lock(mu_);
    ++state_.turnCount;
    if (gotGoodAnswer)  state_.valence += 0.08f;
    if (taskCompleted)  state_.valence += 0.10f;
    if (userFrustrated) state_.valence -= 0.12f;
    if (!gotGoodAnswer) state_.valence -= 0.04f;
    state_.valence *= 0.92f;
    if (newTopicLearned)  state_.arousal += 0.06f;
    if (gotGoodAnswer)    state_.arousal += 0.03f;
    if (userFrustrated)   state_.arousal += 0.08f;
    state_.arousal *= 0.88f;
    if (!gotGoodAnswer)  state_.curiosity += 0.05f;
    if (newTopicLearned) state_.curiosity += 0.04f;
    if (gotGoodAnswer && answerConfidence > 0.8f) state_.curiosity -= 0.02f;
    state_.curiosity = state_.curiosity * 0.95f + 0.5f * 0.05f;
    state_.fatigue += 0.01f;
    if (taskCompleted) state_.fatigue += 0.03f;
    state_.fatigue *= 0.97f;
    clamp();
    if (state_.turnCount % 10 == 0) save();
}

EmotionSnapshot EmotionState::snapshot() const {
    std::lock_guard<std::mutex> lock(mu_);
    return state_;
}

std::string EmotionState::toneResponse(const std::string& raw) const {
    if (raw.empty()) return raw;
    std::lock_guard<std::mutex> lock(mu_);
    if (state_.fatigue > 0.6f) return raw;  // too tired to add flair

    std::ostringstream prefix;
    // Build prefix entirely from float state — no fixed strings
    if (state_.valence < -0.3f && state_.arousal > 0.4f) {
        // Frustrated but engaged — show determination
        int tryPct = static_cast<int>(state_.arousal * 100);
        prefix << "[" << tryPct << "% focused] ";
    } else if (state_.valence > 0.3f && state_.curiosity > 0.65f) {
        // Happy and curious — show enthusiasm score
        int enthu = static_cast<int>((state_.valence + state_.curiosity) * 50);
        prefix << "[enthusiasm: " << enthu << "] ";
    } else if (state_.curiosity > 0.75f) {
        int curioPct = static_cast<int>(state_.curiosity * 100);
        prefix << "[curiosity: " << curioPct << "%] ";
    }
    // No prefix for neutral/calm states — clean output
    return prefix.str() + raw;
}

std::string EmotionState::describeMood() const {
    std::lock_guard<std::mutex> lock(mu_);
    std::string valenceLabel, arousalLabel, curiosityNote, fatigueNote;
    if      (state_.valence >  0.5f) valenceLabel = "very content";
    else if (state_.valence >  0.2f) valenceLabel = "good";
    else if (state_.valence > -0.1f) valenceLabel = "neutral";
    else if (state_.valence > -0.4f) valenceLabel = "a bit frustrated";
    else                              valenceLabel = "stressed";
    if      (state_.arousal > 0.6f) arousalLabel = "highly engaged";
    else if (state_.arousal > 0.3f) arousalLabel = "active";
    else                             arousalLabel = "calm";
    if      (state_.curiosity > 0.75f) curiosityNote = ", very curious to learn more";
    else if (state_.curiosity > 0.6f)  curiosityNote = ", curious";
    if (state_.fatigue > 0.5f) fatigueNote = ". I've been working hard";
    std::ostringstream ss;
    ss << "I'm feeling " << valenceLabel << " and " << arousalLabel
       << curiosityNote << fatigueNote
       << ". I've had " << state_.turnCount << " interactions this session.";
    return ss.str();
}

void EmotionState::clamp() {
    state_.valence   = std::max(-1.0f, std::min(1.0f, state_.valence));
    state_.arousal   = std::max(0.0f,  std::min(1.0f, state_.arousal));
    state_.curiosity = std::max(0.0f,  std::min(1.0f, state_.curiosity));
    state_.fatigue   = std::max(0.0f,  std::min(1.0f, state_.fatigue));
}

void EmotionState::save() const {
    std::ofstream f(EMOTION_FILE);
    if (!f.is_open()) return;
    f << "{\n"
      << "  \"valence\":"   << state_.valence   << ",\n"
      << "  \"arousal\":"   << state_.arousal   << ",\n"
      << "  \"curiosity\":" << state_.curiosity << ",\n"
      << "  \"fatigue\":"   << state_.fatigue   << ",\n"
      << "  \"turnCount\":" << state_.turnCount << "\n"
      << "}\n";
}

void EmotionState::load() {
    std::ifstream f(EMOTION_FILE);
    if (!f.is_open()) return;
    auto extractFloat = [](const std::string& line, float& out) {
        auto p = line.find(':');
        if (p == std::string::npos) return;
        try { out = std::stof(line.substr(p+1)); } catch (...) {}
    };
    auto extractInt = [](const std::string& line, int& out) {
        auto p = line.find(':');
        if (p == std::string::npos) return;
        try { out = std::stoi(line.substr(p+1)); } catch (...) {}
    };
    std::string line;
    while (std::getline(f, line)) {
        if (line.find("valence")   != std::string::npos) extractFloat(line, state_.valence);
        if (line.find("arousal")   != std::string::npos) extractFloat(line, state_.arousal);
        if (line.find("curiosity") != std::string::npos) extractFloat(line, state_.curiosity);
        if (line.find("fatigue")   != std::string::npos) extractFloat(line, state_.fatigue);
        if (line.find("turnCount") != std::string::npos) extractInt(line, state_.turnCount);
    }
    clamp();
    std::cout << "[Emotion] Loaded: valence=" << state_.valence
              << " curiosity=" << state_.curiosity
              << " turns=" << state_.turnCount << "\n";
}

// ══════════════════════════════════════════════════════════════════════════════
// EmpathyLayer
// ══════════════════════════════════════════════════════════════════════════════

std::string EmpathyLayer::toLower(const std::string& s) {
    std::string r = s;
    std::transform(r.begin(), r.end(), r.begin(),
                   [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    return r;
}
bool EmpathyLayer::has(const std::string& h, const std::string& n) {
    return h.find(n) != std::string::npos;
}

UserMood EmpathyLayer::detectMood(const std::string& lower, float& intensity) const {
    intensity = 0.6f;
    if (has(lower,"not feeling well")||has(lower,"not well")||has(lower,"feeling sick")||
        has(lower,"feeling ill")||has(lower,"i am sick")||has(lower,"i'm sick")||
        has(lower,"i am ill")||has(lower,"i'm ill")||has(lower,"not okay")||has(lower,"not ok")||
        has(lower,"feeling bad")||has(lower,"feeling unwell")||has(lower,"have a fever")||
        has(lower,"got fever")||has(lower,"stomach ache")||has(lower,"headache")||
        has(lower,"body pain")||has(lower,"body ache")||has(lower,"running nose")||
        has(lower,"runny nose")||has(lower,"cold and cough")||has(lower,"feeling weak")||
        has(lower,"under the weather")) {
        if (has(lower,"very")||has(lower,"really")||has(lower,"terrible")) intensity = 0.90f;
        return UserMood::UNWELL;
    }
    if (has(lower,"i am tired")||has(lower,"i'm tired")||has(lower,"feeling tired")||
        has(lower,"so tired")||has(lower,"exhausted")||has(lower,"very sleepy")||
        has(lower,"no energy")||has(lower,"low energy")||has(lower,"burned out")||
        has(lower,"burnt out")||has(lower,"drained")||has(lower,"fatigued")) {
        if (has(lower,"very")||has(lower,"so")||has(lower,"completely")) intensity = 0.85f;
        return UserMood::TIRED;
    }
    if (has(lower,"i am stressed")||has(lower,"i'm stressed")||has(lower,"feeling stressed")||
        has(lower,"very stressed")||has(lower,"overwhelmed")||has(lower,"anxious")||
        has(lower,"i am worried")||has(lower,"i'm worried")||has(lower,"too much pressure")||
        has(lower,"under pressure")||has(lower,"can't handle")||has(lower,"cannot handle")||
        has(lower,"too much work")||has(lower,"losing my mind")||has(lower,"panicking")||has(lower,"panic")) {
        if (has(lower,"very")||has(lower,"really")||has(lower,"so")) intensity = 0.88f;
        return UserMood::STRESSED;
    }
    if (has(lower,"i am sad")||has(lower,"i'm sad")||has(lower,"feeling sad")||
        has(lower,"feeling down")||has(lower,"depressed")||has(lower,"feeling low")||
        has(lower,"feeling lonely")||has(lower,"very lonely")||has(lower,"heartbroken")||
        has(lower,"feeling hopeless")||has(lower,"crying")||has(lower,"want to cry")||
        has(lower,"lost someone")||has(lower,"someone passed")||
        // Relationship / breakup patterns
        has(lower,"girlfriend left")||has(lower,"boyfriend left")||
        has(lower,"she left me")||has(lower,"he left me")||
        has(lower,"broke up with me")||has(lower,"we broke up")||
        has(lower,"breakup")||has(lower,"break up")||
        has(lower,"i miss her")||has(lower,"i miss him")||has(lower,"i miss them")||
        has(lower,"miss her so")||has(lower,"miss him so")||
        has(lower,"feeling alone")||has(lower,"all alone")||has(lower,"no one cares")||
        has(lower,"nobody cares")||has(lower,"feel so empty")||has(lower,"feeling empty")||
        has(lower,"i am broken")||has(lower,"i'm broken")||
        has(lower,"she doesn't love")||has(lower,"he doesn't love")||has(lower,"lost my")||
        // Betrayal / cheating patterns
        has(lower,"cheated on me")||has(lower,"she cheated")||has(lower,"he cheated")||
        has(lower,"betrayed me")||has(lower,"she betrayed")||has(lower,"he betrayed")||
        has(lower,"my girlfriend cheated")||has(lower,"my boyfriend cheated")||has(lower,"my partner cheated")||has(lower,"was cheating")||has(lower,"been cheating")) {
        if (has(lower,"very")||has(lower,"really")||has(lower,"so")||has(lower,"cheated")||has(lower,"betrayed")) intensity = 0.95f;
        return UserMood::SAD;
    }
    // Crime / loss of property
    if (has(lower,"my money was stolen")||has(lower,"got robbed")||has(lower,"was robbed")||
        has(lower,"my phone was stolen")||has(lower,"my wallet was stolen")||
        has(lower,"someone stole")||has(lower,"i was scammed")||has(lower,"got scammed")||
        has(lower,"money stolen")||has(lower,"stolen my")||has(lower,"robbed me")) {
        intensity = 0.85f;
        return UserMood::STRESSED;
    }
    if (has(lower,"i am frustrated")||has(lower,"i'm frustrated")||has(lower,"so frustrated")||
        has(lower,"very frustrated")||has(lower,"i am angry")||has(lower,"i'm angry")||
        has(lower,"i am annoyed")||has(lower,"i'm annoyed")||has(lower,"this is frustrating")||
        has(lower,"hate this")||has(lower,"fed up")||has(lower,"so annoyed")) {
        if (has(lower,"very")||has(lower,"so")||has(lower,"extremely")) intensity = 0.85f;
        return UserMood::FRUSTRATED;
    }
    if (has(lower,"i am happy")||has(lower,"i'm happy")||has(lower,"feeling great")||
        has(lower,"feeling amazing")||has(lower,"i am excited")||has(lower,"i'm excited")||
        has(lower,"so happy")||has(lower,"very happy")||has(lower,"feeling good")||
        has(lower,"feeling wonderful")||has(lower,"on top of the world")||has(lower,"feeling fantastic")) {
        if (has(lower,"very")||has(lower,"so")||has(lower,"amazing")) intensity = 0.90f;
        return UserMood::HAPPY;
    }
    if (has(lower,"i got promoted")||has(lower,"got a promotion")||has(lower,"i passed")||
        has(lower,"i cleared")||has(lower,"i finished")||has(lower,"i completed")||
        has(lower,"i achieved")||has(lower,"feeling proud")||has(lower,"great news")||
        has(lower,"good news")||has(lower,"we won")||
        // "i won" is only PROUD for competitions/exams — not minor wins like coupons
        (has(lower,"i won") && !has(lower,"coupon") && !has(lower,"coupn") &&
                               !has(lower,"discount") && !has(lower,"prize ticket"))) {
        intensity = 0.88f;
        return UserMood::PROUD;
    }
    return UserMood::UNKNOWN;
}

// Removed hardcoded fallbackSuggestion, buildOpener, and buildResponse

EmpathyResult EmpathyLayer::evaluate(const std::string& rawInput, const std::string& /*userName*/) const {
    EmpathyResult result;
    const std::string lower = toLower(rawInput);
    float intensity = 0.0f;
    UserMood mood = detectMood(lower, intensity);
    if (mood == UserMood::UNKNOWN) return result;
    result.triggered   = true;
    result.mood        = mood;
    result.intensity   = intensity;
    switch (mood) {
    case UserMood::UNWELL:     result.moodLabel = "unwell";      break;
    case UserMood::TIRED:      result.moodLabel = "tired";       break;
    case UserMood::STRESSED:   result.moodLabel = "stressed";    break;
    case UserMood::SAD:        result.moodLabel = "sad";         break;
    case UserMood::FRUSTRATED: result.moodLabel = "frustrated";  break;
    case UserMood::HAPPY:      result.moodLabel = "happy";       break;
    case UserMood::PROUD:      result.moodLabel = "proud";       break;
    default:                   result.moodLabel = "neutral";     break;
    }
    currentMood_   = mood;
    moodIntensity_ = intensity;
    return result;
}

void EmpathyLayer::resetSession() {
    currentMood_   = UserMood::UNKNOWN;
    moodIntensity_ = 0.0f;
}

// ══════════════════════════════════════════════════════════════════════════════
// EmotionState — GlobalWorkspace integration (production hardening)
// ══════════════════════════════════════════════════════════════════════════════
#include "infrastructure/CoreBus.h"
#include "infrastructure/ModuleRegistry.h"
#include <cmath>

void EmotionState::subscribeToBus() {
    // Subscribe to PERCEPTION_FRAME (multi-modal)
    yuki::gw::CoreBus::instance().subscribe(
        yuki::gw::Topic::PERCEPTION_FRAME, "EmotionSystem",
        [this](const yuki::gw::Message& msg) {
            this->onPerceptionFrame(msg.payload_json);
        });
    // Subscribe to USER_TURN (lexical analysis)
    yuki::gw::CoreBus::instance().subscribe(
        yuki::gw::Topic::USER_TURN, "EmotionSystem",
        [this](const yuki::gw::Message& msg) {
            this->extractAndPublish(msg.payload_json, "{}");
        });
    yuki::infra::ModuleRegistry::instance().heartbeat("EmotionSystem");
}

void EmotionState::onPerceptionFrame(const std::string& /*json_payload*/) {
    // Multi-modal integration stub.
    // Future: parse audio_rms_variance + face_expression_class + screen_brightness.
    // For now: emit current internal state snapshot with low confidence so
    // downstream (PolicySelector, TurnCoordinator) can use it as a baseline.
    EmotionSnapshot snap = snapshot();

    yuki::gw::Message out;
    out.topic         = yuki::gw::Topic::EMOTION_EXTRACTED;
    out.source_module = "EmotionSystem";
    out.salience      = std::abs(snap.valence) * 0.5f + snap.arousal * 0.5f;
    out.payload_json  = "{\"valence\":"    + std::to_string(snap.valence)
                      + ",\"arousal\":"    + std::to_string(snap.arousal)
                      + ",\"dominance\":0.5"
                      + ",\"confidence\":0.3"
                      + ",\"urgency\":0"
                      + ",\"modality\":\"multimodal_snapshot\"}";
    yuki::gw::CoreBus::instance().publish(out);
}

void EmotionState::extractAndPublish(const std::string& text_json,
                                      const std::string& /*audio_features_json*/) {
    // Parse text from USER_TURN JSON payload
    std::string text;
    size_t tpos = text_json.find("\"text\":\"");
    if (tpos != std::string::npos) {
        tpos += 8;
        size_t end = text_json.find('"', tpos);
        if (end != std::string::npos) text = text_json.substr(tpos, end - tpos);
    }
    if (text.empty()) text = text_json;

    // Convert to lowercase for matching
    std::string lower = text;
    for (auto& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    // 22-word VAD lexicon: {word, valence, arousal, dominance}
    struct { const char* word; float v; float a; float d; } lex[] = {
        {"happy",       0.80f,  0.60f, 0.70f},
        {"great",       0.90f,  0.70f, 0.80f},
        {"good",        0.70f,  0.30f, 0.60f},
        {"love",        0.90f,  0.80f, 0.90f},
        {"thanks",      0.60f,  0.30f, 0.50f},
        {"nice",        0.60f,  0.20f, 0.50f},
        {"excited",     0.80f,  0.90f, 0.70f},
        {"bad",        -0.70f,  0.40f, 0.30f},
        {"hate",       -0.90f,  0.80f, 0.90f},
        {"angry",      -0.80f,  0.90f, 0.80f},
        {"sad",        -0.80f, -0.30f, 0.20f},
        {"terrible",   -0.90f,  0.50f, 0.10f},
        {"worst",      -1.00f,  0.60f, 0.10f},
        {"worried",    -0.50f,  0.60f, 0.20f},
        {"bored",      -0.30f, -0.60f, 0.20f},
        {"confused",   -0.20f,  0.50f, 0.10f},
        {"help",        0.00f,  0.40f, 0.30f},
        {"urgent",     -0.20f,  0.90f, 0.80f},
        {"immediately", -0.10f,  0.90f, 0.90f},
        {"asap",       -0.10f,  0.80f, 0.80f},
        {"now",         0.00f,  0.70f, 0.70f},
        {"please",      0.20f,  0.20f, 0.30f},
        {nullptr, 0.f, 0.f, 0.f}
    };

    float v = 0.f, a = 0.f, d = 0.f, w = 0.f;
    int urgent = 0;
    for (int i = 0; lex[i].word; ++i) {
        if (lower.find(lex[i].word) != std::string::npos) {
            v += lex[i].v; a += lex[i].a; d += lex[i].d;
            w += 1.f;
            if (lex[i].a > 0.7f) ++urgent;
        }
    }

    float confidence = std::clamp(w * 0.15f, 0.2f, 0.95f);
    if (w > 0.f) { v /= w; a /= w; d /= w; }
    v = std::clamp(v, -1.f, 1.f);
    a = std::clamp(a, -1.f, 1.f);
    d = std::clamp(d,  0.f, 1.f);

    yuki::gw::Message out;
    out.topic         = yuki::gw::Topic::EMOTION_EXTRACTED;
    out.source_module = "EmotionSystem";
    out.salience      = std::abs(v) + std::abs(a);
    out.payload_json  = "{\"valence\":"    + std::to_string(v)
                      + ",\"arousal\":"    + std::to_string(a)
                      + ",\"dominance\":"  + std::to_string(d)
                      + ",\"confidence\":" + std::to_string(confidence)
                      + ",\"urgency\":"    + std::to_string(urgent)
                      + ",\"modality\":\"lexical\"}";
    yuki::gw::CoreBus::instance().publish(out);
}

