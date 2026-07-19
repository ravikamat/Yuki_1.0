#include "DetailView.h"
#include "input/VisionSystem.h"
#include <windowsx.h>
#include <dwmapi.h>
#include <sstream>
#include <iomanip>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "gdiplus.lib")

#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif
#ifndef DWMWCP_ROUND
#define DWMWCP_ROUND 2
#endif
#ifndef DWMWA_SYSTEMBACKDROP_TYPE
#define DWMWA_SYSTEMBACKDROP_TYPE 38
#endif
#ifndef DWMSBT_TRANSIENTWINDOW
#define DWMSBT_TRANSIENTWINDOW 3
#endif
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

#define WM_APP_REFRESH_DETAIL (WM_APP + 2)

DetailView::DetailView() : m_hwnd(NULL), m_hEditContent(NULL), m_hStateLabel(NULL), m_hBtnClose(NULL), m_gdiplusToken(0) {
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    Gdiplus::GdiplusStartup(&m_gdiplusToken, &gdiplusStartupInput, NULL);
}

DetailView::~DetailView() {
    if (m_hwnd) DestroyWindow(m_hwnd);
    if (m_gdiplusToken) Gdiplus::GdiplusShutdown(m_gdiplusToken);
}

bool DetailView::create(HINSTANCE instance) {
    WNDCLASSA wc = {0};
    wc.lpfnWndProc   = windowProc;
    wc.hInstance     = instance;
    wc.lpszClassName = "YukiDetailViewClass";
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);

    if (!RegisterClassA(&wc)) return false;

    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int width = 800; 
    int height = 600; 
    int x = (screenW - width) / 2;
    int y = (screenH - height) / 2;

    m_hwnd = CreateWindowExA(
        WS_EX_TOPMOST, 
        wc.lpszClassName,
        "Yuki Expanded Detail",
        WS_POPUP | WS_VISIBLE | WS_SIZEBOX, 
        x, y, width, height,
        NULL, NULL, instance, this
    );

    if (!m_hwnd) return false;

    DWM_BLURBEHIND bb = {0};
    bb.dwFlags = DWM_BB_ENABLE;
    bb.fEnable = TRUE;
    bb.hRgnBlur = NULL;
    HRESULT hr = DwmEnableBlurBehindWindow(m_hwnd, &bb);

    if (FAILED(hr)) {
        SetWindowLongA(m_hwnd, GWL_EXSTYLE, GetWindowLongA(m_hwnd, GWL_EXSTYLE) | WS_EX_LAYERED);
        SetLayeredWindowAttributes(m_hwnd, 0, 210, LWA_ALPHA);
    }

    DWORD cornerPref = DWMWCP_ROUND;
    DwmSetWindowAttribute(m_hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &cornerPref, sizeof(cornerPref));
    DWORD backdropType = DWMSBT_TRANSIENTWINDOW;
    DwmSetWindowAttribute(m_hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &backdropType, sizeof(backdropType));
    
    BOOL dark = TRUE;
    DwmSetWindowAttribute(m_hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));

    m_hStateLabel = CreateWindowA("STATIC", "YUKI: EXPANDED DETAIL", 
        WS_CHILD | WS_VISIBLE | SS_CENTER, 
        0, 0, 0, 0, m_hwnd, (HMENU)201, instance, NULL);

    m_hBtnClose = CreateWindowA("BUTTON", "X", 
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 
        0, 0, 0, 0, m_hwnd, (HMENU)205, instance, NULL);

    m_hEditContent = CreateWindowA("EDIT", "",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
        0, 0, 0, 0, m_hwnd, (HMENU)202, instance, NULL);

    HFONT hFontState = CreateFontA(
        15, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 
        OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, 
        VARIABLE_PITCH, "Segoe UI");
        
    HFONT hFontChat = CreateFontA(
        16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 
        OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, 
        VARIABLE_PITCH, "Consolas"); 
        
    SendMessage(m_hStateLabel, WM_SETFONT, (WPARAM)hFontState, TRUE);
    SendMessage(m_hBtnClose, WM_SETFONT, (WPARAM)hFontState, TRUE);
    SendMessage(m_hEditContent, WM_SETFONT, (WPARAM)hFontChat, TRUE);

    RECT rc;
    GetClientRect(m_hwnd, &rc);
    SendMessage(m_hwnd, WM_SIZE, 0, MAKELPARAM(rc.right, rc.bottom));

    return true;
}

void DetailView::show(int cmd_show) {
    if (m_hwnd) {
        ShowWindow(m_hwnd, cmd_show);
        UpdateWindow(m_hwnd);
    }
}

