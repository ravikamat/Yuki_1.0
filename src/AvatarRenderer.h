#pragma once
#include <windows.h>
#include <gdiplus.h>
#include <string>
#include <vector>
#include <cmath>

class AvatarRenderer {
public:
    enum class EyeTrackMode { NONE, MOUSE, CAMERA, AUTO };
    enum class Mood {
        NEUTRAL, HAPPY, EXCITED, SAD, WORRIED,
        THINKING, SURPRISED, EMBARRASSED, ANGRY, CONFUSED
    };

    AvatarRenderer();
    ~AvatarRenderer();

    void initialize();

    // Input controls
    void setEyeTrackMode(EyeTrackMode mode);
    void setMousePos(float mx, float my);
    void setCameraFocus(float nx, float ny); // normalized -1..1
    void setMood(Mood mood);

    // External pose injection (for Yuki AI learning system)
    struct PoseTarget {
        float lHandX = 0, lHandY = 0;
        float rHandX = 0, rHandY = 0;
        float headTilt = 0, headTurn = 0;
        float lean = 0;
        bool overrideHands = false;
        bool overrideHead = false;
        bool overrideLean = false;
    };
    void setPoseTarget(const PoseTarget& target);
    void clearPoseTarget();

    // Main render — (cx,cy) is ground point between feet
    void render(Gdiplus::Graphics& graphics, float cx, float cy,
                const std::string& state, int tick,
                float speakingBounce, float breathOffset,
                const std::string& speechBubble);

private:
    // ── Spring-Damper Physics ─────────────────────────────
    struct Spring {
        float current = 0.0f;
        float velocity = 0.0f;
        float target = 0.0f;
        float stiffness = 12.0f;
        float damping = 0.78f;
        void update(float dt);
    };
    struct Spring2D {
        Spring x, y;
        void update(float dt) { x.update(dt); y.update(dt); }
        void setTarget(float tx, float ty) { x.target = tx; y.target = ty; }
        float getX() const { return x.current; }
        float getY() const { return y.current; }
    };

    // ── Animated Channels (all spring-driven) ─────────────
    struct Channels {
        // Body
        Spring bodySwayX, bodySwayY, bodyBreathe, bodyLean;
        // Head
        Spring headTilt, headTurn, headNod, headBob;
        // Arms
        Spring lShoulderAngle, lElbowAngle, lWristAngle;
        Spring rShoulderAngle, rElbowAngle, rWristAngle;
        Spring2D lHandPos, rHandPos;
        // Hair
        Spring hairSwayMain, hairSwayPony, hairSwayBangs, hairSwaySideL, hairSwaySideR;
        // Face
        Spring eyeBlinkL, eyeBlinkR;
        Spring2D eyeGaze;
        Spring browLY, browRY, browLAngle, browRAngle;
        Spring mouthOpen, mouthWidth, mouthShape; // shape: -1 frown, 0 neutral, 1 smile
        Spring blushIntensity;
        Spring earTwitch; // subtle ear movement
    };
    Channels ch_;

    // ── State ─────────────────────────────────────────────
    EyeTrackMode eyeMode_ = EyeTrackMode::AUTO;
    Mood mood_ = Mood::NEUTRAL;
    float mouseX_ = 0, mouseY_ = 0;
    float camFocusX_ = 0, camFocusY_ = 0;
    bool hasMouse_ = false;

    PoseTarget poseTarget_;
    bool hasPoseTarget_ = false;

    std::string prevState_;
    int lastTick_ = -1;
    float time_ = 0.0f;
    float dt_ = 0.0167f;
    float moodBlend_ = 0.0f; // 0=old mood, 1=new mood

    // ── Body Proportions (matching reference ~7.5 heads) ──
    static constexpr float SCALE = 1.0f;
    static constexpr float HEAD_W = 38.0f;
    static constexpr float HEAD_H = 44.0f;
    static constexpr float NECK_H = 14.0f;
    static constexpr float TORSO_H = 72.0f;
    static constexpr float CHEST_W = 58.0f;
    static constexpr float WAIST_W = 34.0f;
    static constexpr float ARM_UPPER = 44.0f;
    static constexpr float ARM_LOWER = 40.0f;
    static constexpr float HAND_W = 10.0f;
    static constexpr float HAND_H = 12.0f;
    static constexpr float LEG_UPPER = 62.0f;
    static constexpr float LEG_LOWER = 60.0f;
    static constexpr float FOOT_W = 18.0f;
    static constexpr float FOOT_H = 9.0f;
    static constexpr float SKIRT_H = 32.0f;
    static constexpr float SOCK_H = 14.0f;

