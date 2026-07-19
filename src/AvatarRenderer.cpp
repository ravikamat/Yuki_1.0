#include "AvatarRenderer.h"
#include <algorithm>

#pragma comment(lib, "gdiplus.lib")

// ═══════════════════════════════════════════════════════════════
//  SPRING PHYSICS
// ═══════════════════════════════════════════════════════════════

void AvatarRenderer::Spring::update(float dt) {
    float err = target - current;
    velocity += err * stiffness * dt;
    velocity *= damping;
    current += velocity * dt;
}

// ═══════════════════════════════════════════════════════════════
//  CONSTRUCTOR / INIT
// ═══════════════════════════════════════════════════════════════

AvatarRenderer::AvatarRenderer() {
    initPalette();

    // Configure spring stiffness per channel
    ch_.eyeBlinkL.stiffness = 30.0f; ch_.eyeBlinkL.damping = 0.55f;
    ch_.eyeBlinkR.stiffness = 30.0f; ch_.eyeBlinkR.damping = 0.55f;
    ch_.eyeGaze.x.stiffness = 18.0f; ch_.eyeGaze.x.damping = 0.72f;
    ch_.eyeGaze.y.stiffness = 18.0f; ch_.eyeGaze.y.damping = 0.72f;
    ch_.hairSwayPony.stiffness = 3.0f; ch_.hairSwayPony.damping = 0.65f;
    ch_.hairSwayMain.stiffness = 5.0f; ch_.hairSwayMain.damping = 0.70f;
    ch_.hairSwayBangs.stiffness = 8.0f; ch_.hairSwayBangs.damping = 0.75f;
    ch_.hairSwaySideL.stiffness = 4.0f; ch_.hairSwaySideL.damping = 0.68f;
    ch_.hairSwaySideR.stiffness = 4.0f; ch_.hairSwaySideR.damping = 0.68f;
    ch_.mouthOpen.stiffness = 22.0f; ch_.mouthOpen.damping = 0.60f;
    ch_.mouthWidth.stiffness = 14.0f; ch_.mouthWidth.damping = 0.70f;
    ch_.mouthShape.stiffness = 10.0f; ch_.mouthShape.damping = 0.80f;
    ch_.blushIntensity.stiffness = 6.0f; ch_.blushIntensity.damping = 0.85f;
    ch_.earTwitch.stiffness = 15.0f; ch_.earTwitch.damping = 0.60f;

    // Body channels
    ch_.bodySwayX.stiffness = 6.0f; ch_.bodySwayX.damping = 0.82f;
    ch_.bodySwayY.stiffness = 8.0f; ch_.bodySwayY.damping = 0.80f;
    ch_.bodyBreathe.stiffness = 8.0f; ch_.bodyBreathe.damping = 0.78f;
    ch_.bodyLean.stiffness = 5.0f; ch_.bodyLean.damping = 0.85f;

    // Head channels
    ch_.headTilt.stiffness = 10.0f; ch_.headTilt.damping = 0.75f;
    ch_.headTurn.stiffness = 10.0f; ch_.headTurn.damping = 0.75f;
    ch_.headNod.stiffness = 12.0f; ch_.headNod.damping = 0.72f;
    ch_.headBob.stiffness = 14.0f; ch_.headBob.damping = 0.70f;

    // Arm channels
    ch_.lShoulderAngle.stiffness = 8.0f; ch_.lShoulderAngle.damping = 0.80f;
    ch_.lElbowAngle.stiffness = 10.0f; ch_.lElbowAngle.damping = 0.78f;
    ch_.rShoulderAngle.stiffness = 8.0f; ch_.rShoulderAngle.damping = 0.80f;
    ch_.rElbowAngle.stiffness = 10.0f; ch_.rElbowAngle.damping = 0.78f;

    // Brow channels
    ch_.browLY.stiffness = 12.0f; ch_.browLY.damping = 0.72f;
    ch_.browRY.stiffness = 12.0f; ch_.browRY.damping = 0.72f;
    ch_.browLAngle.stiffness = 10.0f; ch_.browLAngle.damping = 0.75f;
    ch_.browRAngle.stiffness = 10.0f; ch_.browRAngle.damping = 0.75f;
}

AvatarRenderer::~AvatarRenderer() {}

void AvatarRenderer::initialize() {}

void AvatarRenderer::initPalette() {
    // Hair — soft sakura pink
    pal_.hairBase      = Gdiplus::Color(255, 245, 200, 210);
    pal_.hairMid       = Gdiplus::Color(255, 235, 175, 190);
    pal_.hairDark      = Gdiplus::Color(255, 210, 145, 165);
    pal_.hairHighlight = Gdiplus::Color(255, 255, 225, 232);
    pal_.hairShine     = Gdiplus::Color(255, 255, 240, 245);

    // Eyes — deep rose/magenta
    pal_.irisOuter  = Gdiplus::Color(255, 160, 50, 85);
    pal_.irisMid    = Gdiplus::Color(255, 200, 90, 130);
    pal_.irisInner  = Gdiplus::Color(255, 235, 150, 180);
    pal_.irisSparkle= Gdiplus::Color(255, 255, 220, 235);

    // Skin — porcelain peach
    pal_.skinBase   = Gdiplus::Color(255, 255, 245, 240);
    pal_.skinMid    = Gdiplus::Color(255, 252, 235, 228);
    pal_.skinShadow = Gdiplus::Color(255, 240, 215, 205);
    pal_.skinDark   = Gdiplus::Color(255, 225, 195, 185);

    // Blush
    pal_.blush      = Gdiplus::Color(255, 255, 140, 160);

    // Uniform — soft pink
    pal_.uniformBase  = Gdiplus::Color(255, 252, 215, 228);
    pal_.uniformMid   = Gdiplus::Color(255, 245, 195, 215);
    pal_.uniformDark  = Gdiplus::Color(255, 230, 170, 195);
    pal_.uniformShadow= Gdiplus::Color(255, 215, 155, 180);

    // Bow — deep rose
    pal_.bowBase      = Gdiplus::Color(255, 200, 55, 85);
    pal_.bowDark      = Gdiplus::Color(255, 165, 35, 65);
    pal_.bowHighlight = Gdiplus::Color(255, 235, 100, 130);

    // Skirt
    pal_.skirtBase = pal_.uniformBase;
    pal_.skirtDark = pal_.uniformDark;

    // Socks & shoes
    pal_.sockBase = Gdiplus::Color(255, 255, 220, 230);
    pal_.shoeBase = Gdiplus::Color(255, 245, 245, 250);
    pal_.shoeDark = Gdiplus::Color(255, 220, 220, 230);

    // Outlines — soft, not black
    pal_.outline      = Gdiplus::Color(255, 120, 85, 100);
    pal_.outlineLight = Gdiplus::Color(255, 160, 120, 135);
}

// ═══════════════════════════════════════════════════════════════
//  PUBLIC API
// ═══════════════════════════════════════════════════════════════

void AvatarRenderer::setEyeTrackMode(EyeTrackMode mode) { eyeMode_ = mode; }
void AvatarRenderer::setMousePos(float mx, float my) { mouseX_ = mx; mouseY_ = my; hasMouse_ = true; }
void AvatarRenderer::setCameraFocus(float nx, float ny) { camFocusX_ = nx; camFocusY_ = ny; }

void AvatarRenderer::setMood(Mood mood) {
    if (mood_ != mood) {
        mood_ = mood;
        moodBlend_ = 0.0f;
    }
}

void AvatarRenderer::setPoseTarget(const PoseTarget& target) {
    poseTarget_ = target;
    hasPoseTarget_ = true;
}

void AvatarRenderer::clearPoseTarget() { hasPoseTarget_ = false; }

// ═══════════════════════════════════════════════════════════════
//  MAIN RENDER
// ═══════════════════════════════════════════════════════════════

