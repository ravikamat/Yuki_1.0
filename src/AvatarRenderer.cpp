#include "AvatarRenderer.h"
#include "brain/core/RandomEngine.h"
#include <algorithm>
#include <cmath>
#include <random>

#pragma comment(lib, "gdiplus.lib")

static constexpr float PI = 3.14159265358979323846f;
static constexpr float TWO_PI = 6.28318530717958647692f;
static constexpr float DEG_TO_RAD = PI / 180.0f;
static constexpr float RAD_TO_DEG = 180.0f / PI;

// ═══════════════════════════════════════════════════════════════
//  CONSTRUCTOR & PUBLIC API
// ═══════════════════════════════════════════════════════════════

AvatarRenderer::AvatarRenderer() {
    initialize();
}

AvatarRenderer::~AvatarRenderer() {}

void AvatarRenderer::initialize() {
    currentShape_ = BotShapeId::CIRCLE;
    previousShape_ = BotShapeId::CIRCLE;
    shapeMorphTime_ = 1.0f;
    currentGaze_ = { 28.49f, 28.62f, -13.0f }; // Reference rest gaze
    targetGaze_ = currentGaze_;
    blinkTimer_ = 0.0f;
    blinkValue_ = 0.0f;
    time_ = 0.0f;
    lastTick_ = -1;
}

void AvatarRenderer::setEyeTrackMode(EyeTrackMode mode) {
    eyeMode_ = mode;
}

void AvatarRenderer::setMousePos(float mx, float my) {
    mouseX_ = mx;
    mouseY_ = my;
    hasMouse_ = true;
}

void AvatarRenderer::setCameraFocus(float nx, float ny) {
    camFocusX_ = nx;
    camFocusY_ = ny;
}

void AvatarRenderer::setShape(BotShapeId shape) {
    if (shape != currentShape_) {
        previousShape_ = currentShape_;
        currentShape_ = shape;
        shapeMorphTime_ = 0.0f;
    }
}

void AvatarRenderer::nextShape() {
    int nextId = (static_cast<int>(currentShape_) + 1) % 12;
    setShape(static_cast<BotShapeId>(nextId));
}

void AvatarRenderer::setEmotionalDrive(const EmotionalDrive& drive) {
    emotionalDrive_ = drive;
}

const char* AvatarRenderer::getShapeName(BotShapeId shape) const {
    switch (shape) {
        case BotShapeId::CIRCLE:   return "Circle (Default)";
        case BotShapeId::PEBBLE:   return "Pebble";
        case BotShapeId::SQUIRCLE: return "Squircle";
        case BotShapeId::CAPSULE:  return "Capsule";
        case BotShapeId::TRIANGLE: return "Triangle";
        case BotShapeId::HEXAGON:  return "Hexagon";
        case BotShapeId::CLOUD:    return "Cloud";
        case BotShapeId::TEARDROP: return "Teardrop";
        case BotShapeId::EGG:      return "Egg";
        case BotShapeId::ALERT:    return "Alert (!)";
        case BotShapeId::ORBIT:    return "Orbit";
        case BotShapeId::SWIRL:    return "Swirl";
        default:                   return "Unknown";
    }
}

// ═══════════════════════════════════════════════════════════════
//  EASING & MATH HELPERS
// ═══════════════════════════════════════════════════════════════

float AvatarRenderer::easeOutQuint(float t) {
    t = clamp(t, 0.0f, 1.0f);
    float inv = 1.0f - t;
    return 1.0f - inv * inv * inv * inv * inv;
}

float AvatarRenderer::easeOutCubic(float t) {
    t = clamp(t, 0.0f, 1.0f);
    float inv = 1.0f - t;
    return 1.0f - inv * inv * inv;
}

float AvatarRenderer::lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

float AvatarRenderer::clamp(float v, float lo, float hi) {
    return (v < lo) ? lo : (v > hi) ? hi : v;
}

// ═══════════════════════════════════════════════════════════════
//  RADIAL PROFILE SHAPE GENERATION (64 ANGLES)
// ═══════════════════════════════════════════════════════════════