void DetailView::closeView(bool quitProcess) {
    if (quitProcess) {
        PostMessage(m_hwnd, WM_CLOSE, 0, 0);
    } else {
        ShowWindow(m_hwnd, SW_HIDE);
    }
}

void DetailView::setContent(const std::string& text) {
    m_pendingContent = text;
}

void DetailView::postRefresh() {
    if (m_hwnd) {
        PostMessage(m_hwnd, WM_APP_REFRESH_DETAIL, 0, 0);
    }
}

LRESULT CALLBACK DetailView::windowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    DetailView* pThis = nullptr;
    if (uMsg == WM_NCCREATE) {
        CREATESTRUCT* pCreate = (CREATESTRUCT*)lParam;
        pThis = (DetailView*)pCreate->lpCreateParams;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
        pThis->m_hwnd = hwnd;
    } else {
        pThis = (DetailView*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    }
    if (pThis) return pThis->handleMessage(hwnd, uMsg, wParam, lParam);
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

LRESULT DetailView::handleMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_APP_REFRESH_DETAIL: {
            if (m_syncCb) m_syncCb();
            
            std::string contentToShow = m_pendingContent;
            VisionResult res = vision().getLatestResult();
            if (res.mode != VisionMode::NONE) {
                std::ostringstream ss;
                ss << "======================================================================\r\n";
                ss << "YUKI ACTIVE REAL-TIME VISION PERCEPTION FEED\r\n";
                ss << "======================================================================\r\n\r\n";
                
                if (res.mode == VisionMode::CAMERA) {
                    ss << "[MODE]       : Camera Vision Mode (Environmental Perception)\r\n";
                    ss << "[STATUS]     : " << res.status << "\r\n";
                    ss << "[TIMESTAMP]  : " << res.timestamp << "\r\n";
                    ss << "[RESOLUTION] : " << res.frameWidth << "x" << res.frameHeight << " pixels\r\n";
                    ss << "[FRAME HASH] : 0x" << std::hex << std::uppercase << res.pixelHash << "\r\n\r\n";
                    ss << "[DETECTION RESULTS / DETAILS]:\r\n";
                    ss << "  " << res.details << "\r\n\r\n";
                } else if (res.mode == VisionMode::SCREEN) {
                    ss << "[MODE]       : Screen-Focused Vision Mode (Intentional Capture)\r\n";
                    ss << "[STATUS]     : " << res.status << "\r\n";
                    ss << "[TIMESTAMP]  : " << res.timestamp << "\r\n";
                    ss << "[RESOLUTION] : " << res.frameWidth << "x" << res.frameHeight << " pixels\r\n";
                    ss << "[RASTER HASH]: 0x" << std::hex << std::uppercase << res.pixelHash << "\r\n\r\n";
                    ss << "[ANALYSIS FOCUS / DETAILS]:\r\n";
                    ss << "  " << res.details << "\r\n\r\n";
                }
                
                ss << "----------------------------------------------------------------------\r\n";
                ss << "COGNITIVE DIALOGUE PERSISTENT BUFFERS:\r\n";
                ss << "----------------------------------------------------------------------\r\n";
                ss << m_pendingContent;
                
                contentToShow = ss.str();
            }
            
            SetWindowTextA(m_hEditContent, contentToShow.c_str());
            return 0;
        }
        case WM_COMMAND: {
            if (LOWORD(wParam) == 205 && HIWORD(wParam) == BN_CLICKED) { 
                closeView(false);
            }
            break;
        }
        case WM_SIZE: {
            int width = LOWORD(lParam);
            int height = HIWORD(lParam);

            int padding = 24;
            int headerH = 32;
            int gap = 20;

            if (m_hStateLabel) {
                MoveWindow(m_hStateLabel, padding + 32, padding, width - padding * 2 - 64, headerH, TRUE);
            }
            if (m_hBtnClose) {
                MoveWindow(m_hBtnClose, width - padding - 32, padding, 32, 32, TRUE);
            }
            
            int outY = padding + headerH + gap;
            int outH = height - padding - outY;
            
            if (outH > 0 && m_hEditContent) {
                MoveWindow(m_hEditContent, padding + 16, outY + 16, width - padding * 2 - 32, outH - 32, TRUE);
            }
            
            InvalidateRect(hwnd, NULL, TRUE);
            return 0;
        }
        case WM_ERASEBKGND: {
            HDC hdc = (HDC)wParam;
            RECT rc;
            GetClientRect(hwnd, &rc);
            FillRect(hdc, &rc, (HBRUSH)GetStockObject(BLACK_BRUSH)); 
            return 1;
        }
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            
            Gdiplus::Graphics graphics(hdc);
            graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
            
            int radius = 16; 
            
            RECT rcClient;
            GetClientRect(hwnd, &rcClient);
            
            int padding = 24;
            int headerH = 32;
            int gap = 20;
            int outY = padding + headerH + gap;
            int outH = rcClient.bottom - padding - outY;
            
            if (outH > 0) {
                Gdiplus::GraphicsPath path;
                int x = padding;
                int y = outY;
                int w = rcClient.right - padding * 2;
                int h = outH;
                
                path.AddArc(x, y, radius, radius, 180, 90);
                path.AddArc(x + w - radius, y, radius, radius, 270, 90);
                path.AddArc(x + w - radius, y + h - radius, radius, radius, 0, 90);
                path.AddArc(x, y + h - radius, radius, radius, 90, 90);
                path.CloseFigure();
                
                Gdiplus::Pen outPen(Gdiplus::Color(40, 255, 255, 255), 1.5f); 
                graphics.DrawPath(&outPen, &path);
            }
            
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_DRAWITEM: {
            LPDRAWITEMSTRUCT pdis = (LPDRAWITEMSTRUCT)lParam;
            if (pdis->CtlID == 205) { 
                HDC hdc = pdis->hDC;
                RECT rc = pdis->rcItem;
                
                bool isPressed = (pdis->itemState & ODS_SELECTED);
                
                Gdiplus::Graphics graphics(hdc);
                graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
                
                int w = rc.right - rc.left;
                int h = rc.bottom - rc.top;

                Gdiplus::SolidBrush clearBrush(Gdiplus::Color(255, 0, 0, 0));
                graphics.FillRectangle(&clearBrush, 0, 0, w, h);
                
                if (isPressed) {
                    Gdiplus::SolidBrush pressBrush(Gdiplus::Color(150, 200, 60, 60)); 
                    int radius = 16;
                    Gdiplus::GraphicsPath path;
                    path.AddArc(0, 0, radius, radius, 180, 90);
                    path.AddArc(w - radius, 0, radius, radius, 270, 90);
                    path.AddArc(w - radius, h - radius, radius, radius, 0, 90);
                    path.AddArc(0, h - radius, radius, radius, 90, 90);
                    path.CloseFigure();
                    graphics.FillPath(&pressBrush, &path);
                }
                
                SetBkMode(hdc, TRANSPARENT);
                SetTextColor(hdc, RGB(255, 255, 255));
                
                HFONT hFont = CreateFontA(15, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 
                            OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, 
                            VARIABLE_PITCH, "Segoe UI");
                HGDIOBJ oldF = SelectObject(hdc, hFont);
                DrawTextA(hdc, "X", -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                SelectObject(hdc, oldF);
                DeleteObject(hFont);
                return TRUE;
            }
            break;
        }
        case WM_NCHITTEST: {
            LRESULT hit = DefWindowProc(hwnd, uMsg, wParam, lParam);
            
            POINT pt;
            pt.x = GET_X_LPARAM(lParam);
            pt.y = GET_Y_LPARAM(lParam);
            
            RECT rc;
            GetWindowRect(hwnd, &rc);
            
            int border = 12;
            int dragHeight = 60;
            
            bool left = pt.x < rc.left + border;
            bool right = pt.x >= rc.right - border;
            bool top = pt.y < rc.top + border;
            bool bottom = pt.y >= rc.bottom - border;
            
            if (top && left) return HTTOPLEFT;
            if (top && right) return HTTOPRIGHT;
            if (bottom && left) return HTBOTTOMLEFT;
            if (bottom && right) return HTBOTTOMRIGHT;
            if (left) return HTLEFT;
            if (right) return HTRIGHT;
            if (top) return HTTOP;
            if (bottom) return HTBOTTOM;
            
            if (hit == HTCLIENT && pt.y < rc.top + dragHeight) {
                return HTCAPTION;
            }
            
            return hit;
        }
        case WM_CTLCOLORBTN: {
            HDC hdcBtn = (HDC)wParam;
            SetBkMode(hdcBtn, TRANSPARENT);
            return (LRESULT)GetStockObject(NULL_BRUSH);
        }
        case WM_CTLCOLORSTATIC: {
            if ((HWND)lParam == m_hEditContent || (HWND)lParam == m_hStateLabel) {
                HDC hdcStatic = (HDC)wParam;
                SetTextColor(hdcStatic, RGB(225, 225, 230));
                SetBkColor(hdcStatic, RGB(0, 0, 0)); 
                return (LRESULT)GetStockObject(BLACK_BRUSH);
            }
            break;
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