void AvatarRenderer::render(Gdiplus::Graphics& g, float cx, float cy,
                            const std::string& state, int tick,
                            float speakingBounce, float breathOffset,
                            const std::string& speechBubble) {
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
    g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);

    // Time / dt
    if (lastTick_ < 0) dt_ = 0.0167f;
    else {
        int d = tick - lastTick_;
        if (d < 1) d = 1; if (d > 6) d = 6;
        dt_ = d * 0.0167f;
    }
    lastTick_ = tick;
    time_ += dt_;
    moodBlend_ = (std::min)(1.0f, moodBlend_ + dt_ * 3.0f);

    // Compute targets then update physics
    computeTargets(state, tick, breathOffset, speechBubble);
    updatePhysics(dt_);

    // ── Compute Skeleton ──
    float sway = ch_.bodySwayX.current;
    float breathe = ch_.bodyBreathe.current;
    float lean = ch_.bodyLean.current;

    // Ground → feet
    float rFootX = cx + 8.0f + sway * 0.2f;
    float rFootY = cy;
    float lFootX = cx - 10.0f + sway * 0.35f;
    float lFootY = cy - 2.0f;

    // Feet → knees
    float rKneeX = rFootX + lean * 0.4f;
    float rKneeY = rFootY - LEG_LOWER;
    float lKneeX = lFootX + lean * 0.3f + 3.0f;
    float lKneeY = lFootY - LEG_LOWER + 2.0f;

    // Knees → hips
    float hipX = cx + sway + lean * 0.15f;
    float hipY = cy - LEG_LOWER - LEG_UPPER + breathe * 2.0f;

    // Hips → waist → chest
    float waistX = hipX + sinf(lean * 0.017f) * 6.0f;
    float waistY = hipY - SKIRT_H * 0.4f;
    float chestX = hipX + sinf(lean * 0.017f) * 10.0f;
    float chestY = hipY - TORSO_H + breathe * 1.5f;

    // Shoulders
    float lShoulderX = chestX - CHEST_W * 0.42f;
    float lShoulderY = chestY - 2.0f;
    float rShoulderX = chestX + CHEST_W * 0.42f;
    float rShoulderY = chestY + 1.0f;

    // Neck → Head
    float neckX = chestX + sinf(ch_.headTilt.current * 0.017f) * 2.0f;
    float neckY = chestY - NECK_H;
    float headX = neckX + sinf(ch_.headTilt.current * 0.017f) * 3.0f + ch_.headTurn.current * 0.15f;
    float headY = neckY - HEAD_H * 0.55f + ch_.headBob.current + speakingBounce * 0.25f;

    // Arms (FK)
    float lElbowX = lShoulderX + cosf(ch_.lShoulderAngle.current * 0.017f) * ARM_UPPER;
    float lElbowY = lShoulderY + sinf(ch_.lShoulderAngle.current * 0.017f) * ARM_UPPER;
    float lHandX = lElbowX + cosf((ch_.lShoulderAngle.current + ch_.lElbowAngle.current) * 0.017f) * ARM_LOWER;
    float lHandY = lElbowY + sinf((ch_.lShoulderAngle.current + ch_.lElbowAngle.current) * 0.017f) * ARM_LOWER;

    float rElbowX = rShoulderX + cosf(ch_.rShoulderAngle.current * 0.017f) * ARM_UPPER;
    float rElbowY = rShoulderY + sinf(ch_.rShoulderAngle.current * 0.017f) * ARM_UPPER;
    float rHandX = rElbowX + cosf((ch_.rShoulderAngle.current + ch_.rElbowAngle.current) * 0.017f) * ARM_LOWER;
    float rHandY = rElbowY + sinf((ch_.rShoulderAngle.current + ch_.rElbowAngle.current) * 0.017f) * ARM_LOWER;

    // Blend with pose target if active
    if (hasPoseTarget_ && poseTarget_.overrideHands) {
        lHandX = lHandX * 0.3f + (lShoulderX + poseTarget_.lHandX) * 0.7f;
        lHandY = lHandY * 0.3f + (lShoulderY + poseTarget_.lHandY) * 0.7f;
        rHandX = rHandX * 0.3f + (rShoulderX + poseTarget_.rHandX) * 0.7f;
        rHandY = rHandY * 0.3f + (rShoulderY + poseTarget_.rHandY) * 0.7f;
    }

    // Clasped-hands pose for IDLE/LISTENING
    bool clasped = (state != "SPEAKING" && state != "THINKING");
    if (clasped && !hasPoseTarget_) {
        float claspX = chestX + sway * 0.5f + sinf(time_ * 0.6f) * 1.5f;
        float claspY = chestY + 18.0f + breathe * 1.0f;
        lHandX = claspX - 4.0f; lHandY = claspY;
        rHandX = claspX + 4.0f; rHandY = claspY;
        // Simple IK for elbows
        lElbowX = (lShoulderX + lHandX) * 0.5f - 6.0f;
        lElbowY = (lShoulderY + lHandY) * 0.5f - 4.0f;
        rElbowX = (rShoulderX + rHandX) * 0.5f + 6.0f;
        rElbowY = (rShoulderY + rHandY) * 0.5f - 4.0f;
    }

    // ── LAYERED RENDER (back → front) ──
    Gdiplus::GraphicsState gs = g.Save();

    drawShadow(g, cx, cy);
    drawHairBack(g, headX, headY);
    drawLegBack(g, hipX + 5.0f, hipY, rKneeX, rKneeY, rKneeX, rKneeY + 5.0f, rFootX, rFootY);
    drawLegFront(g, hipX - 5.0f, hipY, lKneeX, lKneeY, lKneeX, lKneeY + 5.0f, lFootX, lFootY);
    drawSkirt(g, waistX, waistY);
    drawTorso(g, chestX, chestY, waistX, waistY);

    if (clasped && !hasPoseTarget_) {
        drawArmBack(g, lShoulderX, lShoulderY, lElbowX, lElbowY, lHandX, lHandY);
        drawArmBack(g, rShoulderX, rShoulderY, rElbowX, rElbowY, rHandX, rHandY);
        drawHandClasped(g, (lHandX + rHandX) * 0.5f, (lHandY + rHandY) * 0.5f);
    } else {
        drawArmBack(g, rShoulderX, rShoulderY, rElbowX, rElbowY, rHandX, rHandY);
        drawHandOpen(g, rHandX, rHandY, ch_.rShoulderAngle.current + ch_.rElbowAngle.current, false);
        drawArmFront(g, lShoulderX, lShoulderY, lElbowX, lElbowY, lHandX, lHandY);
        drawHandOpen(g, lHandX, lHandY, ch_.lShoulderAngle.current + ch_.lElbowAngle.current, true);
    }

    drawNeck(g, neckX, neckY);
    drawHead(g, headX, headY);
    drawFace(g, headX, headY, state, tick, speechBubble);
    drawHairFront(g, headX, headY);
    drawPonytail(g, headX, headY);

    g.Restore(gs);
}

// ═══════════════════════════════════════════════════════════════
//  PHYSICS UPDATE
// ═══════════════════════════════════════════════════════════════

void AvatarRenderer::updatePhysics(float dt) {
    #define UPD(s) s.update(dt)
    UPD(ch_.bodySwayX); UPD(ch_.bodySwayY); UPD(ch_.bodyBreathe); UPD(ch_.bodyLean);
    UPD(ch_.headTilt); UPD(ch_.headTurn); UPD(ch_.headNod); UPD(ch_.headBob);
    UPD(ch_.lShoulderAngle); UPD(ch_.lElbowAngle); UPD(ch_.lWristAngle);
    UPD(ch_.rShoulderAngle); UPD(ch_.rElbowAngle); UPD(ch_.rWristAngle);
    ch_.lHandPos.update(dt); ch_.rHandPos.update(dt);
    UPD(ch_.hairSwayMain); UPD(ch_.hairSwayPony); UPD(ch_.hairSwayBangs);
    UPD(ch_.hairSwaySideL); UPD(ch_.hairSwaySideR);
    UPD(ch_.eyeBlinkL); UPD(ch_.eyeBlinkR);
    ch_.eyeGaze.update(dt);
    UPD(ch_.browLY); UPD(ch_.browRY); UPD(ch_.browLAngle); UPD(ch_.browRAngle);
    UPD(ch_.mouthOpen); UPD(ch_.mouthWidth); UPD(ch_.mouthShape);
    UPD(ch_.blushIntensity); UPD(ch_.earTwitch);
    #undef UPD
}

// ═══════════════════════════════════════════════════════════════
//  TARGET COMPUTATION
// ═══════════════════════════════════════════════════════════════