AvatarRenderer::RadialProfile AvatarRenderer::getProfileForShape(BotShapeId shape) {
    RadialProfile prof;
    for (int i = 0; i < PROFILE_SAMPLES; i++) {
        float theta = (static_cast<float>(i) / PROFILE_SAMPLES) * TWO_PI;
        float r = 1.0f;

        switch (shape) {
            case BotShapeId::CIRCLE:
                r = 1.0f;
                break;

            case BotShapeId::PEBBLE:
                // Pebble: circular base with smooth 2nd & 3rd harmonic ripples
                r = 1.0f + 0.075f * cosf(2.0f * theta + 0.5f) + 0.035f * cosf(3.0f * theta + 2.1f);
                break;

            case BotShapeId::SQUIRCLE: {
                // Superellipse (|cos|^4 + |sin|^4)^(-1/4)
                float c = cosf(theta);
                float s = sinf(theta);
                float c4 = c * c * c * c;
                float s4 = s * s * s * s;
                float val = c4 + s4;
                r = (val > 1e-5f) ? powf(val, -0.25f) * 0.94f : 1.0f;
                break;
            }

            case BotShapeId::CAPSULE: {
                // Pill / Capsule shape
                float c = cosf(theta);
                float s = sinf(theta);
                float hw = 0.82f, hh = 1.15f;
                float dx = fabsf(c) / hw;
                float dy = fabsf(s) / hh;
                r = 1.0f / sqrtf(dx * dx + dy * dy);
                break;
            }

            case BotShapeId::TRIANGLE: {
                // Smooth rounded regular triangle
                float rotTheta = theta + PI * 0.5f;
                float tri = cosf(rotTheta) - 0.28f * cosf(2.0f * rotTheta) + 0.08f * cosf(4.0f * rotTheta);
                r = 0.85f + 0.28f * tri;
                break;
            }

            case BotShapeId::HEXAGON: {
                // Rounded hexagon (6-fold symmetry)
                r = 0.94f + 0.09f * cosf(6.0f * theta);
                break;
            }

            case BotShapeId::CLOUD: {
                // Organic 5-lobe cloud profile
                r = 0.92f + 0.12f * cosf(3.0f * theta) + 0.08f * sinf(5.0f * theta - 0.4f);
                break;
            }

            case BotShapeId::TEARDROP: {
                // Upward pointed droplet
                float s = sinf(theta - PI * 0.5f);
                r = 0.92f * (1.0f - 0.32f * s);
                break;
            }

            case BotShapeId::EGG: {
                // Asymmetric vertical oval (measured reference image 164)
                float s = sinf(theta - PI * 0.5f);
                r = 0.90f + 0.15f * s + 0.04f * cosf(2.0f * theta);
                break;
            }

            case BotShapeId::ALERT: {
                // Exclamation mark tapered pill body
                float s = sinf(theta - PI * 0.5f);
                r = 0.95f + 0.22f * s;
                break;
            }

            case BotShapeId::ORBIT: {
                // Elliptical inclined trajectory
                float c = cosf(theta - 0.45f);
                float s = sinf(theta - 0.45f);
                r = sqrtf((c * 1.25f) * (c * 1.25f) + (s * 0.78f) * (s * 0.78f));
                break;
            }

            case BotShapeId::SWIRL: {
                // Spiral vortex
                r = 0.92f + 0.14f * sinf(2.0f * theta + time_ * 2.0f);
                break;
            }
        }

        prof.radii[i] = r;
    }
    return prof;
}

AvatarRenderer::RadialProfile AvatarRenderer::blendProfiles(const RadialProfile& a, const RadialProfile& b, float t) {
    RadialProfile out;
    for (int i = 0; i < PROFILE_SAMPLES; i++) {
        out.radii[i] = lerp(a.radii[i], b.radii[i], t);
    }
    return out;
}

void AvatarRenderer::computeShapePoints(const RadialProfile& prof, float radius, float cx, float cy,
                                        float rot, float sx, float sy, std::vector<Point2D>& outPts) {
    outPts.resize(PROFILE_SAMPLES);
    float cosRot = cosf(rot);
    float sinRot = sinf(rot);

    for (int i = 0; i < PROFILE_SAMPLES; i++) {
        float angle = (static_cast<float>(i) / PROFILE_SAMPLES) * TWO_PI;
        float r = prof.radii[i] * radius;
        float localX = r * cosf(angle);
        float localY = r * sinf(angle);

        // Screen rotation & scaling
        float rx = (localX * cosRot - localY * sinRot) * sx;
        float ry = (localX * sinRot + localY * cosRot) * sy;

        outPts[i].x = cx + rx;
        outPts[i].y = cy + ry;
    }
}