    // ── Color Palette (matching reference) ────────────────
    struct Palette {
        // Hair — soft pink
        Gdiplus::Color hairBase, hairMid, hairDark, hairHighlight, hairShine;
        // Eyes — deep pink/magenta
        Gdiplus::Color irisOuter, irisMid, irisInner, irisSparkle;
        // Skin — pale peach
        Gdiplus::Color skinBase, skinMid, skinShadow, skinDark;
        // Blush
        Gdiplus::Color blush;
        // Uniform — light pink
        Gdiplus::Color uniformBase, uniformMid, uniformDark, uniformShadow;
        // Bow — deep red
        Gdiplus::Color bowBase, bowDark, bowHighlight;
        // Skirt
        Gdiplus::Color skirtBase, skirtDark;
        // Socks & Shoes
        Gdiplus::Color sockBase, shoeBase, shoeDark;
        // Outlines
        Gdiplus::Color outline, outlineLight;
    };
    Palette pal_;

    void initPalette();

    // ── Physics & Animation ───────────────────────────────
    void updatePhysics(float dt);
    void computeTargets(const std::string& state, int tick,
                        float breathOffset, const std::string& speech);
    void computeEyeTracking(float& outPx, float& outPy);
    void computeLipSync(const std::string& speech, int tick,
                        float& outOpen, float& outWidth, float& outShape);
    void computeMoodExpression(float& browY, float& browA, float& mouthS,
                                float& blush, float& eyeWide);

    // ── Render Layers (back to front) ─────────────────────
    void drawShadow(Gdiplus::Graphics& g, float cx, float cy);
    void drawHairBack(Gdiplus::Graphics& g, float hx, float hy);
    void drawLegBack(Gdiplus::Graphics& g, float hipX, float hipY, float kneeX, float kneeY,
                      float ankleX, float ankleY, float footX, float footY);
    void drawLegFront(Gdiplus::Graphics& g, float hipX, float hipY, float kneeX, float kneeY,
                       float ankleX, float ankleY, float footX, float footY);
    void drawSkirt(Gdiplus::Graphics& g, float wx, float wy);
    void drawTorso(Gdiplus::Graphics& g, float cx, float cy, float wx, float wy);
    void drawArmBack(Gdiplus::Graphics& g, float sx, float sy, float ex, float ey, float hx, float hy);
    void drawArmFront(Gdiplus::Graphics& g, float sx, float sy, float ex, float ey, float hx, float hy);
    void drawHandClasped(Gdiplus::Graphics& g, float x, float y);
    void drawHandOpen(Gdiplus::Graphics& g, float x, float y, float angle, bool left);
    void drawNeck(Gdiplus::Graphics& g, float nx, float ny);
    void drawHead(Gdiplus::Graphics& g, float hx, float hy);
    void drawFace(Gdiplus::Graphics& g, float hx, float hy,
                  const std::string& state, int tick, const std::string& speech);
    void drawHairFront(Gdiplus::Graphics& g, float hx, float hy);
    void drawPonytail(Gdiplus::Graphics& g, float hx, float hy);
    void drawRibbon(Gdiplus::Graphics& g, float x, float y);

    // ── Limb Segment (tapered quad oriented along joint-to-joint) ──
    void drawLimbSegment(Gdiplus::Graphics& g, float x1, float y1, float x2, float y2,
                         float w1, float w2,
                         const Gdiplus::Color& c1, const Gdiplus::Color& c2);

    // ── Helpers ───────────────────────────────────────────
    void fillGradientPath(Gdiplus::Graphics& g, const Gdiplus::GraphicsPath& path,
                          const Gdiplus::Color& center, const Gdiplus::Color& edge);
    void fillLinearPath(Gdiplus::Graphics& g, const Gdiplus::GraphicsPath& path,
                        float x1, float y1, float x2, float y2,
                        const Gdiplus::Color& c1, const Gdiplus::Color& c2);
    Gdiplus::Color lerpColor(const Gdiplus::Color& a, const Gdiplus::Color& b, float t);
    Gdiplus::Color darken(const Gdiplus::Color& c, float factor);
    Gdiplus::Color lighten(const Gdiplus::Color& c, float factor);
};
