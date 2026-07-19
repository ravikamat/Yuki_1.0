#pragma once
// EmotionSystem.h — Emotion state + empathy detection (merged from EmotionState + EmpathyLayer)
#include <string>
#include <vector>
#include <atomic>
#include <mutex>

// ── §EmotionState ─────────────────────────────────────────────────────────────

struct EmotionSnapshot {
    float valence   = 0.0f;
    float arousal   = 0.0f;
    float curiosity = 0.5f;
    float fatigue   = 0.0f;
    int   turnCount = 0;
};

class EmotionState {
public:
    EmotionState();
    void update(bool gotGoodAnswer, bool userFrustrated,
                bool taskCompleted, bool newTopicLearned, float answerConfidence);
    std::string toneResponse(const std::string& raw) const;
    std::string describeMood() const;
    void save() const;
    void load();
    EmotionSnapshot snapshot() const;
    // GlobalWorkspace integration
    void subscribeToBus();
    void onPerceptionFrame(const std::string& json_payload);
    void extractAndPublish(const std::string& text_json,
                           const std::string& audio_features_json);
private:
    mutable std::mutex mu_;
    EmotionSnapshot    state_;
    void clamp();
};

// ── §EmpathyLayer ─────────────────────────────────────────────────────────────

enum class UserMood {
    UNKNOWN, UNWELL, TIRED, STRESSED, SAD,
    FRUSTRATED, HAPPY, PROUD, NEUTRAL
};

struct EmpathyResult {
    bool        triggered  = false;
    UserMood    mood       = UserMood::UNKNOWN;
    float       intensity  = 0.0f;
    std::string moodLabel;
};

class EmpathyLayer {
public:
    EmpathyLayer() = default;
    EmpathyResult evaluate(const std::string& rawInput, const std::string& userName) const;
    UserMood currentMood()   const { return currentMood_; }
    float    moodIntensity() const { return moodIntensity_; }
    void     resetSession();
private:
    UserMood    detectMood(const std::string& lower, float& intensity) const;
    static std::string toLower(const std::string& s);
    static bool        has(const std::string& h, const std::string& n);
    mutable UserMood currentMood_   = UserMood::UNKNOWN;
    mutable float    moodIntensity_ = 0.0f;
};