void AvatarRenderer::computeTargets(const std::string& state, int tick,
                                     float breathOffset, const std::string& speech) {
    float t = time_;
    float breathe = sinf(t * 1.6f) * 0.5f + 0.5f;

    // ── Base Body ──
    ch_.bodySwayX.target = sinf(t * 0.35f) * 1.8f + sinf(t * 0.12f) * 0.8f;
    ch_.bodySwayY.target = sinf(t * 0.28f) * 0.6f;
    ch_.bodyBreathe.target = breathe;
    ch_.bodyLean.target = 0.0f;

    // ── Base Head ──
    ch_.headTilt.target = sinf(t * 0.25f) * 1.0f;
    ch_.headTurn.target = sinf(t * 0.18f) * 1.5f;
    ch_.headNod.target = sinf(t * 0.4f) * 0.5f;
    ch_.headBob.target = breathOffset * 0.5f;

    // ── Base Arms (relaxed at sides) ──
    ch_.lShoulderAngle.target = 8.0f;
    ch_.lElbowAngle.target = -10.0f;
    ch_.rShoulderAngle.target = -8.0f;
    ch_.rElbowAngle.target = 10.0f;

    // ── Base Hair ──
    float wind = sinf(t * 0.45f) * 2.0f + sinf(t * 0.9f) * 1.0f;
    ch_.hairSwayMain.target = wind;
    ch_.hairSwayPony.target = wind * 1.4f + sinf(t * 1.2f) * 2.5f;
    ch_.hairSwayBangs.target = wind * 0.25f;
    ch_.hairSwaySideL.target = wind * 0.6f;
    ch_.hairSwaySideR.target = wind * 0.6f;

    // ── Ear twitch ──
    ch_.earTwitch.target = sinf(t * 0.3f) * 0.5f;

    // ── State Overrides ──
    if (state == "SPEAKING") {
        float nod = sinf(t * 3.2f) * 2.5f;
        ch_.headTilt.target = nod + sinf(t * 0.22f) * 1.5f;
        ch_.headTurn.target = sinf(t * 0.15f) * 2.0f;
        ch_.headNod.target = sinf(t * 2.8f) * 1.5f;

        ch_.lShoulderAngle.target = -15.0f + sinf(t * 1.8f) * 6.0f;
        ch_.lElbowAngle.target = -55.0f + sinf(t * 1.6f) * 8.0f;
        ch_.rShoulderAngle.target = 5.0f;
        ch_.rElbowAngle.target = -5.0f;

        ch_.hairSwayMain.target = wind + sinf(t * 1.5f) * 3.0f;
        ch_.hairSwayPony.target = wind * 1.4f + sinf(t * 1.8f) * 4.0f;

        int bi = 140;
        ch_.eyeBlinkL.target = ((tick % bi) < 4) ? 1.0f : 0.0f;
        ch_.eyeBlinkR.target = ch_.eyeBlinkL.target;

    } else if (state == "THINKING") {
        ch_.headTilt.target = -5.0f + sinf(t * 0.5f) * 1.0f;
        ch_.headTurn.target = -4.0f;
        ch_.bodyLean.target = 3.0f;

        ch_.lShoulderAngle.target = -40.0f;
        ch_.lElbowAngle.target = -85.0f;
        ch_.rShoulderAngle.target = 10.0f;
        ch_.rElbowAngle.target = -15.0f;

        ch_.hairSwayMain.target = wind * 0.5f;
        ch_.hairSwayPony.target = wind * 0.7f;

        int bi = 55;
        ch_.eyeBlinkL.target = ((tick % bi) < 4) ? 1.0f : 0.0f;
        ch_.eyeBlinkR.target = ch_.eyeBlinkL.target;

    } else if (state == "LISTENING") {
        ch_.headTilt.target = 4.0f;
        ch_.headTurn.target = 3.0f;
        ch_.bodyLean.target = 4.0f;
        ch_.lShoulderAngle.target = 3.0f;
        ch_.lElbowAngle.target = -5.0f;
        ch_.rShoulderAngle.target = -3.0f;
        ch_.rElbowAngle.target = 5.0f;

        int bi = 200;
        ch_.eyeBlinkL.target = ((tick % bi) < 4) ? 1.0f : 0.0f;
        ch_.eyeBlinkR.target = ch_.eyeBlinkL.target;

    } else { // IDLE / default
        int bi = 130;
        ch_.eyeBlinkL.target = ((tick % bi) < 4) ? 1.0f : 0.0f;
        ch_.eyeBlinkR.target = ch_.eyeBlinkL.target;
    }

    // ── Eye Tracking ──
    float gazeX = 0, gazeY = 0;
    computeEyeTracking(gazeX, gazeY);
    ch_.eyeGaze.setTarget(gazeX, gazeY);

    // ── Lip Sync ──
    float mouthOpen = 0, mouthWidth = 0, mouthShape = 0;
    computeLipSync(speech, tick, mouthOpen, mouthWidth, mouthShape);
    ch_.mouthOpen.target = mouthOpen;
    ch_.mouthWidth.target = mouthWidth;

    // ── Mood Expression ──
    float browY = 0, browA = 0, mouthS = 0, blush = 0, eyeWide = 0;
    computeMoodExpression(browY, browA, mouthS, blush, eyeWide);
    ch_.browLY.target = browY; ch_.browRY.target = browY;
    ch_.browLAngle.target = browA; ch_.browRAngle.target = browA;
    // Blend mood mouth shape with lip sync
    ch_.mouthShape.target = mouthS * 0.6f + mouthShape * 0.4f;
    ch_.blushIntensity.target = blush;
}

// ═══════════════════════════════════════════════════════════════
//  EYE TRACKING
// ═══════════════════════════════════════════════════════════════

void AvatarRenderer::computeEyeTracking(float& outPx, float& outPy) {
    outPx = 0.0f; outPy = 0.0f;

    bool useMouse = false, useCam = false;
    switch (eyeMode_) {
        case EyeTrackMode::MOUSE: useMouse = true; break;
        case EyeTrackMode::CAMERA: useCam = true; break;
        case EyeTrackMode::AUTO:
            useMouse = hasMouse_;
            useCam = !hasMouse_;
            break;
        case EyeTrackMode::NONE:
            // Random idle gaze
            outPx = sinf(time_ * 0.15f) * 1.5f;
            outPy = cosf(time_ * 0.12f) * 0.8f;
            return;
    }

    if (useMouse && hasMouse_) {
        float dx = mouseX_;
        float dy = mouseY_;
        float dist = sqrtf(dx * dx + dy * dy);
        float maxOff = 3.5f;
        if (dist > 0.001f) {
            float scale = (std::min)(dist * 0.02f, maxOff) / dist;
            outPx = dx * scale;
            outPy = dy * scale;
        }
    } else if (useCam) {
        outPx = camFocusX_ * 3.0f;
        outPy = camFocusY_ * 2.5f;
    }
}

// ═══════════════════════════════════════════════════════════════
//  LIP SYNC
// ═══════════════════════════════════════════════════════════════

void AvatarRenderer::computeLipSync(const std::string& speech, int tick,
                                     float& outOpen, float& outWidth, float& outShape) {
    outOpen = 0.0f; outWidth = 0.0f; outShape = 0.0f;
    if (speech.empty()) return;

    int idx = (tick / 2) % (int)speech.length();
    char c = speech[idx];
    char next = (idx + 1 < (int)speech.length()) ? speech[idx + 1] : ' ';

    auto isVowel = [](char ch) {
        ch = (char)tolower((unsigned char)ch);
        return ch == 'a' || ch == 'e' || ch == 'i' || ch == 'o' || ch == 'u';
    };
    auto isWideVowel = [](char ch) {
        ch = (char)tolower((unsigned char)ch);
        return ch == 'a' || ch == 'e';
    };
    auto isRoundVowel = [](char ch) {
        ch = (char)tolower((unsigned char)ch);
        return ch == 'o' || ch == 'u';
    };
    auto isClosedCons = [](char ch) {
        ch = (char)tolower((unsigned char)ch);
        return ch == 'm' || ch == 'b' || ch == 'p';
    };
    auto isFricative = [](char ch) {
        ch = (char)tolower((unsigned char)ch);
        return ch == 's' || ch == 'z' || ch == 'f' || ch == 'v' || ch == 't' || ch == 'd' || ch == 'n' || ch == 'l';
    };

    if (c == ' ' || c == '.' || c == ',' || c == '!' || c == '?') {
        outOpen = 0.05f; outWidth = 0.3f; outShape = 0.0f; return;
    }

    if (isClosedCons(c)) {
        outOpen = 0.0f; outWidth = 0.4f; outShape = 0.0f;
    } else if (isFricative(c)) {
        outOpen = 0.15f; outWidth = 0.6f; outShape = 0.0f;
    } else if (isWideVowel(c)) {
        outOpen = 0.75f; outWidth = 0.8f; outShape = 0.1f;
    } else if (isRoundVowel(c)) {
        outOpen = 0.55f; outWidth = 0.5f; outShape = 0.2f;
    } else if (tolower((unsigned char)c) == 'i') {
        outOpen = 0.35f; outWidth = 0.7f; outShape = 0.15f;
    } else {
        outOpen = 0.25f; outWidth = 0.5f; outShape = 0.0f;
    }

    // Co-articulation
    if (isVowel(next) && !isVowel(c)) {
        outOpen = outOpen * 0.6f + 0.4f * 0.5f;
    }
}

