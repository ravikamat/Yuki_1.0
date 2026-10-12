#include "AvatarBody.h"
#include "input/VisionSystem.h"
#include <windowsx.h>
#include <cmath>

#pragma comment(lib, "gdiplus.lib")

#define WM_APP_REFRESH_AVATAR (WM_APP + 3)

AvatarBody::AvatarBody() : m_hwnd(NULL), m_gdiplusToken(0), m_currentState("IDLE"), m_animationTick(0), m_speakTime(0), 
                           m_currentX(0), m_currentY(0), m_targetX(0), m_targetY(0), 
                           m_isMoving(false), m_moveDelay(100), m_stepCount(0) {
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    Gdiplus::GdiplusStartup(&m_gdiplusToken, &gdiplusStartupInput, NULL);
}

AvatarBody::~AvatarBody() {
    if (m_hwnd) DestroyWindow(m_hwnd);
    if (m_gdiplusToken) Gdiplus::GdiplusShutdown(m_gdiplusToken);
}

bool AvatarBody::create(HINSTANCE instance) {
    WNDCLASSA wc = {0};
    wc.lpfnWndProc   = windowProc;
    wc.hInstance     = instance;
    wc.lpszClassName = "YukiAvatarBodyClass";
    wc.hCursor       = LoadCursor(NULL, IDC_SIZEALL); // Show draggable cursor
    wc.style         = CS_DBLCLKS;

    if (!RegisterClassA(&wc)) return false;

    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    
    int width = 240; 
    int height = 240; 
    int x = screenW - width - 50; 
    int y = screenH - height - 120; 

    m_currentX = x;
    m_currentY = y;
    m_targetX = x;
    m_targetY = y;

    // Initialize Grok Bloub vector avatar engine
    m_renderer.initialize();

    m_hwnd = CreateWindowExA(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW, 
        wc.lpszClassName,
        "Yuki Avatar",
        WS_POPUP | WS_VISIBLE, 
        x, y, width, height,
        NULL, NULL, instance, this
    );

    if (!m_hwnd) return false;

    SetTimer(m_hwnd, 1, 16, NULL);  // Silky 60 FPS update loop
    renderLayeredWindow();

    return true;
}

void AvatarBody::show(int cmd_show) {
    if (m_hwnd) {
        ShowWindow(m_hwnd, cmd_show);
        if (cmd_show == SW_SHOW || cmd_show == SW_SHOWNA) {
            renderLayeredWindow();
        }
    }
}

void AvatarBody::closeView(bool quitProcess) {
    if (quitProcess) {
        PostMessage(m_hwnd, WM_CLOSE, 0, 0);
    } else {
        ShowWindow(m_hwnd, SW_HIDE);
    }
}

void AvatarBody::setState(const std::string& state) {
    m_currentState = state;
    m_speakTime = 0;
    if (state == "THINKING") {
        m_speechBubble = "...";
    } else if (state == "LISTENING") {
        m_speechBubble = "Listening...";
    } else if (state == "IDLE") {
        m_speechBubble = "";
    }
    // Auto-map state → shape (only if not manually overridden by right-click)
    if (m_manualOverrideTicks <= 0) {
        applyStateShape(state);
    }
}

void AvatarBody::setSpeech(const std::string& speech) {
    if (speech.empty()) return;
    if (speech.length() > 25) {
        m_speechBubble = speech.substr(0, 22) + "...";
    } else {
        m_speechBubble = speech;
    }
}

void AvatarBody::postRefresh() {
    if (m_hwnd) {
        PostMessage(m_hwnd, WM_APP_REFRESH_AVATAR, 0, 0);
    }
}

void AvatarBody::updateRoaming() {
    // Roaming suspended for pinned interactive draggable desktop bot
}