void AvatarRenderer::buildCatmullRomPath(const std::vector<Point2D>& pts, Gdiplus::GraphicsPath& outPath, float tension) {
    int n = static_cast<int>(pts.size());
    if (n < 3) return;

    outPath.Reset();
    outPath.StartFigure();

    for (int i = 0; i < n; i++) {
        const Point2D& p0 = pts[(i - 1 + n) % n];
        const Point2D& p1 = pts[i];
        const Point2D& p2 = pts[(i + 1) % n];
        const Point2D& p3 = pts[(i + 2) % n];

        float c1x = p1.x + (p2.x - p0.x) * tension;
        float c1y = p1.y + (p2.y - p0.y) * tension;
        float c2x = p2.x - (p3.x - p1.x) * tension;
        float c2y = p2.y - (p3.y - p1.y) * tension;

        outPath.AddBezier(p1.x, p1.y, c1x, c1y, c2x, c2y, p2.x, p2.y);
    }
    outPath.CloseFigure();
}

// ═══════════════════════════════════════════════════════════════
//  3D ORTHOGRAPHIC SPHERE EYE PROJECTION
// ═══════════════════════════════════════════════════════════════

static void spinVectorPair(AvatarRenderer::Vec3& u, AvatarRenderer::Vec3& v, float angleRad) {
    float c = cosf(angleRad);
    float s = sinf(angleRad);
    AvatarRenderer::Vec3 newU = { u.x * c + v.x * s, u.y * c + v.y * s, u.z * c + v.z * s };
    AvatarRenderer::Vec3 newV = { v.x * c - u.x * s, v.y * c - u.y * s, v.z * c - u.z * s };
    u = newU;
    v = newV;
}

void AvatarRenderer::computeGazeAngles(float mx, float my, float& outYaw, float& outPitch) {
    // If mouse tracking active, point eyes directly at the cursor
    if (hasMouse_) {
        float depth = 160.0f; // Distance from sphere surface to virtual cursor plane
        outYaw = atan2f(mx, depth) * RAD_TO_DEG;
        outPitch = -atan2f(my, depth) * RAD_TO_DEG;
        // Clamp to natural eye movement range on sphere
        outYaw = clamp(outYaw, -55.0f, 55.0f);
        outPitch = clamp(outPitch, -45.0f, 45.0f);
    } else {
        // Idle living saccades
        float wander = sinf(time_ * 0.8f) * 12.0f;
        outYaw = 28.49f + wander;
        outPitch = 28.62f + cosf(time_ * 0.6f) * 8.0f;
    }
}

std::pair<AvatarRenderer::EyePose, AvatarRenderer::EyePose>
AvatarRenderer::computeEyePoses(const HeadGaze& gaze, float sphereRadius, float splitDegrees) {
    Vec3 f = { 0.0f, 0.0f, 1.0f };
    Vec3 right = { 1.0f, 0.0f, 0.0f };
    Vec3 down = { 0.0f, 1.0f, 0.0f };

    // 1. Yaw rotation (forward spins towards right)
    spinVectorPair(f, right, gaze.yaw * DEG_TO_RAD);
    // 2. Pitch rotation (forward spins upwards)
    spinVectorPair(down, f, -gaze.pitch * DEG_TO_RAD);
    // 3. Roll rotation (right spins towards down)
    spinVectorPair(right, down, gaze.roll * DEG_TO_RAD);

    auto makeEye = [&](float splitAngle) -> EyePose {
        Vec3 eyeF = f;
        Vec3 eyeR = right;
        spinVectorPair(eyeF, eyeR, splitAngle * DEG_TO_RAD);

        EyePose p;
        p.x = eyeF.x * sphereRadius;
        p.y = eyeF.y * sphereRadius;
        p.depth = eyeF.z;

        // Tangent 2x2 matrix: columns are projected right and down vectors
        p.a = eyeR.x;
        p.b = eyeR.y;
        p.c = down.x;
        p.d = down.y;
        return p;
    };

    // Inner eye (0) and outer eye (1)
    EyePose eye0 = makeEye(-splitDegrees);
    EyePose eye1 = makeEye(+splitDegrees);
    return { eye0, eye1 };
}