// ═══════════════════════════════════════════════════════════════
//  MOOD EXPRESSION
// ═══════════════════════════════════════════════════════════════

void AvatarRenderer::computeMoodExpression(float& browY, float& browA, float& mouthS,
                                            float& blush, float& eyeWide) {
    float tBY = 0, tBA = 0, tMS = 0.1f, tBl = 0, tEW = 0;

    switch (mood_) {
        case Mood::HAPPY:       tBY=-1.5f; tBA=-5.0f;  tMS=0.7f;  tBl=0.4f; tEW=0.1f;  break;
        case Mood::EXCITED:     tBY=-2.5f; tBA=-8.0f;  tMS=0.85f; tBl=0.5f; tEW=0.3f;  break;
        case Mood::SAD:         tBY=2.0f;  tBA=8.0f;   tMS=-0.5f; tBl=0.1f; tEW=-0.1f; break;
        case Mood::WORRIED:     tBY=1.5f;  tBA=5.0f;   tMS=-0.2f; tBl=0.15f;tEW=0.05f; break;
        case Mood::THINKING:    tBY=-0.5f; tBA=-2.0f;  tMS=0.0f;  tBl=0.0f; tEW=-0.05f;break;
        case Mood::SURPRISED:   tBY=-3.0f; tBA=-10.0f; tMS=0.3f;  tBl=0.2f; tEW=0.5f;  break;
        case Mood::EMBARRASSED: tBY=0.5f;  tBA=3.0f;   tMS=0.2f;  tBl=0.8f; tEW=-0.1f; break;
        case Mood::ANGRY:       tBY=1.0f;  tBA=-12.0f; tMS=-0.3f; tBl=0.3f; tEW=0.15f; break;
        case Mood::CONFUSED:    tBY=-0.5f; tBA=-6.0f;  tMS=-0.1f; tBl=0.0f; tEW=0.0f;  break;
        default: break;
    }

    float blend = moodBlend_;
    browY = tBY * blend;
    browA = tBA * blend;
    mouthS = tMS * blend + 0.1f * (1.0f - blend);
    blush = tBl * blend;
    eyeWide = tEW * blend;
}

// ═══════════════════════════════════════════════════════════════
//  RENDER — BODY PARTS
// ═══════════════════════════════════════════════════════════════

void AvatarRenderer::drawShadow(Gdiplus::Graphics& g, float cx, float cy) {
    Gdiplus::GraphicsPath path;
    path.AddEllipse(cx - 42.0f, cy - 5.0f, 84.0f, 16.0f);
    fillGradientPath(g, path, Gdiplus::Color(50, 60, 40, 55), Gdiplus::Color(0, 0, 0, 0));
}

void AvatarRenderer::drawHairBack(Gdiplus::Graphics& g, float hx, float hy) {
    float sway = ch_.hairSwayMain.current;
    float swayL = ch_.hairSwaySideL.current;
    float swayR = ch_.hairSwaySideR.current;

    // Left side long hair
    Gdiplus::GraphicsPath hairL;
    hairL.AddBezier(
        Gdiplus::PointF(hx - 32.0f, hy - 20.0f),
        Gdiplus::PointF(hx - 42.0f + swayL * 0.8f, hy + 20.0f),
        Gdiplus::PointF(hx - 38.0f + swayL * 1.2f, hy + 70.0f),
        Gdiplus::PointF(hx - 30.0f + swayL * 0.6f, hy + 110.0f));
    hairL.AddBezier(
        Gdiplus::PointF(hx - 30.0f + swayL * 0.6f, hy + 110.0f),
        Gdiplus::PointF(hx - 22.0f + swayL * 0.3f, hy + 75.0f),
        Gdiplus::PointF(hx - 18.0f, hy + 30.0f),
        Gdiplus::PointF(hx - 22.0f, hy - 15.0f));
    fillLinearPath(g, hairL, hx - 32.0f, hy - 20.0f, hx - 30.0f + sway * 0.5f, hy + 110.0f,
                   pal_.hairMid, pal_.hairDark);

    // Right side long hair
    Gdiplus::GraphicsPath hairR;
    hairR.AddBezier(
        Gdiplus::PointF(hx + 28.0f, hy - 18.0f),
        Gdiplus::PointF(hx + 38.0f + swayR * 0.8f, hy + 22.0f),
        Gdiplus::PointF(hx + 34.0f + swayR * 1.2f, hy + 72.0f),
        Gdiplus::PointF(hx + 26.0f + swayR * 0.6f, hy + 108.0f));
    hairR.AddBezier(
        Gdiplus::PointF(hx + 26.0f + swayR * 0.6f, hy + 108.0f),
        Gdiplus::PointF(hx + 20.0f + swayR * 0.3f, hy + 70.0f),
        Gdiplus::PointF(hx + 16.0f, hy + 28.0f),
        Gdiplus::PointF(hx + 20.0f, hy - 12.0f));
    fillLinearPath(g, hairR, hx + 28.0f, hy - 18.0f, hx + 26.0f + sway * 0.5f, hy + 108.0f,
                   pal_.hairMid, pal_.hairDark);
}

void AvatarRenderer::drawLegBack(Gdiplus::Graphics& g, float hipX, float hipY,
                                  float kneeX, float kneeY, float ankleX, float ankleY,
                                  float footX, float footY) {
    // Thigh — oriented along hip→knee
    drawLimbSegment(g, hipX, hipY, kneeX, kneeY, 10.0f, 8.0f, pal_.skinBase, pal_.skinMid);
    // Calf — oriented along knee→ankle
    drawLimbSegment(g, kneeX, kneeY, footX, footY, 8.0f, 5.0f, pal_.skinMid, pal_.skinShadow);

    // Knee cap shadow
    Gdiplus::SolidBrush kneeShadow(Gdiplus::Color(40, 220, 190, 180));
    g.FillEllipse(&kneeShadow, kneeX - 5.0f, kneeY - 3.0f, 10.0f, 8.0f);

    // Sock
    Gdiplus::SolidBrush sockBrush(pal_.sockBase);
    g.FillEllipse(&sockBrush, footX - 6.0f, footY - SOCK_H, 12.0f, SOCK_H + 2.0f);

    // Shoe
    float shoeDir = (footX > hipX) ? 1.0f : -1.0f;
    Gdiplus::GraphicsPath shoe;
    shoe.AddEllipse(footX - 8.0f + shoeDir * 2.0f, footY - 3.0f, FOOT_W, FOOT_H + 4.0f);
    fillLinearPath(g, shoe, footX, footY - 3.0f, footX, footY + FOOT_H, pal_.shoeBase, pal_.shoeDark);

    // Shoe accent stripe
    Gdiplus::Pen accentPen(Gdiplus::Color(255, 235, 160, 180), 1.2f);
    g.DrawLine(&accentPen, footX - 6.0f + shoeDir * 2.0f, footY + 3.0f,
               footX + 8.0f + shoeDir * 2.0f, footY + 3.0f);
}