void AvatarBody::renderLayeredWindow() {
    if (!m_hwnd) return;
    
    int w = 240;
    int h = 240;

    HDC hdcScreen = GetDC(NULL);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    
    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h; // Top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    
    void* pBits = nullptr;
    HBITMAP hbmMem = CreateDIBSection(hdcMem, &bmi, DIB_RGB_COLORS, &pBits, NULL, 0);
    HGDIOBJ hOld = SelectObject(hdcMem, hbmMem);

    {
        Gdiplus::Graphics graphics(hdcMem);
        graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
        graphics.Clear(Gdiplus::Color(0, 0, 0, 0)); // 100% Pure transparent canvas

        float cx = w / 2.0f;
        float cy = h / 2.0f;

        // Global Desktop Mouse Tracking: eyes follow cursor anywhere across the monitor
        m_renderer.setEyeTrackMode(AvatarRenderer::EyeTrackMode::MOUSE);
        POINT ptMouse;
        GetCursorPos(&ptMouse);
        ScreenToClient(m_hwnd, &ptMouse);
        m_renderer.setMousePos((float)ptMouse.x - cx, (float)ptMouse.y - cy);

        float breathOffset = sinf(m_animationTick * 0.06f) * 1.0f;
        float speakingBounce = (m_currentState == "SPEAKING") ? sinf(m_animationTick * 0.4f) * 2.0f : 0.0f;

        // 1. Render Grok Bloub vector avatar with pure transparency
        m_renderer.render(graphics, cx, cy, m_currentState, m_animationTick, speakingBounce, breathOffset, m_speechBubble);

        // 2. Glassmorphism Speech Capsule Overlay (when speaking)
        if (!m_speechBubble.empty()) {
            int bW = 180;
            int bH = 30;
            int bX = (w - bW) / 2;
            int bY = 6;
            
            Gdiplus::GraphicsPath bubblePath;
            int radius = 10;
            bubblePath.AddArc(bX, bY, radius, radius, 180, 90);
            bubblePath.AddArc(bX + bW - radius, bY, radius, radius, 270, 90);
            bubblePath.AddArc(bX + bW - radius, bY + bH - radius, radius, radius, 0, 90);
            bubblePath.AddArc(bX, bY + bH - radius, radius, radius, 90, 90);
            bubblePath.CloseFigure();
            
            Gdiplus::SolidBrush glassBg(Gdiplus::Color(225, 12, 14, 20));
            graphics.FillPath(&glassBg, &bubblePath);
            
            Gdiplus::Pen glassBorder(Gdiplus::Color(100, 255, 255, 255), 1.2f);
            graphics.DrawPath(&glassBorder, &bubblePath);
            
            Gdiplus::FontFamily fontFamily(L"Segoe UI");
            Gdiplus::Font font(&fontFamily, 9.0f, Gdiplus::FontStyleBold, Gdiplus::UnitPoint);
            Gdiplus::SolidBrush textBrush(Gdiplus::Color(255, 255, 255, 255));
            
            int wLen = MultiByteToWideChar(CP_UTF8, 0, m_speechBubble.c_str(), -1, NULL, 0);
            std::wstring wText(wLen, L'\0');
            MultiByteToWideChar(CP_UTF8, 0, m_speechBubble.c_str(), -1, &wText[0], wLen);
            Gdiplus::RectF layoutRect((float)bX + 2, (float)bY + 6, (float)bW - 4, (float)bH);
            Gdiplus::StringFormat format;
            format.SetAlignment(Gdiplus::StringAlignmentCenter);
            
            graphics.DrawString(wText.c_str(), -1, &font, layoutRect, &format, &textBrush);
        }
    }
    
    POINT ptSrc = {0, 0};
    SIZE size = {w, h};
    BLENDFUNCTION blend = {0};
    blend.BlendOp = AC_SRC_OVER;
    blend.BlendFlags = 0;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;

    UpdateLayeredWindow(m_hwnd, hdcScreen, NULL, &size, hdcMem, &ptSrc, 0, &blend, ULW_ALPHA);

    SelectObject(hdcMem, hOld);
    DeleteObject(hbmMem);
    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);
}

