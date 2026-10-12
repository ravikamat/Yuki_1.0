#pragma once

#include <windows.h>
#include <gdiplus.h>
#include <string>
#include <vector>
#include <array>
#include <cmath>

// ═══════════════════════════════════════════════════════════════
//  GROK / X.AI (BLOUB) MORPHING VECTOR AVATAR ENGINE
// ═══════════════════════════════════════════════════════════════

class AvatarRenderer {
public:
    static constexpr int PROFILE_SAMPLES = 64;

    enum class BotShapeId {
        CIRCLE,
        PEBBLE,
        SQUIRCLE,
        CAPSULE,
        TRIANGLE,
        HEXAGON,
        CLOUD,
        TEARDROP,
        EGG,
        ALERT,
        ORBIT,
        SWIRL
    };

    enum class EyeTrackMode {
        NONE,
        MOUSE,
        CAMERA,
        AUTO
    };

    // ── Emotional Drive (injected from Yuki's brain) ──
    struct EmotionalDrive {
        float valence   = 0.0f;   // -1.0 (sad/negative) → +1.0 (happy/positive)
        float arousal   = 0.0f;   // 0.0 (calm) → 1.0 (excited/agitated)
        float curiosity = 0.5f;   // 0.0 → 1.0
        float fatigue   = 0.0f;   // 0.0 → 1.0
        int   dominantEmotion = 0; // Maps to mind::Emotion enum values
    };

    struct Point2D {
        float x = 0.0f;
        float y = 0.0f;
    };

    struct Vec3 {
        float x = 0.0f, y = 0.0f, z = 0.0f;
    };

    struct HeadGaze {
        float yaw = 0.0f;   // Degrees (+ = right)
        float pitch = 0.0f; // Degrees (+ = up)
        float roll = 0.0f;  // Degrees (+ = tilt)
    };

    struct EyePose {
        float x = 0.0f;
        float y = 0.0f;
        float a = 1.0f, b = 0.0f; // 2x2 affine matrix
        float c = 0.0f, d = 1.0f;
        float depth = 1.0f;       // Normal Z component (>0 = visible)
    };

    struct RadialProfile {
        std::array<float, PROFILE_SAMPLES> radii{};
    };

    AvatarRenderer();
    ~AvatarRenderer();

    void initialize();

    // ── Public Configuration & Controls ──
    void setEyeTrackMode(EyeTrackMode mode);
    void setMousePos(float mx, float my);
    void setCameraFocus(float nx, float ny);
    void setShape(BotShapeId shape);
    void nextShape();
    BotShapeId getShape() const { return currentShape_; }
    const char* getShapeName(BotShapeId shape) const;
    void setEmotionalDrive(const EmotionalDrive& drive);

    // ── Main Render Pipeline ──
    void render(Gdiplus::Graphics& g, float cx, float cy,
                const std::string& state, int tick,
                float speakingBounce, float breathOffset,
                const std::string& speechBubble);

private:
    // ── Shape Generators & Math ──
    RadialProfile getProfileForShape(BotShapeId shape);
    RadialProfile blendProfiles(const RadialProfile& a, const RadialProfile& b, float t);
    void computeShapePoints(const RadialProfile& prof, float radius, float cx, float cy,
                            float rot, float sx, float sy, std::vector<Point2D>& outPts);
    void buildCatmullRomPath(const std::vector<Point2D>& pts, Gdiplus::GraphicsPath& outPath, float tension = 1.0f / 6.0f);

    // ── 3D Sphere Eye Math ──
    void computeGazeAngles(float mx, float my, float& outYaw, float& outPitch);
    std::pair<EyePose, EyePose> computeEyePoses(const HeadGaze& gaze, float sphereRadius, float splitDegrees = 15.46f);
    void drawCapsuleEye(Gdiplus::Graphics& g, float cx, float cy, const EyePose& eye,
                        float eyeW, float eyeH, float openScale, const Gdiplus::Color& color);

    // ── Easing & Helpers ──
    static float easeOutQuint(float t);
    static float easeOutCubic(float t);
    static float lerp(float a, float b, float t);
    static float clamp(float v, float lo, float hi);

    // ── Engine State ──
    EyeTrackMode eyeMode_ = EyeTrackMode::AUTO;
    float mouseX_ = 0.0f, mouseY_ = 0.0f;
    bool hasMouse_ = false;
    float camFocusX_ = 0.0f, camFocusY_ = 0.0f;

    // Shape Morphing
    BotShapeId currentShape_ = BotShapeId::CIRCLE;
    BotShapeId previousShape_ = BotShapeId::CIRCLE;
    float shapeMorphTime_ = 1.0f; // 0..1 (1 = fully completed)
    static constexpr float SHAPE_MORPH_DURATION = 0.45f; // seconds

    // Gaze & Eye State
    HeadGaze currentGaze_{ 28.49f, 28.62f, -13.0f }; // Signature measured rest gaze
    HeadGaze targetGaze_{ 28.49f, 28.62f, -13.0f };
    float blinkTimer_ = 0.0f;
    float blinkValue_ = 0.0f; // 0 = open, 1 = closed

    // Time & Tick Tracking
    int lastTick_ = -1;
    float time_ = 0.0f;
    float dt_ = 0.0167f;

    // Styling Palette (dynamically blended by emotional drive)
    Gdiplus::Color bodyColor_{ 255, 10, 10, 12 };       // Current body color (starts ink black #0a0a0c)
    Gdiplus::Color eyeColor_{ 255, 255, 255, 255 };      // Crisp pure white eyes
    Gdiplus::Color glowColor_{ 40, 255, 255, 255 };      // Subtle outer atmospheric luminescence

    // ── Emotional Drive State ──
    EmotionalDrive emotionalDrive_;
    float targetBodyR_ = 10.0f, targetBodyG_ = 10.0f, targetBodyB_ = 12.0f;
    float currentBodyR_ = 10.0f, currentBodyG_ = 10.0f, currentBodyB_ = 12.0f;
    float emotionEyeOpenModifier_ = 0.0f;   // Added to base eye openness
    float emotionBlinkRateModifier_ = 0.0f;  // Multiplier adjustment on blink interval
    float emotionBreathModifier_ = 1.0f;     // Scales breathing amplitude
    float emotionGlowIntensity_ = 40.0f;     // Alpha channel of glow pen
    void applyEmotionalExpression(const std::string& state);
};