void AvatarRenderer::drawLegFront(Gdiplus::Graphics& g, float hipX, float hipY,
                                   float kneeX, float kneeY, float ankleX, float ankleY,
                                   float footX, float footY) {
    drawLimbSegment(g, hipX, hipY, kneeX, kneeY, 11.0f, 9.0f, pal_.skinBase, pal_.skinMid);
    drawLimbSegment(g, kneeX, kneeY, footX, footY, 9.0f, 5.5f, pal_.skinMid, pal_.skinShadow);

    Gdiplus::SolidBrush kneeShadow(Gdiplus::Color(45, 220, 190, 180));
    g.FillEllipse(&kneeShadow, kneeX - 6.0f, kneeY - 3.0f, 12.0f, 9.0f);

    Gdiplus::SolidBrush sockBrush(pal_.sockBase);
    g.FillEllipse(&sockBrush, footX - 7.0f, footY - SOCK_H, 14.0f, SOCK_H + 2.0f);

    float shoeDir = (footX < hipX) ? -1.0f : 1.0f;
    Gdiplus::GraphicsPath shoe;
    shoe.AddEllipse(footX - 9.0f + shoeDir * 2.0f, footY - 3.0f, FOOT_W + 2.0f, FOOT_H + 4.0f);
    fillLinearPath(g, shoe, footX, footY - 3.0f, footX, footY + FOOT_H, pal_.shoeBase, pal_.shoeDark);

    Gdiplus::Pen accentPen(Gdiplus::Color(255, 235, 160, 180), 1.2f);
    g.DrawLine(&accentPen, footX - 7.0f + shoeDir * 2.0f, footY + 3.0f,
               footX + 9.0f + shoeDir * 2.0f, footY + 3.0f);
}

void AvatarRenderer::drawSkirt(Gdiplus::Graphics& g, float wx, float wy) {
    float sway = ch_.bodySwayX.current;
    float breathe = ch_.bodyBreathe.current;

    float topW = 36.0f;
    float hemW = 54.0f + breathe * 2.0f;

    Gdiplus::GraphicsPath skirt;
    skirt.AddBezier(
        Gdiplus::PointF(wx - topW * 0.5f, wy),
        Gdiplus::PointF(wx - hemW * 0.5f + sway * 0.3f, wy + SKIRT_H * 0.5f),
        Gdiplus::PointF(wx - hemW * 0.55f + sway * 0.5f, wy + SKIRT_H * 0.85f),
        Gdiplus::PointF(wx - hemW * 0.35f + sway * 0.7f, wy + SKIRT_H));
    skirt.AddBezier(
        Gdiplus::PointF(wx - hemW * 0.35f + sway * 0.7f, wy + SKIRT_H),
        Gdiplus::PointF(wx, wy + SKIRT_H + 2.0f),
        Gdiplus::PointF(wx, wy + SKIRT_H + 2.0f),
        Gdiplus::PointF(wx + hemW * 0.35f + sway * 0.7f, wy + SKIRT_H));
    skirt.AddBezier(
        Gdiplus::PointF(wx + hemW * 0.35f + sway * 0.7f, wy + SKIRT_H),
        Gdiplus::PointF(wx + hemW * 0.55f + sway * 0.5f, wy + SKIRT_H * 0.85f),
        Gdiplus::PointF(wx + hemW * 0.5f + sway * 0.3f, wy + SKIRT_H * 0.5f),
        Gdiplus::PointF(wx + topW * 0.5f, wy));
    skirt.CloseFigure();

    fillLinearPath(g, skirt, wx, wy, wx + sway * 0.3f, wy + SKIRT_H,
                   pal_.skirtBase, pal_.skirtDark);

    // Pleat lines
    Gdiplus::Pen pleatPen(Gdiplus::Color(40, 160, 100, 125), 1.0f);
    for (int i = -4; i <= 4; i++) {
        float px = wx + i * 7.0f + sway * 0.2f;
        g.DrawLine(&pleatPen, px, wy + 3.0f, px + i * 1.2f + sway * 0.4f, wy + SKIRT_H - 2.0f);
    }

    // Hem highlight
    Gdiplus::Pen hemPen(pal_.outlineLight, 1.5f);
    g.DrawBezier(&hemPen,
        Gdiplus::PointF(wx - hemW * 0.35f + sway * 0.7f, wy + SKIRT_H),
        Gdiplus::PointF(wx, wy + SKIRT_H + 2.0f),
        Gdiplus::PointF(wx, wy + SKIRT_H + 2.0f),
        Gdiplus::PointF(wx + hemW * 0.35f + sway * 0.7f, wy + SKIRT_H));
}

void AvatarRenderer::drawTorso(Gdiplus::Graphics& g, float cx, float cy, float wx, float wy) {
    float breathe = ch_.bodyBreathe.current;
    float expand = 1.0f + breathe * 0.025f;
    float topW = CHEST_W * 0.5f * expand;
    float botW = WAIST_W * 0.5f;

    // Shirt body
    Gdiplus::GraphicsPath shirt;
    shirt.AddBezier(
        Gdiplus::PointF(cx - topW, cy),
        Gdiplus::PointF(cx - botW - 2.0f, cy + TORSO_H * 0.45f),
        Gdiplus::PointF(cx - botW, cy + TORSO_H * 0.8f),
        Gdiplus::PointF(wx - botW, wy));
    shirt.AddLine(Gdiplus::PointF(wx - botW, wy), Gdiplus::PointF(wx + botW, wy));
    shirt.AddBezier(
        Gdiplus::PointF(wx + botW, wy),
        Gdiplus::PointF(cx + botW, cy + TORSO_H * 0.8f),
        Gdiplus::PointF(cx + botW + 2.0f, cy + TORSO_H * 0.45f),
        Gdiplus::PointF(cx + topW, cy));
    shirt.CloseFigure();

    fillLinearPath(g, shirt, cx, cy, wx, wy, pal_.uniformBase, pal_.uniformMid);

    // Sailor collar (V shape)
    Gdiplus::PointF leftCollar[3] = {
        Gdiplus::PointF(cx - 20.0f, cy + 2.0f),
        Gdiplus::PointF(cx, cy + 18.0f),
        Gdiplus::PointF(cx - 9.0f, cy + 18.0f)
    };
    Gdiplus::PointF rightCollar[3] = {
        Gdiplus::PointF(cx + 20.0f, cy + 2.0f),
        Gdiplus::PointF(cx, cy + 18.0f),
        Gdiplus::PointF(cx + 9.0f, cy + 18.0f)
    };
    Gdiplus::SolidBrush collarFill(pal_.uniformBase);
    g.FillPolygon(&collarFill, leftCollar, 3);
    g.FillPolygon(&collarFill, rightCollar, 3);

    // Collar stripe
    Gdiplus::Pen collarPen(Gdiplus::Color(255, 255, 255, 255), 1.5f);
    g.DrawLine(&collarPen, cx - 18.0f, cy + 4.0f, cx - 2.0f, cy + 16.0f);
    g.DrawLine(&collarPen, cx + 18.0f, cy + 4.0f, cx + 2.0f, cy + 16.0f);

    // Large red bow
    float bowY = cy + 14.0f;
    // Left loop
    Gdiplus::GraphicsPath bowL;
    bowL.AddBezier(
        Gdiplus::PointF(cx - 4.0f, bowY),
        Gdiplus::PointF(cx - 22.0f, bowY - 10.0f),
        Gdiplus::PointF(cx - 24.0f, bowY + 8.0f),
        Gdiplus::PointF(cx - 4.0f, bowY + 6.0f));
    fillLinearPath(g, bowL, cx - 14.0f, bowY - 5.0f, cx - 4.0f, bowY + 5.0f,
                   pal_.bowHighlight, pal_.bowDark);

    // Right loop
    Gdiplus::GraphicsPath bowR;
    bowR.AddBezier(
        Gdiplus::PointF(cx + 4.0f, bowY),
        Gdiplus::PointF(cx + 22.0f, bowY - 10.0f),
        Gdiplus::PointF(cx + 24.0f, bowY + 8.0f),
        Gdiplus::PointF(cx + 4.0f, bowY + 6.0f));
    fillLinearPath(g, bowR, cx + 14.0f, bowY - 5.0f, cx + 4.0f, bowY + 5.0f,
                   pal_.bowHighlight, pal_.bowDark);

    // Bow center knot
    Gdiplus::SolidBrush bowCenter(pal_.bowDark);
    g.FillEllipse(&bowCenter, cx - 4.0f, bowY - 2.0f, 8.0f, 8.0f);

    // Bow ribbons hanging down
    float ribbonSway = sinf(time_ * 0.7f) * 2.0f;
    Gdiplus::PointF ribL[3] = {
        Gdiplus::PointF(cx - 3.0f, bowY + 4.0f),
        Gdiplus::PointF(cx - 14.0f + ribbonSway, bowY + 22.0f),
        Gdiplus::PointF(cx - 6.0f, bowY + 5.0f)
    };
    Gdiplus::PointF ribR[3] = {
        Gdiplus::PointF(cx + 3.0f, bowY + 4.0f),
        Gdiplus::PointF(cx + 14.0f + ribbonSway, bowY + 22.0f),
        Gdiplus::PointF(cx + 6.0f, bowY + 5.0f)
    };
    Gdiplus::SolidBrush ribbonBrush(pal_.bowBase);
    g.FillPolygon(&ribbonBrush, ribL, 3);
    g.FillPolygon(&ribbonBrush, ribR, 3);

    // Puffy sleeve L
    Gdiplus::GraphicsPath sleeveL;
    sleeveL.AddEllipse(cx - topW - 8.0f, cy - 2.0f, 20.0f, 18.0f);
    fillLinearPath(g, sleeveL, cx - topW - 8.0f, cy - 2.0f, cx - topW + 8.0f, cy + 14.0f,
                   pal_.uniformBase, pal_.uniformDark);

    // Puffy sleeve R
    Gdiplus::GraphicsPath sleeveR;
    sleeveR.AddEllipse(cx + topW - 12.0f, cy - 2.0f, 20.0f, 18.0f);
    fillLinearPath(g, sleeveR, cx + topW - 12.0f, cy - 2.0f, cx + topW + 4.0f, cy + 14.0f,
                   pal_.uniformBase, pal_.uniformDark);
}