void AvatarRenderer::drawCapsuleEye(Gdiplus::Graphics& g, float cx, float cy, const EyePose& eye,
                                    float eyeW, float eyeH, float openScale, const Gdiplus::Color& color) {
    if (eye.depth <= 0.01f || openScale <= 0.01f) return;

    Gdiplus::GraphicsState state = g.Save();

    // 2D Affine transform matching SVG matrix(a,b,c,d,x,y)
    Gdiplus::Matrix mat(eye.a, eye.b, eye.c, eye.d, cx + eye.x, cy + eye.y);
    g.SetTransform(&mat);

    float h = eyeH * openScale;
    float w = eyeW;
    float r = w * 0.5f;

    Gdiplus::GraphicsPath cap;
    if (h > w) {
        // Vertical capsule: top arc, bottom arc, connected by lines
        float topArcY = -h * 0.5f;
        float botArcY = h * 0.5f - w;
        cap.AddArc(-r, topArcY, w, w, 180.0f, 180.0f);
        cap.AddArc(-r, botArcY, w, w, 0.0f, 180.0f);
        cap.CloseFigure();
    } else {
        // Squashed / blinking pill
        cap.AddEllipse(-r, -h * 0.5f, w, h);
    }

    Gdiplus::SolidBrush brush(color);
    g.FillPath(&brush, &cap);

    g.Restore(state);
}

// ═══════════════════════════════════════════════════════════════
//  EMOTIONAL EXPRESSION ENGINE
//  Maps Yuki's brain emotions → visual body parameters
// ═══════════════════════════════════════════════════════════════

