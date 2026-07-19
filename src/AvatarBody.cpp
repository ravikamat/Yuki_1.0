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
    srand((unsigned int)GetTickCount());
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
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);

    if (!RegisterClassA(&wc)) return false;

    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    
    int width = 280; 
    int height = 440; 
    int x = screenW - width - 40; 
    int y = screenH - height - 100; 

    m_currentX = x;
    m_currentY = y;
    m_targetX = x;
    m_targetY = y;

    // Initialize decoupled part-based render engine
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

    SetTimer(m_hwnd, 1, 33, NULL);  // ~30 FPS for smooth spring animation
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
    if (!m_hwnd) return;
    
    // Suspend autonomous roaming while user dragging
    if (GetCapture() == m_hwnd) {
        RECT rect;
        GetWindowRect(m_hwnd, &rect);
        m_currentX = rect.left;
        m_currentY = rect.top;
        m_targetX = m_currentX;
        m_targetY = m_currentY;
        m_isMoving = false;
        return;
    }
    
    // Suspend roaming while active with user or actively perceiving to maintain focus
    if (m_currentState != "IDLE" || vision().getMode() != VisionMode::NONE) {
        m_isMoving = false;
        return;
    }
    
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int limitX_Min = 40;
    int limitX_Max = screenW - 280 - 40;
    int limitY_Min = 40;
    int limitY_Max = screenH - 440 - 100;
    
    if (!m_isMoving) {
        m_moveDelay--;
        if (m_moveDelay <= 0) {
            // Pick a new near target position
            int maxRadius = 140;
            int dx = (rand() % (maxRadius * 2)) - maxRadius;
            int dy = (rand() % (maxRadius * 2)) - maxRadius;
            
            m_targetX = m_currentX + dx;
            m_targetY = m_currentY + dy;
            
            if (m_targetX < limitX_Min) m_targetX = limitX_Min;
            if (m_targetX > limitX_Max) m_targetX = limitX_Max;
            if (m_targetY < limitY_Min) m_targetY = limitY_Min;
            if (m_targetY > limitY_Max) m_targetY = limitY_Max;
            
            m_isMoving = true;
            m_stepCount = 0;
        }
    } else {
        int dx = m_targetX - m_currentX;
        int dy = m_targetY - m_currentY;
        int dist = (int)sqrt(dx * dx + dy * dy);
        
        if (dist <= 3) {
            m_currentX = m_targetX;
            m_currentY = m_targetY;
            m_isMoving = false;
            m_moveDelay = 100 + (rand() % 140); // Stand still for ~5-12 seconds
        } else {
            // Move slowly towards target with a cute floating/walking bob
            float speed = 1.0f;
            float stepX = (dx / (float)dist) * speed;
            float stepY = (dy / (float)dist) * speed;
            
            m_currentX += (int)(stepX >= 0 ? ceil(stepX) : floor(stepX));
            m_currentY += (int)(stepY >= 0 ? ceil(stepY) : floor(stepY));
            
            SetWindowPos(m_hwnd, NULL, m_currentX, m_currentY, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        }
    }
}