// ── Tapered Limb Segment (oriented along skeleton direction) ──

void AvatarRenderer::drawLimbSegment(Gdiplus::Graphics& g, float x1, float y1,
                                      float x2, float y2, float w1, float w2,
                                      const Gdiplus::Color& c1, const Gdiplus::Color& c2) {
    float dx = x2 - x1, dy = y2 - y1;
    float dist = sqrtf(dx * dx + dy * dy);
    if (dist < 0.5f) return;

    // Perpendicular direction for width
    float px = -dy / dist, py = dx / dist;

    // Build tapered quad
    Gdiplus::PointF pts[4] = {
        Gdiplus::PointF(x1 + px * w1, y1 + py * w1),
        Gdiplus::PointF(x2 + px * w2, y2 + py * w2),
        Gdiplus::PointF(x2 - px * w2, y2 - py * w2),
        Gdiplus::PointF(x1 - px * w1, y1 - py * w1)
    };
    Gdiplus::GraphicsPath path;
    path.AddPolygon(pts, 4);
    // Round the joints
    path.AddEllipse(x1 - w1, y1 - w1, w1 * 2.0f, w1 * 2.0f);
    path.AddEllipse(x2 - w2, y2 - w2, w2 * 2.0f, w2 * 2.0f);

    Gdiplus::LinearGradientBrush brush(
        Gdiplus::PointF(x1, y1), Gdiplus::PointF(x2, y2), c1, c2);
    g.FillPath(&brush, &path);
}

// ── Arms ──

void AvatarRenderer::drawArmBack(Gdiplus::Graphics& g, float sx, float sy,
                                   float ex, float ey, float hx, float hy) {
    drawLimbSegment(g, sx, sy, ex, ey, 5.5f, 4.5f, pal_.skinBase, pal_.skinShadow);
    drawLimbSegment(g, ex, ey, hx, hy, 4.5f, 3.5f, pal_.skinShadow, pal_.skinBase);

    // Elbow shadow
    Gdiplus::SolidBrush elbowShadow(Gdiplus::Color(30, 210, 180, 170));
    g.FillEllipse(&elbowShadow, ex - 3.5f, ey - 3.0f, 7.0f, 6.0f);
}

void AvatarRenderer::drawArmFront(Gdiplus::Graphics& g, float sx, float sy,
                                    float ex, float ey, float hx, float hy) {
    drawLimbSegment(g, sx, sy, ex, ey, 6.0f, 5.0f, pal_.skinBase, pal_.skinMid);
    drawLimbSegment(g, ex, ey, hx, hy, 5.0f, 3.8f, pal_.skinMid, pal_.skinBase);

    Gdiplus::SolidBrush elbowShadow(Gdiplus::Color(35, 210, 180, 170));
    g.FillEllipse(&elbowShadow, ex - 4.0f, ey - 3.0f, 8.0f, 7.0f);
}

// ── Hands ──

void AvatarRenderer::drawHandClasped(Gdiplus::Graphics& g, float x, float y) {
    Gdiplus::SolidBrush skinBrush(pal_.skinBase);
    Gdiplus::SolidBrush shadowBrush(pal_.skinShadow);

    // Two overlapping palms
    g.FillEllipse(&skinBrush, x - 8.0f, y - 5.0f, 16.0f, 14.0f);
    g.FillEllipse(&shadowBrush, x - 6.0f, y + 2.0f, 12.0f, 6.0f);

    // Visible fingertips
    for (int i = 0; i < 4; i++) {
        float fx = x - 5.0f + i * 2.8f;
        g.FillEllipse(&skinBrush, fx, y - 7.0f + i * 0.3f, 2.8f, 5.0f);
    }

    // Thumbs
    g.FillEllipse(&skinBrush, x - 9.0f, y - 1.0f, 4.0f, 6.0f);
    g.FillEllipse(&skinBrush, x + 5.0f, y - 1.0f, 4.0f, 6.0f);
}

void AvatarRenderer::drawHandOpen(Gdiplus::Graphics& g, float x, float y, float angle, bool left) {
    Gdiplus::SolidBrush skinBrush(pal_.skinBase);

    Gdiplus::GraphicsState gs = g.Save();
    g.TranslateTransform(x, y);
    g.RotateTransform(angle * 0.5f);

    // Palm
    g.FillEllipse(&skinBrush, -5.0f, -3.0f, 10.0f, 12.0f);

    // Fingers
    float fW = 2.5f, fLen = 7.0f;
    float startX = left ? -3.5f : -3.0f;
    for (int i = 0; i < 4; i++) {
        float fx = startX + i * 2.0f;
        g.FillEllipse(&skinBrush, fx, -fLen - 1.0f + i * 0.4f, fW, fLen);
    }

    // Thumb
    float tx = left ? -6.0f : 3.0f;
    g.FillEllipse(&skinBrush, tx, -1.0f, 3.5f, 7.0f);

    g.Restore(gs);
}

// ── Neck ──

void AvatarRenderer::drawNeck(Gdiplus::Graphics& g, float nx, float ny) {
    Gdiplus::GraphicsPath neckPath;
    neckPath.AddRectangle(Gdiplus::RectF(nx - 6.5f, ny, 13.0f, NECK_H + 3.0f));
    fillLinearPath(g, neckPath, nx, ny, nx, ny + NECK_H, pal_.skinBase, pal_.skinShadow);

    // Chin shadow
    Gdiplus::SolidBrush chinShadow(Gdiplus::Color(40, 220, 195, 185));
    g.FillEllipse(&chinShadow, nx - 7.0f, ny - 1.0f, 14.0f, 6.0f);
}

// ── Head ──

void AvatarRenderer::drawHead(Gdiplus::Graphics& g, float hx, float hy) {
    float hw = HEAD_W * 0.5f, hh = HEAD_H * 0.5f;

    Gdiplus::GraphicsPath headPath;
    headPath.AddArc(hx - hw, hy - hh, hw * 2.0f, hh * 1.5f, 180, 180);
    headPath.AddBezier(
        Gdiplus::PointF(hx - hw, hy + hh * 0.25f),
        Gdiplus::PointF(hx - hw * 0.85f, hy + hh * 1.25f),
        Gdiplus::PointF(hx - hw * 0.3f, hy + hh * 1.65f),
        Gdiplus::PointF(hx, hy + hh * 1.75f));
    headPath.AddBezier(
        Gdiplus::PointF(hx, hy + hh * 1.75f),
        Gdiplus::PointF(hx + hw * 0.3f, hy + hh * 1.65f),
        Gdiplus::PointF(hx + hw * 0.85f, hy + hh * 1.25f),
        Gdiplus::PointF(hx + hw, hy + hh * 0.25f));
    headPath.CloseFigure();

    // Radial skin gradient (subsurface scattering warmth)
    fillGradientPath(g, headPath, pal_.skinBase, pal_.skinMid);

    // Jaw shadow
    Gdiplus::SolidBrush jawShadow(Gdiplus::Color(30, 230, 200, 195));
    g.FillEllipse(&jawShadow, hx - hw * 0.65f, hy + hh * 1.15f, hw * 1.3f, hh * 0.5f);
}