void AvatarRenderer::applyEmotionalExpression(const std::string& state) {
    // ── 1. Determine target body color from state + emotion ──
    // Base: ink black #0a0a0c (R=10, G=10, B=12)
    float tR = 10.0f, tG = 10.0f, tB = 12.0f;

    // State-driven base tint
    if (state == "LISTENING") {
        tR = 12.0f; tG = 14.0f; tB = 26.0f;  // Dark blue tint #0c0e1a
    } else if (state == "THINKING") {
        tR = 18.0f; tG = 8.0f; tB = 26.0f;   // Deep purple tint #12081a
    } else if (state == "SPEAKING") {
        tR = 26.0f; tG = 14.0f; tB = 8.0f;   // Warm dark #1a0e08
    }

    // Emotion overrides (blend on top of state color based on intensity)
    float v = emotionalDrive_.valence;
    float a = emotionalDrive_.arousal;
    float c = emotionalDrive_.curiosity;
    float f = emotionalDrive_.fatigue;
    int emo = emotionalDrive_.dominantEmotion;

    // Emotion enum values from EmbodiedInteroceptiveSelf.h:
    // 0=NEUTRAL, 7=JOY, 8=SADNESS, 10=CURIOSITY, 11=FRUSTRATION, 12=SATISFACTION
    float emotionStrength = clamp(a + fabsf(v) * 0.5f, 0.0f, 1.0f);

    if (emo == 7 && v > 0.3f) {
        // JOY: warm gold tint #1a1408
        float blend = clamp((v - 0.3f) / 0.7f, 0.0f, 1.0f) * emotionStrength;
        tR = lerp(tR, 26.0f, blend);
        tG = lerp(tG, 20.0f, blend);
        tB = lerp(tB, 8.0f, blend);
    } else if (emo == 10 && c > 0.5f) {
        // CURIOSITY: teal tint #081a16
        float blend = clamp((c - 0.5f) / 0.5f, 0.0f, 1.0f) * emotionStrength;
        tR = lerp(tR, 8.0f, blend);
        tG = lerp(tG, 26.0f, blend);
        tB = lerp(tB, 22.0f, blend);
    } else if (emo == 8 && v < -0.3f) {
        // SADNESS: cool blue #080e1a
        float blend = clamp((-v - 0.3f) / 0.7f, 0.0f, 1.0f) * emotionStrength;
        tR = lerp(tR, 8.0f, blend);
        tG = lerp(tG, 14.0f, blend);
        tB = lerp(tB, 26.0f, blend);
    } else if (emo == 11 || emo == 6) {
        // FRUSTRATION / ANGER: red tint #1a0808
        float blend = emotionStrength * 0.7f;
        tR = lerp(tR, 26.0f, blend);
        tG = lerp(tG, 8.0f, blend);
        tB = lerp(tB, 8.0f, blend);
    } else if (emo == 12 && v > 0.2f) {
        // SATISFACTION: soft warm #181410
        float blend = clamp(v * 0.6f, 0.0f, 1.0f);
        tR = lerp(tR, 24.0f, blend);
        tG = lerp(tG, 20.0f, blend);
        tB = lerp(tB, 16.0f, blend);
    }

    // Fatigue desaturates toward grey
    if (f > 0.5f) {
        float grey = lerp(0.0f, 1.0f, clamp((f - 0.5f) / 0.5f, 0.0f, 1.0f));
        float avg = (tR + tG + tB) / 3.0f;
        tR = lerp(tR, avg + 4.0f, grey);
        tG = lerp(tG, avg + 4.0f, grey);
        tB = lerp(tB, avg + 4.0f, grey);
    }

    targetBodyR_ = tR;
    targetBodyG_ = tG;
    targetBodyB_ = tB;

    // ── 2. Smooth exponential interpolation of body color ──
    float colorSpeed = 6.0f * dt_;
    currentBodyR_ = lerp(currentBodyR_, targetBodyR_, colorSpeed);
    currentBodyG_ = lerp(currentBodyG_, targetBodyG_, colorSpeed);
    currentBodyB_ = lerp(currentBodyB_, targetBodyB_, colorSpeed);

    bodyColor_ = Gdiplus::Color(255,
        static_cast<BYTE>(clamp(currentBodyR_, 0.0f, 255.0f)),
        static_cast<BYTE>(clamp(currentBodyG_, 0.0f, 255.0f)),
        static_cast<BYTE>(clamp(currentBodyB_, 0.0f, 255.0f)));

    // ── 3. Glow intensity from arousal ──
    // Low arousal = subtle glow (30), high arousal = brighter glow (80)
    float targetGlow = lerp(30.0f, 80.0f, clamp(a, 0.0f, 1.0f));
    emotionGlowIntensity_ = lerp(emotionGlowIntensity_, targetGlow, colorSpeed);
    glowColor_ = Gdiplus::Color(static_cast<BYTE>(clamp(emotionGlowIntensity_, 10.0f, 120.0f)), 255, 255, 255);

    // ── 4. Eye openness modifier ──
    // Curiosity → wider eyes (+0.15), fatigue → droopier eyes (-0.25)
    float targetEyeMod = 0.0f;
    if (c > 0.6f) targetEyeMod += (c - 0.6f) * 0.375f;    // Up to +0.15
    if (f > 0.4f) targetEyeMod -= (f - 0.4f) * 0.417f;    // Down to -0.25
    if (emo == 7) targetEyeMod += 0.08f;  // JOY: slightly wider
    if (emo == 8) targetEyeMod -= 0.1f;   // SADNESS: droopier
    emotionEyeOpenModifier_ = lerp(emotionEyeOpenModifier_, targetEyeMod, colorSpeed);

    // ── 5. Blink rate modifier ──
    // High arousal → less blinking (longer interval), fatigue → more blinking (shorter)
    float targetBlinkMod = 0.0f;
    if (a > 0.5f) targetBlinkMod += (a - 0.5f) * 1.0f;    // Up to +0.5 (longer interval)
    if (f > 0.5f) targetBlinkMod -= (f - 0.5f) * 1.6f;    // Down to -0.8 (shorter interval)
    emotionBlinkRateModifier_ = lerp(emotionBlinkRateModifier_, targetBlinkMod, colorSpeed);

    // ── 6. Breathing modifier ──
    // High arousal → faster/larger breathing, fatigue → slower
    float targetBreath = 1.0f;
    targetBreath += a * 0.4f;      // Arousal amplifies breathing
    targetBreath -= f * 0.3f;      // Fatigue dampens breathing
    targetBreath = clamp(targetBreath, 0.5f, 1.6f);
    emotionBreathModifier_ = lerp(emotionBreathModifier_, targetBreath, colorSpeed);
}

// ═══════════════════════════════════════════════════════════════
//  MAIN RENDER PIPELINE
// ═══════════════════════════════════════════════════════════════