LRESULT CALLBACK AvatarBody::windowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    AvatarBody* pThis = nullptr;
    if (uMsg == WM_NCCREATE) {
        CREATESTRUCT* pCreate = (CREATESTRUCT*)lParam;
        pThis = (AvatarBody*)pCreate->lpCreateParams;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
        pThis->m_hwnd = hwnd;
    } else {
        pThis = (AvatarBody*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    }
    if (pThis) return pThis->handleMessage(hwnd, uMsg, wParam, lParam);
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

LRESULT AvatarBody::handleMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_APP_REFRESH_AVATAR: {
            renderLayeredWindow();
            return 0;
        }
        case WM_TIMER: {
            m_animationTick++;

            // Decay manual shape override timer
            if (m_manualOverrideTicks > 0) {
                m_manualOverrideTicks--;
                if (m_manualOverrideTicks == 0) {
                    // Brain regains shape control — re-apply current state's shape
                    applyStateShape(m_currentState);
                }
            }

            // Handle Speaking / Thinking timeout
            if (m_currentState == "SPEAKING") {
                m_speakTime++;
                int expectedTicks = (int)(m_speechBubble.length() * 3.0f);
                if (expectedTicks < 60) expectedTicks = 60;
                if (m_speakTime > expectedTicks) { 
                    m_currentState = "IDLE";
                    m_speechBubble = "";
                }
            } else if (m_currentState == "THINKING") {
                m_speakTime++;
                if (m_speakTime > 120) { 
                    m_currentState = "IDLE";
                    m_speechBubble = "";
                }
            }

            renderLayeredWindow();
            return 0;
        }
        case WM_LBUTTONDOWN: {
            // Drag window on left-click drag
            ReleaseCapture();
            SendMessage(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
            return 0;
        }
        case WM_RBUTTONUP: {
            // Morph to next signature shape on right-click
            nextShape();
            return 0;
        }
        case WM_LBUTTONDBLCLK: {
            // Morph to next signature shape on double-click
            nextShape();
            return 0;
        }
        case WM_PAINT: {
            PAINTSTRUCT ps;
            BeginPaint(hwnd, &ps);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_ERASEBKGND: {
            return 1;
        }
        case WM_CLOSE: {
            ShowWindow(hwnd, SW_HIDE);
            return 0;
        }
        case WM_DESTROY: {
            PostQuitMessage(0);
            return 0;
        }
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

// ═══════════════════════════════════════════════════════════════
//  STATE → SHAPE MAPPING (Yuki's actions drive her body shape)
// ═══════════════════════════════════════════════════════════════

void AvatarBody::applyStateShape(const std::string& state) {
    if (state == "IDLE") {
        m_renderer.setShape(AvatarRenderer::BotShapeId::CIRCLE);
    } else if (state == "LISTENING") {
        m_renderer.setShape(AvatarRenderer::BotShapeId::PEBBLE);
    } else if (state == "THINKING") {
        m_renderer.setShape(AvatarRenderer::BotShapeId::SQUIRCLE);
    } else if (state == "SPEAKING") {
        m_renderer.setShape(AvatarRenderer::BotShapeId::EGG);
    }
}

void AvatarBody::applyEmotionShape(int dominantEmotion, float valence, float curiosity, float fatigue) {
    // Emotion overrides shape only when emotion is strong enough
    // Emotion enum from EmbodiedInteroceptiveSelf.h:
    // 7=JOY, 8=SADNESS, 10=CURIOSITY, 11=FRUSTRATION, 6=ANGER

    if (dominantEmotion == 7 && valence > 0.5f) {
        m_renderer.setShape(AvatarRenderer::BotShapeId::CLOUD);     // Buoyant joy
    } else if (dominantEmotion == 10 && curiosity > 0.7f) {
        m_renderer.setShape(AvatarRenderer::BotShapeId::ORBIT);     // Reaching curiosity
    } else if (dominantEmotion == 8 && valence < -0.5f) {
        m_renderer.setShape(AvatarRenderer::BotShapeId::TEARDROP);  // Drooping sadness
    } else if (dominantEmotion == 11 || dominantEmotion == 6) {
        m_renderer.setShape(AvatarRenderer::BotShapeId::ALERT);     // Sharp frustration/anger
    } else if (fatigue > 0.7f) {
        m_renderer.setShape(AvatarRenderer::BotShapeId::CAPSULE);   // Heavy fatigue
    }
    // If no emotion is strong enough, state-driven shape remains active
}

void AvatarBody::setEmotionalDrive(float valence, float arousal, float curiosity, float fatigue, int dominantEmotion) {
    // Forward emotional data to the renderer for color/eye/breath modulation
    AvatarRenderer::EmotionalDrive drive;
    drive.valence = valence;
    drive.arousal = arousal;
    drive.curiosity = curiosity;
    drive.fatigue = fatigue;
    drive.dominantEmotion = dominantEmotion;
    m_renderer.setEmotionalDrive(drive);

    // Apply emotion-driven shape override (only if not manually overridden)
    if (m_manualOverrideTicks <= 0) {
        applyEmotionShape(dominantEmotion, valence, curiosity, fatigue);
    }
}