// ── Face ──

void AvatarRenderer::drawFace(Gdiplus::Graphics& g, float hx, float hy,
                                const std::string& state, int tick, const std::string& speech) {
    float eyeY = hy + 2.0f;
    float eyeDist = 11.0f;
    float eyeW = 14.0f, eyeH = 17.0f;

    // ── Eyebrows ──
    float browLOff = ch_.browLY.current;
    float browROff = ch_.browRY.current;
    float browLA = ch_.browLAngle.current;
    float browRA = ch_.browRAngle.current;

    Gdiplus::Pen browPen(pal_.hairDark, 1.8f);
    // Left brow
    {
        Gdiplus::GraphicsState bs = g.Save();
        g.TranslateTransform(hx - eyeDist, eyeY - 10.0f + browLOff);
        g.RotateTransform(browLA * 0.3f);
        g.DrawArc(&browPen, -eyeW * 0.5f - 1.0f, 0.0f, eyeW + 2.0f, 6.0f, 200.0f, 140.0f);
        g.Restore(bs);
    }
    // Right brow
    {
        Gdiplus::GraphicsState bs = g.Save();
        g.TranslateTransform(hx + eyeDist, eyeY - 10.0f + browROff);
        g.RotateTransform(-browRA * 0.3f);
        g.DrawArc(&browPen, -eyeW * 0.5f - 1.0f, 0.0f, eyeW + 2.0f, 6.0f, 200.0f, 140.0f);
        g.Restore(bs);
    }

    // ── Eyes ──
    float blinkL = ch_.eyeBlinkL.current;
    float blinkR = ch_.eyeBlinkR.current;

    for (int side = -1; side <= 1; side += 2) {
        float ex = hx + side * eyeDist - eyeW * 0.5f;
        float blink = (side < 0) ? blinkL : blinkR;

        if (blink > 0.85f) {
            // Closed eye curve
            Gdiplus::Pen lashPen(pal_.hairDark, 2.0f);
            g.DrawArc(&lashPen, ex, eyeY + 3.0f, eyeW, 6.0f, 190.0f, 160.0f);
        } else {
            // Sclera
            Gdiplus::SolidBrush whiteBrush(Gdiplus::Color(255, 255, 255, 255));
            g.FillEllipse(&whiteBrush, ex, eyeY, eyeW, eyeH);

            float px = ch_.eyeGaze.getX();
            float py = ch_.eyeGaze.getY();
            px = (std::max)(-3.5f, (std::min)(3.5f, px));
            py = (std::max)(-3.0f, (std::min)(3.0f, py));

            // 4-layer iris gradient
            Gdiplus::SolidBrush outerBrush(pal_.irisOuter);
            g.FillEllipse(&outerBrush, ex + 1.5f + px, eyeY + 1.5f + py,
                          eyeW - 3.0f, eyeH - 3.0f);

            Gdiplus::SolidBrush midBrush(pal_.irisMid);
            g.FillEllipse(&midBrush, ex + 3.0f + px, eyeY + 3.5f + py,
                          eyeW - 6.0f, eyeH - 6.0f);

            Gdiplus::SolidBrush innerBrush(pal_.irisInner);
            g.FillEllipse(&innerBrush, ex + 4.5f + px, eyeY + 7.0f + py,
                          eyeW - 9.0f, eyeH - 10.0f);

            // Pupil
            Gdiplus::SolidBrush pupilBrush(Gdiplus::Color(255, 90, 25, 45));
            g.FillEllipse(&pupilBrush, ex + 5.5f + px, eyeY + 6.0f + py,
                          eyeW - 11.0f, eyeH - 11.0f);

            // Sparkle highlights
            Gdiplus::SolidBrush sparkle(Gdiplus::Color(255, 255, 255, 255));
            g.FillEllipse(&sparkle, ex + 3.5f + px, eyeY + 3.0f + py, 4.0f, 5.0f);
            g.FillEllipse(&sparkle, ex + 8.0f + px, eyeY + 10.0f + py, 2.0f, 2.0f);

            // Upper lid shadow
            Gdiplus::SolidBrush lidShadow(Gdiplus::Color(45, 200, 180, 175));
            g.FillEllipse(&lidShadow, ex + 0.5f, eyeY + 0.5f, eyeW - 1.0f, 3.5f);

            // Eyelashes
            Gdiplus::Pen lashPen(pal_.outline, 1.8f);
            g.DrawArc(&lashPen, ex - 0.5f, eyeY - 0.5f, eyeW + 1.0f, 7.0f, 180.0f, 180.0f);

            // Partial blink interpolation
            if (blink > 0.05f) {
                int alpha = (std::max)(0, (std::min)(255, (int)(blink * 255)));
                Gdiplus::SolidBrush blinkBrush(Gdiplus::Color((BYTE)alpha, 255, 245, 240));
                float lidH = blink * eyeH * 0.85f;
                g.FillRectangle(&blinkBrush, ex - 1.0f, eyeY, eyeW + 2.0f, lidH);
            }
        }
    }

    // ── Blush ──
    float blushAlpha = ch_.blushIntensity.current * 120.0f;
    blushAlpha = (std::max)(0.0f, (std::min)(140.0f, blushAlpha));
    if (blushAlpha > 5.0f) {
        Gdiplus::Color blushCenter((BYTE)blushAlpha, pal_.blush.GetR(), pal_.blush.GetG(), pal_.blush.GetB());
        Gdiplus::Color blushEdge(0, pal_.blush.GetR(), pal_.blush.GetG(), pal_.blush.GetB());

        Gdiplus::GraphicsPath bL, bR;
        bL.AddEllipse(hx - 24.0f, eyeY + 10.0f, 14.0f, 8.0f);
        bR.AddEllipse(hx + 10.0f, eyeY + 10.0f, 14.0f, 8.0f);
        fillGradientPath(g, bL, blushCenter, blushEdge);
        fillGradientPath(g, bR, blushCenter, blushEdge);
    }

    // ── Nose ──
    Gdiplus::SolidBrush noseBrush(Gdiplus::Color(140, 220, 185, 178));
    g.FillEllipse(&noseBrush, hx - 1.0f, eyeY + 13.0f, 2.5f, 2.5f);

    // ── Mouth ──
    float mouthY = eyeY + 24.0f;
    float mOpen = ch_.mouthOpen.current;
    float mWidth = 4.0f + ch_.mouthWidth.current * 8.0f;
    float mShape = ch_.mouthShape.current;

    if (mOpen > 0.08f) {
        float mh = 2.0f + mOpen * 10.0f;
        // Dark interior
        Gdiplus::SolidBrush mouthBrush(Gdiplus::Color(255, 140, 40, 60));
        g.FillEllipse(&mouthBrush, hx - mWidth * 0.5f, mouthY, mWidth, mh);

        // Tongue for wide open
        if (mOpen > 0.4f) {
            Gdiplus::SolidBrush tongueBrush(Gdiplus::Color(255, 230, 140, 155));
            g.FillEllipse(&tongueBrush, hx - mWidth * 0.3f, mouthY + mh * 0.4f,
                          mWidth * 0.6f, mh * 0.5f);
        }

        // Lip outline
        Gdiplus::Pen lipPen(Gdiplus::Color(255, 210, 100, 125), 1.2f);
        g.DrawEllipse(&lipPen, hx - mWidth * 0.5f, mouthY, mWidth, mh);
    } else {
        // Closed mouth
        Gdiplus::Pen mouthPen(Gdiplus::Color(255, 210, 115, 135), 1.5f);
        if (mShape > 0.15f) {
            // Smile
            g.DrawArc(&mouthPen, hx - mWidth * 0.5f, mouthY - 2.0f,
                      mWidth, mWidth * 0.5f, 15.0f, 150.0f);
        } else if (mShape < -0.15f) {
            // Frown
            g.DrawArc(&mouthPen, hx - mWidth * 0.5f, mouthY + 1.0f,
                      mWidth, mWidth * 0.5f, 195.0f, 150.0f);
        } else {
            // Neutral
            g.DrawLine(&mouthPen, hx - mWidth * 0.4f, mouthY + 1.0f,
                       hx + mWidth * 0.4f, mouthY + 1.0f);
        }
    }
}

// ── Hair Front ──