void AvatarBody::renderLayeredWindow() {
    if (!m_hwnd) return;
    
    int w = 280;
    int h = 440;

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
        graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
        graphics.Clear(Gdiplus::Color(0, 0, 0, 0));

        float cx = w / 2.0f;
        float cy = (float)h - 25.0f; // Ground point between feet

        // Smart eye tracking based on vision system state
        if (vision().isScreenActive()) {
            m_renderer.setEyeTrackMode(AvatarRenderer::EyeTrackMode::MOUSE);
            POINT ptMouse;
            GetCursorPos(&ptMouse);
            ScreenToClient(m_hwnd, &ptMouse);
            m_renderer.setMousePos((float)ptMouse.x - cx, (float)ptMouse.y - cy);
        } else if (vision().isCameraActive()) {
            m_renderer.setEyeTrackMode(AvatarRenderer::EyeTrackMode::CAMERA);
            m_renderer.setCameraFocus(0.0f, 0.0f);  // Forward gaze; future: object center
        } else {
            m_renderer.setEyeTrackMode(AvatarRenderer::EyeTrackMode::NONE);
        }

        float breathOffset = sinf(m_animationTick * 0.06f) * 1.2f;
        float speakingBounce = 0.0f;

        if (m_currentState == "SPEAKING") {
            speakingBounce = sinf(m_animationTick * 0.4f) * 2.0f;
        }

        // Resolve perception state overrides
        std::string resolvedState = m_currentState;
        if (m_currentState != "SPEAKING") {
            if (vision().isCameraActive()) {
                resolvedState = "OBSERVING";
            } else if (vision().isScreenActive()) {
                resolvedState = "FOCUSED";
            }
        }

        // 1. Delegate character layered rendering to visual subsystem engine
        m_renderer.render(graphics, cx, cy, resolvedState, m_animationTick, speakingBounce, breathOffset, m_speechBubble);

        // 2. Modern Glassmorphism Speech Capsule Overlay
        if (!m_speechBubble.empty()) {
            int bW = 210;
            int bH = 36;
            int bX = (w - bW) / 2;
            int bY = 4;
            
            Gdiplus::GraphicsPath bubblePath;
            int radius = 10;
            bubblePath.AddArc(bX, bY, radius, radius, 180, 90);
            bubblePath.AddArc(bX + bW - radius, bY, radius, radius, 270, 90);
            bubblePath.AddArc(bX + bW - radius, bY + bH - radius, radius, radius, 0, 90);
            bubblePath.AddArc(bX, bY + bH - radius, radius, radius, 90, 90);
            bubblePath.CloseFigure();
            
            // Glass background
            Gdiplus::SolidBrush glassBg(Gdiplus::Color(215, 18, 24, 34));
            graphics.FillPath(&glassBg, &bubblePath);
            
            // Subtle glowing border
            Gdiplus::Pen glassBorder(Gdiplus::Color(110, 255, 255, 255), 1.2f);
            graphics.DrawPath(&glassBorder, &bubblePath);
            
            // Little pointing triangle tail
            Gdiplus::PointF tailPts[3] = {
                Gdiplus::PointF(cx - 5.0f, (float)(bY + bH)),
                Gdiplus::PointF(cx + 5.0f, (float)(bY + bH)),
                Gdiplus::PointF(cx, (float)(bY + bH + 6))
            };
            Gdiplus::SolidBrush tailBrush(Gdiplus::Color(215, 18, 24, 34));
            graphics.FillPolygon(&tailBrush, tailPts, 3);
            graphics.DrawLine(&glassBorder, cx - 5.0f, (float)(bY + bH), cx, (float)(bY + bH + 6));
            graphics.DrawLine(&glassBorder, cx + 5.0f, (float)(bY + bH), cx, (float)(bY + bH + 6));
            
            Gdiplus::FontFamily fontFamily(L"Segoe UI");
            Gdiplus::Font font(&fontFamily, 9.5f, Gdiplus::FontStyleBold, Gdiplus::UnitPoint);
            Gdiplus::SolidBrush textBrush(Gdiplus::Color(255, 245, 248, 255));
            
            // Proper UTF-8 to wide string conversion
            int wLen = MultiByteToWideChar(CP_UTF8, 0, m_speechBubble.c_str(), -1, NULL, 0);
            std::wstring wText(wLen, L'\0');
            MultiByteToWideChar(CP_UTF8, 0, m_speechBubble.c_str(), -1, &wText[0], wLen);
            Gdiplus::RectF layoutRect((float)bX + 4, (float)bY + 7, (float)bW - 8, (float)bH);
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

            // Update perception manager clock tick
            vision().tick();

            // Roaming disabled — avatar stays in its initial position
            // (was: updateRoaming())

            // Handle State Constraints
            if (m_currentState == "SPEAKING") {
                m_speakTime++;
                int expectedTicks = (int)(m_speechBubble.length() * 2.5f);
                if (expectedTicks < 45) expectedTicks = 45; // Min 2.25 seconds
                if (expectedTicks > 350) expectedTicks = 350; // Max 17.5 seconds
                
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
        case WM_NCHITTEST: {
            LRESULT hit = DefWindowProc(hwnd, uMsg, wParam, lParam);
            if (hit == HTCLIENT) return HTCAPTION;
            return hit;
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