void AvatarRenderer::render(Gdiplus::Graphics& g, float cx, float cy,
                            const std::string& state, int tick,
                            float speakingBounce, float breathOffset,
                            const std::string& speechBubble) {
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
    g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);

    // ── Time & Delta Update ──
    if (lastTick_ < 0) dt_ = 0.0167f;
    else {
        int d = tick - lastTick_;
        if (d < 1) d = 1; if (d > 6) d = 6;
        dt_ = d * 0.0167f;
    }
    lastTick_ = tick;
    time_ += dt_;

    // ── Apply Emotional Expression (updates colors, glow, eye/blink/breath modifiers) ──
    applyEmotionalExpression(state);

    // ── Morph Progression ──
    if (shapeMorphTime_ < 1.0f) {
        shapeMorphTime_ += dt_ / SHAPE_MORPH_DURATION;
        if (shapeMorphTime_ > 1.0f) shapeMorphTime_ = 1.0f;
    }
    float morphK = easeOutQuint(shapeMorphTime_);

    // ── Gaze Tracking & Saccades ──
    float targetYaw = 0.0f, targetPitch = 0.0f;
    computeGazeAngles(mouseX_, mouseY_, targetYaw, targetPitch);

    // Natural exponential catchup (lookMorph ~ 0.24s)
    float gazeFollowSpeed = 12.0f * dt_;
    currentGaze_.yaw = lerp(currentGaze_.yaw, targetYaw, gazeFollowSpeed);
    currentGaze_.pitch = lerp(currentGaze_.pitch, targetPitch, gazeFollowSpeed);
    currentGaze_.roll = lerp(currentGaze_.roll, -targetYaw * 0.25f, gazeFollowSpeed);

    // ── Blink Generator (emotionally modulated) ──
    blinkTimer_ += dt_;
    float blinkInterval = 3.8f + emotionBlinkRateModifier_;
    blinkInterval = clamp(blinkInterval, 1.5f, 6.0f);  // Bounds: 1.5s to 6s between blinks
    if (blinkTimer_ > blinkInterval) {
        // Quick 120ms organic blink
        float bTime = blinkTimer_ - blinkInterval;
        if (bTime < 0.12f) {
            blinkValue_ = sinf((bTime / 0.12f) * PI);
        } else {
            blinkValue_ = 0.0f;
            std::uniform_real_distribution<float> jitter(0.0f, 1.5f);
            blinkTimer_ = jitter(yuki::core::getRandomEngine()); // Randomize next interval
        }
    }

    // ── Base Dimensions (emotionally modulated breathing) ──
    float bodyRadius = 78.0f;
    float breathAmplitude = 0.008f * emotionBreathModifier_;
    float breathSpeed = 1.8f * emotionBreathModifier_;
    float breathe = sinf(time_ * breathSpeed) * breathAmplitude;
    float effectiveRadius = bodyRadius * (1.0f + breathe);

    // ── 1. Render Body Silhouette ──
    RadialProfile profA = getProfileForShape(previousShape_);
    RadialProfile profB = getProfileForShape(currentShape_);
    RadialProfile activeProf = blendProfiles(profA, profB, morphK);

    std::vector<Point2D> shapePoints;
    computeShapePoints(activeProf, effectiveRadius, cx, cy, 0.0f, 1.0f, 1.0f, shapePoints);

    Gdiplus::GraphicsPath bodyPath;
    buildCatmullRomPath(shapePoints, bodyPath, 1.0f / 6.0f);

    // Subtle atmospheric outer glow (intensity driven by arousal)
    Gdiplus::Pen glowPen(glowColor_, 3.0f);
    g.DrawPath(&glowPen, &bodyPath);

    // Emotionally tinted filled silhouette
    Gdiplus::SolidBrush bodyBrush(bodyColor_);
    g.FillPath(&bodyBrush, &bodyPath);

    // ── 2. Render 3D Orthographic Projected Eyes ──
    float eyeSphereRadius = effectiveRadius * 0.95f;
    auto [eyeLeft, eyeRight] = computeEyePoses(currentGaze_, eyeSphereRadius, 15.46f);

    float eyeWidth = 0.186f * effectiveRadius * 1.90f;
    float eyeHeight = 0.412f * effectiveRadius * 1.85f;
    float eyeOpen = clamp(1.0f - blinkValue_ + emotionEyeOpenModifier_, 0.06f, 1.0f);

    // Speech excitement / bounce dilation
    if (state == "SPEAKING") {
        eyeOpen *= (1.0f + sinf(time_ * 12.0f) * 0.08f);
    }

    drawCapsuleEye(g, cx, cy, eyeLeft, eyeWidth, eyeHeight, eyeOpen, eyeColor_);
    drawCapsuleEye(g, cx, cy, eyeRight, eyeWidth, eyeHeight, eyeOpen, eyeColor_);
}