void AvatarRenderer::drawHairFront(Gdiplus::Graphics& g, float hx, float hy) {
    float sway = ch_.hairSwayBangs.current;

    // Top dome
    Gdiplus::GraphicsPath hairTop;
    hairTop.AddEllipse(hx - 36.0f, hy - 36.0f, 72.0f, 34.0f);

    // Side locks
    hairTop.AddEllipse(hx - 38.0f, hy - 18.0f, 11.0f, 50.0f);
    hairTop.AddEllipse(hx + 27.0f, hy - 18.0f, 11.0f, 50.0f);

    // Center bangs (3 spikes)
    Gdiplus::PointF s1[3] = {
        Gdiplus::PointF(hx - 20.0f + sway * 0.3f, hy - 30.0f),
        Gdiplus::PointF(hx - 12.0f + sway * 0.5f, hy + 0.0f),
        Gdiplus::PointF(hx - 4.0f + sway * 0.2f, hy - 30.0f)
    };
    Gdiplus::PointF s2[3] = {
        Gdiplus::PointF(hx - 6.0f + sway * 0.2f, hy - 30.0f),
        Gdiplus::PointF(hx + sway * 0.4f, hy + 3.0f),
        Gdiplus::PointF(hx + 6.0f + sway * 0.2f, hy - 30.0f)
    };
    Gdiplus::PointF s3[3] = {
        Gdiplus::PointF(hx + 4.0f + sway * 0.2f, hy - 30.0f),
        Gdiplus::PointF(hx + 12.0f + sway * 0.5f, hy + 0.0f),
        Gdiplus::PointF(hx + 20.0f + sway * 0.3f, hy - 30.0f)
    };
    hairTop.AddPolygon(s1, 3);
    hairTop.AddPolygon(s2, 3);
    hairTop.AddPolygon(s3, 3);

    fillLinearPath(g, hairTop, hx, hy - 34.0f, hx, hy + 15.0f, pal_.hairBase, pal_.hairMid);

    // Shine highlight arc
    Gdiplus::Pen shinePen(pal_.hairShine, 2.2f);
    g.DrawArc(&shinePen, hx - 24.0f, hy - 34.0f, 48.0f, 18.0f, 200.0f, 140.0f);

    // Wispy strands
    Gdiplus::Pen strandPen(Gdiplus::Color(100, 215, 185, 205), 1.2f);
    g.DrawBezier(&strandPen,
        Gdiplus::PointF(hx - 30.0f, hy - 5.0f),
        Gdiplus::PointF(hx - 38.0f + sway * 0.8f, hy + 8.0f),
        Gdiplus::PointF(hx - 35.0f + sway * 0.6f, hy + 18.0f),
        Gdiplus::PointF(hx - 40.0f + sway * 0.4f, hy + 28.0f));
    g.DrawBezier(&strandPen,
        Gdiplus::PointF(hx + 28.0f, hy - 5.0f),
        Gdiplus::PointF(hx + 36.0f + sway * 0.8f, hy + 6.0f),
        Gdiplus::PointF(hx + 33.0f + sway * 0.6f, hy + 16.0f),
        Gdiplus::PointF(hx + 38.0f + sway * 0.4f, hy + 26.0f));
}

// ── Ponytail ──

void AvatarRenderer::drawPonytail(Gdiplus::Graphics& g, float hx, float hy) {
    float sway = ch_.hairSwayPony.current;
    float px = hx + 26.0f, py = hy - 20.0f;

    Gdiplus::GraphicsPath pony;
    pony.AddBezier(
        Gdiplus::PointF(px - 6.0f, py),
        Gdiplus::PointF(px + 14.0f + sway * 0.5f, py + 22.0f),
        Gdiplus::PointF(px + 22.0f + sway * 0.8f, py + 60.0f),
        Gdiplus::PointF(px + 10.0f + sway * 0.7f, py + 95.0f));
    pony.AddBezier(
        Gdiplus::PointF(px + 10.0f + sway * 0.7f, py + 95.0f),
        Gdiplus::PointF(px - 4.0f + sway * 0.3f, py + 70.0f),
        Gdiplus::PointF(px - 10.0f, py + 35.0f),
        Gdiplus::PointF(px - 6.0f, py));

    fillLinearPath(g, pony, px, py, px + 15.0f, py + 95.0f, pal_.hairBase, pal_.hairDark);

    // Highlight streak
    Gdiplus::Pen hiPen(pal_.hairHighlight, 2.0f);
    g.DrawBezier(&hiPen,
        Gdiplus::PointF(px + 2.0f, py + 10.0f),
        Gdiplus::PointF(px + 16.0f + sway * 0.6f, py + 35.0f),
        Gdiplus::PointF(px + 18.0f + sway * 0.7f, py + 62.0f),
        Gdiplus::PointF(px + 6.0f + sway * 0.5f, py + 85.0f));

    // Ribbon at tie point
    drawRibbon(g, px, py);
}

void AvatarRenderer::drawRibbon(Gdiplus::Graphics& g, float x, float y) {
    Gdiplus::SolidBrush ribbonBrush(Gdiplus::Color(255, 255, 250, 252));

    // Center clip
    g.FillEllipse(&ribbonBrush, x - 5.0f, y - 3.0f, 12.0f, 8.0f);

    // Bow tails
    Gdiplus::PointF bowL[3] = {
        Gdiplus::PointF(x - 1.0f, y + 1.0f),
        Gdiplus::PointF(x - 10.0f, y + 10.0f),
        Gdiplus::PointF(x + 1.0f, y + 3.0f)
    };
    Gdiplus::PointF bowR[3] = {
        Gdiplus::PointF(x + 5.0f, y + 1.0f),
        Gdiplus::PointF(x + 14.0f, y + 10.0f),
        Gdiplus::PointF(x + 7.0f, y + 3.0f)
    };
    g.FillPolygon(&ribbonBrush, bowL, 3);
    g.FillPolygon(&ribbonBrush, bowR, 3);
}

// ═══════════════════════════════════════════════════════════════
//  HELPERS
// ═══════════════════════════════════════════════════════════════

void AvatarRenderer::fillGradientPath(Gdiplus::Graphics& g, const Gdiplus::GraphicsPath& path,
                                       const Gdiplus::Color& center, const Gdiplus::Color& edge) {
    Gdiplus::PathGradientBrush brush(const_cast<Gdiplus::GraphicsPath*>(&path));
    brush.SetCenterColor(center);
    Gdiplus::Color e = edge;
    int count = 1;
    brush.SetSurroundColors(&e, &count);
    g.FillPath(&brush, const_cast<Gdiplus::GraphicsPath*>(&path));
}

void AvatarRenderer::fillLinearPath(Gdiplus::Graphics& g, const Gdiplus::GraphicsPath& path,
                                     float x1, float y1, float x2, float y2,
                                     const Gdiplus::Color& c1, const Gdiplus::Color& c2) {
    Gdiplus::LinearGradientBrush brush(
        Gdiplus::PointF(x1, y1), Gdiplus::PointF(x2, y2), c1, c2);
    g.FillPath(&brush, const_cast<Gdiplus::GraphicsPath*>(&path));
}

Gdiplus::Color AvatarRenderer::lerpColor(const Gdiplus::Color& a, const Gdiplus::Color& b, float t) {
    t = (std::max)(0.0f, (std::min)(1.0f, t));
    return Gdiplus::Color(
        (BYTE)((float)a.GetA() + ((float)b.GetA() - (float)a.GetA()) * t),
        (BYTE)((float)a.GetR() + ((float)b.GetR() - (float)a.GetR()) * t),
        (BYTE)((float)a.GetG() + ((float)b.GetG() - (float)a.GetG()) * t),
        (BYTE)((float)a.GetB() + ((float)b.GetB() - (float)a.GetB()) * t));
}

Gdiplus::Color AvatarRenderer::darken(const Gdiplus::Color& c, float factor) {
    return Gdiplus::Color(c.GetA(),
        (BYTE)(c.GetR() * factor), (BYTE)(c.GetG() * factor), (BYTE)(c.GetB() * factor));
}

Gdiplus::Color AvatarRenderer::lighten(const Gdiplus::Color& c, float factor) {
    return Gdiplus::Color(c.GetA(),
        (BYTE)(std::min)(255.0f, c.GetR() + (255.0f - c.GetR()) * factor),
        (BYTE)(std::min)(255.0f, c.GetG() + (255.0f - c.GetG()) * factor),
        (BYTE)(std::min)(255.0f, c.GetB() + (255.0f - c.GetB()) * factor));
}
