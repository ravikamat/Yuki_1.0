#include "PresenceShell.h"
#include "SubsystemControl.h"
#include "input/VisionSystem.h"

#include <algorithm>
#include <iostream>
#include <dwmapi.h>
#include <uxtheme.h>
#include <memory>
#include <stdexcept>
#include <windowsx.h>
#include <sstream>
#include <ctime>
#include <cmath>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "uxtheme.lib")
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

#define WM_APP_REFRESH_CHAT       (WM_APP + 1)
#define WM_APP_SET_VOICE_DRAFT      (WM_APP + 2)
#define WM_APP_CLEAR_VOICE_DRAFT    (WM_APP + 3)
#define WM_APP_COMMIT_VOICE_DRAFT   (WM_APP + 4)

namespace {

std::string getWindowTextStringA(HWND hwnd) {
  if (!hwnd) return {};
  const int len = GetWindowTextLengthA(hwnd);
  if (len <= 0) return {};
  std::string text(static_cast<size_t>(len), ' ');
  GetWindowTextA(hwnd, text.data(), len + 1);
  return text;
}

void setCaretToEnd(HWND hwnd) {
  if (!hwnd) return;
  const int len = GetWindowTextLengthA(hwnd);
  SendMessageA(hwnd, EM_SETSEL, static_cast<WPARAM>(len), static_cast<LPARAM>(len));
  SendMessageA(hwnd, EM_SCROLLCARET, 0, 0);
}

void addRoundedRectToPath(Gdiplus::GraphicsPath &path, const RECT &rc, int radius) {
  const int x = rc.left;
  const int y = rc.top;
  const int w = rc.right - rc.left;
  const int h = rc.bottom - rc.top;
  if (w <= 0 || h <= 0) return;

  const int d = (std::min)(radius, (std::min)(w / 2, h / 2));
  if (d <= 0) {
    path.AddRectangle(Gdiplus::Rect(x, y, w, h));
    return;
  }

  path.AddArc(x, y, d, d, 180, 90);
  path.AddArc(x + w - d, y, d, d, 270, 90);
  path.AddArc(x + w - d, y + h - d, d, d, 0, 90);
  path.AddArc(x, y + h - d, d, d, 90, 90);
  path.CloseFigure();
}

} // namespace

// ── Lifecycle ────────────────────────────────────────────────────────────────

PresenceShell::PresenceShell(SessionState& session) : m_session(session) {
  Gdiplus::GdiplusStartupInput startupInput;
  Gdiplus::GdiplusStartup(&m_gdiplusToken, &startupInput, nullptr);
}

PresenceShell::~PresenceShell() {
  if (m_hwnd && IsWindow(m_hwnd)) {
    DestroyWindow(m_hwnd);
    m_hwnd = nullptr;
  }
  if (m_hFontState)  { DeleteObject(m_hFontState);  m_hFontState = nullptr; }
  if (m_hFontChat)   { DeleteObject(m_hFontChat);   m_hFontChat = nullptr; }
  if (m_hFontSmall)  { DeleteObject(m_hFontSmall);  m_hFontSmall = nullptr; }
  if (m_gdiplusToken) {
    Gdiplus::GdiplusShutdown(m_gdiplusToken);
    m_gdiplusToken = 0;
  }
}

// ── Create / Init ───────────────────────────────────────────────────────────

bool PresenceShell::create(HINSTANCE instance) {
  WNDCLASSA wc = {};
  wc.lpfnWndProc = PresenceShell::windowProc;
  wc.hInstance = instance;
  wc.lpszClassName = "YukiPresenceShellClass";
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.hbrBackground = CreateSolidBrush(RGB(12, 12, 14));
  RegisterClassA(&wc);

  const int screenW = GetSystemMetrics(SM_CXSCREEN);
  const int screenH = GetSystemMetrics(SM_CYSCREEN);
  const int width   = 400;
  const int height  = 540;
  const int x = screenW - width - 28;
  const int y = screenH - height - 48;

  m_hwnd = CreateWindowExA(WS_EX_TOPMOST, wc.lpszClassName, "Yuki Presence",
                           WS_POPUP | WS_VISIBLE | WS_THICKFRAME, x, y, width, height,
                           nullptr, nullptr, instance, this);
  if (!m_hwnd) return false;

  // Acrylic / glass backdrop
  DWM_BLURBEHIND bb = {};
  bb.dwFlags = DWM_BB_ENABLE;
  bb.fEnable = TRUE;
  bb.hRgnBlur = nullptr;
  const HRESULT hr = DwmEnableBlurBehindWindow(m_hwnd, &bb);
  if (FAILED(hr)) {
    SetWindowLongA(m_hwnd, GWL_EXSTYLE,
                   GetWindowLongA(m_hwnd, GWL_EXSTYLE) | WS_EX_LAYERED);
    SetLayeredWindowAttributes(m_hwnd, 0, 220, LWA_ALPHA);
  }

  DWORD cornerPref = DWMWCP_ROUND;
  DwmSetWindowAttribute(m_hwnd, DWMWA_WINDOW_CORNER_PREFERENCE,
                        &cornerPref, sizeof(cornerPref));

  DWORD backdropType = DWMSBT_TRANSIENTWINDOW;
  DwmSetWindowAttribute(m_hwnd, DWMWA_SYSTEMBACKDROP_TYPE,
                        &backdropType, sizeof(backdropType));

  BOOL dark = TRUE;
  DwmSetWindowAttribute(m_hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE,
                        &dark, sizeof(dark));

  // ── Child windows ─────────────────────────────────────────────────────────
  m_hStateLabel = CreateWindowA(
      "STATIC", "YUKI: IDLE", WS_CHILD | WS_VISIBLE | SS_CENTER,
      0, 0, 0, 0, m_hwnd, reinterpret_cast<HMENU>(101), instance, nullptr);

  m_hBtnClose = CreateWindowA(
      "BUTTON", "X", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
      0, 0, 0, 0, m_hwnd, reinterpret_cast<HMENU>(105), instance, nullptr);

  m_hBtnMic = CreateWindowA(
      "BUTTON", "Mic", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
      0, 0, 0, 0, m_hwnd, reinterpret_cast<HMENU>(201), instance, nullptr);

  m_hBtnSpeaker = CreateWindowA(
      "BUTTON", "Speaker", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
      0, 0, 0, 0, m_hwnd, reinterpret_cast<HMENU>(202), instance, nullptr);

  m_hBtnCamera = CreateWindowA(
      "BUTTON", "Camera", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
      0, 0, 0, 0, m_hwnd, reinterpret_cast<HMENU>(203), instance, nullptr);

  m_hBtnScreen = CreateWindowA(
      "BUTTON", "Screen", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
      0, 0, 0, 0, m_hwnd, reinterpret_cast<HMENU>(204), instance, nullptr);

  LoadLibraryA("riched20.dll");

  m_hEditOutput = CreateWindowA(
      "RichEdit20A", "",
      WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
      0, 0, 0, 0, m_hwnd, reinterpret_cast<HMENU>(102), instance, nullptr);
  SendMessageA(m_hEditOutput, EM_SETBKGNDCOLOR, 0, RGB(16, 16, 18));

  m_hEditInput = CreateWindowA(
      "EDIT", "", WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL,
      0, 0, 0, 0, m_hwnd, reinterpret_cast<HMENU>(103), instance, nullptr);

  m_hIpLabel = CreateWindowA(
      "STATIC", "Server starting...",
      WS_CHILD | WS_VISIBLE | SS_CENTER,
      0, 0, 0, 0, m_hwnd, reinterpret_cast<HMENU>(205), instance, nullptr);

  m_hBtnSend = CreateWindowA(
      "BUTTON", "Send", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
      0, 0, 0, 0, m_hwnd, reinterpret_cast<HMENU>(104), instance, nullptr);

  m_hBtnSafetyOK = CreateWindowA(
      "BUTTON", "I understand, continue", WS_CHILD | BS_OWNERDRAW,
      0, 0, 0, 0, m_hwnd, reinterpret_cast<HMENU>(301), instance, nullptr);

  m_hBtnSafetyCancel = CreateWindowA(
      "BUTTON", "Cancel", WS_CHILD | BS_OWNERDRAW,
      0, 0, 0, 0, m_hwnd, reinterpret_cast<HMENU>(302), instance, nullptr);

  // ── Fonts ────────────────────────────────────────────────────────────────
  m_hFontState = CreateFontA(14, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
      DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS,
      CLEARTYPE_QUALITY, VARIABLE_PITCH, "Segoe UI");
  if (!m_hFontState) m_hFontState = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));

  m_hFontChat = CreateFontA(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
      DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS,
      CLEARTYPE_QUALITY, VARIABLE_PITCH, "Segoe UI");
  if (!m_hFontChat) m_hFontChat = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));

  m_hFontSmall = CreateFontA(10, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
      DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS,
      CLEARTYPE_QUALITY, VARIABLE_PITCH, "Segoe UI");
  if (!m_hFontSmall) m_hFontSmall = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));

  SendMessageA(m_hStateLabel, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFontState), TRUE);
  SendMessageA(m_hBtnClose, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFontState), TRUE);
  SendMessageA(m_hBtnMic, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFontState), TRUE);
  SendMessageA(m_hBtnSpeaker, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFontState), TRUE);
  SendMessageA(m_hBtnCamera, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFontState), TRUE);
  SendMessageA(m_hBtnScreen, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFontState), TRUE);
  SendMessageA(m_hEditOutput, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFontChat), TRUE);
  SendMessageA(m_hEditInput, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFontChat), TRUE);
  SendMessageA(m_hBtnSend, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFontChat), TRUE);
  if (m_hIpLabel) {
    SendMessageA(m_hIpLabel, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFontSmall), TRUE);
  }

  // ── Subclass input ───────────────────────────────────────────────────────
  m_originalInputProc = reinterpret_cast<WNDPROC>(SetWindowLongPtrA(
      m_hEditInput, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(inputEditProc)));

  SetPropA(m_hEditInput, "PresenceShellPtr", static_cast<HANDLE>(this));

  // Remove borders
  auto stripBorder = [](HWND h) {
    LONG_PTR exStyle = GetWindowLongPtrA(h, GWL_EXSTYLE);
    exStyle &= ~WS_EX_CLIENTEDGE;
    SetWindowLongPtrA(h, GWL_EXSTYLE, exStyle);
    SetWindowPos(h, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
  };
  stripBorder(m_hEditOutput);
  stripBorder(m_hEditInput);

  HWND btns[] = { m_hBtnClose, m_hBtnMic, m_hBtnSpeaker,
                  m_hBtnCamera, m_hBtnScreen, m_hBtnSend,
                  m_hBtnSafetyOK, m_hBtnSafetyCancel };
  for (HWND b : btns) {
    if (b) SetWindowTheme(b, L"", L"");
  }

  // Initial layout
  RECT rc{};
  GetClientRect(m_hwnd, &rc);
  layoutChildren(rc.right - rc.left, rc.bottom - rc.top);
  updateStateLabel();
  return true;
}

// ── Show / Run / Close ───────────────────────────────────────────────────────

void PresenceShell::show(int cmd_show) {
  if (m_hwnd) {
    ShowWindow(m_hwnd, cmd_show);
    UpdateWindow(m_hwnd);
  }
}

int PresenceShell::run() {
  MSG msg = {};
  while (GetMessageA(&msg, nullptr, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessageA(&msg);
  }
  return static_cast<int>(msg.wParam);
}

void PresenceShell::closeShell(bool quitProcess) {
  if (!m_hwnd) return;
  if (quitProcess) {
    PostMessageA(m_hwnd, WM_CLOSE, 0, 0);
  } else {
    ShowWindow(m_hwnd, SW_HIDE);
  }
}

// ── State & Labels ───────────────────────────────────────────────────────────

void PresenceShell::setState(const std::string &stateText) {
  m_currentState = stateText;
  updateStateLabel();
}

void PresenceShell::updateStateLabel() {
  if (!m_hStateLabel) return;

  std::string display = "YUKI: " + m_currentState;
  if (vision().isCameraActive()) {
    display = "YUKI: PERCEIVING CAMERA";
  } else if (vision().isScreenActive()) {
    display = "YUKI: ANALYZING SCREEN";
  }

  // If thinking, show the most active layer detail (lock to avoid data race with inference thread)
  {
    std::lock_guard<std::mutex> lk(m_layersMutex_);
    if (m_showThinkingStrip && !m_cognitiveLayers.empty()) {
      for (const auto& layer : m_cognitiveLayers) {
        if (layer.activity > 0.35f) {
          display = "YUKI: " + layer.detail;
          break;
        }
      }
    }
  }

  SetWindowTextA(m_hStateLabel, display.c_str());
  InvalidateRect(m_hStateLabel, nullptr, TRUE);
}

// ── Chat History ─────────────────────────────────────────────────────────────

void PresenceShell::appendMessage(const std::string &speaker,
                                  const std::string &text, bool isVoice) {
  if (!m_hEditOutput) return;

  const std::string prefix = isVoice ? "\xF0\x9F\x8E\xA4 " : "";
  const std::string header = prefix + speaker + ":\r\n";
  const std::string body = text + "\r\n\r\n";

  int len = GetWindowTextLengthA(m_hEditOutput);
  SendMessageA(m_hEditOutput, EM_SETSEL, len, len);

  CHARFORMAT2A cf = { sizeof(cf) };
  cf.dwMask = CFM_COLOR | CFM_BOLD | CFM_BACKCOLOR;
  cf.crTextColor = RGB(0, 200, 185);
  cf.crBackColor = RGB(16, 16, 18);
  cf.dwEffects = CFE_BOLD;
  SendMessageA(m_hEditOutput, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
  SendMessageA(m_hEditOutput, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(header.c_str()));

  len = GetWindowTextLengthA(m_hEditOutput);
  SendMessageA(m_hEditOutput, EM_SETSEL, len, len);

  CHARFORMAT2A cfBody = { sizeof(cfBody) };
  cfBody.dwMask = CFM_COLOR | CFM_BOLD | CFM_BACKCOLOR;
  cfBody.crTextColor = RGB(210, 215, 230);
  cfBody.crBackColor = RGB(16, 16, 18);
  cfBody.dwEffects = 0;
  SendMessageA(m_hEditOutput, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cfBody);
  SendMessageA(m_hEditOutput, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(body.c_str()));
  SendMessageA(m_hEditOutput, EM_SCROLLCARET, 0, 0);
}

void PresenceShell::rebuildHistory(const std::vector<ChatEntry> &history) {
  if (!m_hEditOutput) return;
  SetWindowTextA(m_hEditOutput, "");
  for (const auto &entry : history) {
    appendMessage(entry.speaker, entry.text, entry.isVoice);
  }
}

void PresenceShell::postRefresh() {
  if (m_hwnd) PostMessageA(m_hwnd, WM_APP_REFRESH_CHAT, 0, 0);
}

// ── Voice Draft ──────────────────────────────────────────────────────────────

void PresenceShell::postSetVoiceDraftText(const std::string &text) {
  if (!m_hwnd) return;
  auto *heapText = new std::string(text);
  if (!PostMessageA(m_hwnd, WM_APP_SET_VOICE_DRAFT, 0, reinterpret_cast<LPARAM>(heapText))) {
    delete heapText;
  }
}

void PresenceShell::postClearVoiceDraftText() {
  if (m_hwnd) PostMessageA(m_hwnd, WM_APP_CLEAR_VOICE_DRAFT, 0, 0);
}

void PresenceShell::postCommitVoiceDraftText() {
  if (m_hwnd) PostMessageA(m_hwnd, WM_APP_COMMIT_VOICE_DRAFT, 0, 0);
}

std::string PresenceShell::normalizeDraftText(const std::string &text) {
  std::string out;
  out.reserve(text.size());
  bool prevSpace = false;
  for (unsigned char ch : text) {
    char c = static_cast<char>(ch);
    if (c == '\r' || c == '\n' || c == '\t') c = ' ';
    if (c == ' ') {
      if (prevSpace) continue;
      prevSpace = true;
      out.push_back(' ');
    } else {
      prevSpace = false;
      out.push_back(c);
    }
  }
  while (!out.empty() && std::isspace(static_cast<unsigned char>(out.front()))) out.erase(out.begin());
  while (!out.empty() && std::isspace(static_cast<unsigned char>(out.back()))) out.pop_back();
  return out;
}

std::string PresenceShell::getInputText() const {
  return getWindowTextStringA(m_hEditInput);
}

void PresenceShell::applyEditTextPreservingCaret(const std::string &text) {
  if (!m_hEditInput) return;
  m_applyingProgrammaticInput = true;
  SetWindowTextA(m_hEditInput, text.c_str());
  setCaretToEnd(m_hEditInput);
  m_applyingProgrammaticInput = false;

  RECT rc{};
  GetClientRect(m_hwnd, &rc);
  layoutChildren(rc.right - rc.left, rc.bottom - rc.top);
}

void PresenceShell::setVoiceDraftText(const std::string &text) {
  if (!m_hEditInput) return;
  const std::string normalized = normalizeDraftText(text);
  if (!m_voiceDraftActive) {
    m_inputBeforeVoiceDraft = getInputText();
    m_voiceDraftActive = true;
  }
  if (normalized == m_voiceDraftText) return;
  m_voiceDraftText = normalized;
  applyEditTextPreservingCaret(m_voiceDraftText);
}

void PresenceShell::clearVoiceDraftText() {
  if (!m_hEditInput || !m_voiceDraftActive) return;
  const std::string current = getInputText();
  if (current == m_voiceDraftText || current.empty()) {
    applyEditTextPreservingCaret(m_inputBeforeVoiceDraft);
  }
  m_voiceDraftText.clear();
  m_inputBeforeVoiceDraft.clear();
  m_voiceDraftActive = false;
}

void PresenceShell::commitVoiceDraftText() {
  if (!m_hEditInput || !m_voiceDraftActive) return;
  const std::string current = getInputText();
  if (current != m_voiceDraftText && !current.empty()) {
    m_voiceDraftText = current;
  }
  m_inputBeforeVoiceDraft.clear();
  m_voiceDraftActive = false;
}

// ── Send ─────────────────────────────────────────────────────────────────────

void PresenceShell::onSend() {
  std::string text = getInputText();
  while (!text.empty() && (text.back() == '\n' || text.back() == '\r')) text.pop_back();
  if (text.empty()) return;

  // Show user input immediately in chat history (before LLM responds)
  appendMessage("You", text, false);

  m_voiceDraftActive = false;
  m_voiceDraftText.clear();
  m_inputBeforeVoiceDraft.clear();
  SetWindowTextA(m_hEditInput, "");
  SendMessageA(m_hwnd, WM_COMMAND, MAKEWPARAM(103, EN_CHANGE), reinterpret_cast<LPARAM>(m_hEditInput));

  if (m_inClarificationMode) {
    m_inClarificationMode = false;
    EnableWindow(m_hEditOutput, TRUE);
    invalidateShell();
    RECT rc{};
    GetClientRect(m_hwnd, &rc);
    layoutChildren(rc.right - rc.left, rc.bottom - rc.top);
  }

  if (m_processCb) {
    std::string reply = m_processCb(text);
    if (reply == "__QUIT_SIGNAL_SENT__") return;
    // BabyMode::process() calls presence_shell_->show_response() directly,
    // so the GUI display is already handled by the time we get here.
  }
}


// ── Layout (FIXED: clarification panel no longer crushed) ────────────────────

void PresenceShell::layoutChildren(int width, int height) {
  if (m_inLayout) return;
  m_inLayout = true;

  const int padding = 16;
  const int headerH = 24;
  const int dockH = 44;
  const int ipLabelH = 16;
  const int thinkingH = 30;
  const int btnW = 40;
  const int kClarifyMinH = 96;  // was 68 — too small for wrapped text

  // Input height from line count
  int lineCount = 1;
  if (m_hEditInput) {
    lineCount = static_cast<int>(SendMessageA(m_hEditInput, EM_GETLINECOUNT, 0, 0));
    lineCount = (std::max)(1, lineCount);
  }
  int inputH = 34 + (lineCount - 1) * 18;
  inputH = (std::min)(inputH, 100);

  // Bottom-up anchor: input stays at bottom
  const int inputY = height - padding - inputH;
  const int inputW = width - padding * 2 - btnW - 8; // 8 = gap

  // Clarification panel sits directly above input
  const int clarifyH = m_inClarificationMode ? kClarifyMinH : 0;
  const int clarifyBot = inputY - 6;
  const int clarifyTop = clarifyBot - clarifyH;

  // Output area ends where clarification begins (or near input if no clarification)
  const int outputBot = m_inClarificationMode ? (clarifyTop - 6) : (inputY - 10);

  // Top-down
  const int stateY = padding;
  const int dockY = stateY + headerH + 16;
  const int ipLabelY = dockY + dockH + 4;
  const int ipLabelBot = ipLabelY + ipLabelH;

  const int thinkingY = ipLabelBot + 4;
  const int thinkingBot = thinkingY + thinkingH;
  const bool showThinking = m_showThinkingStrip && !m_cognitiveLayers.empty();

  const int outputY = showThinking ? (thinkingBot + 6) : (ipLabelBot + 6);
  int outputH = outputBot - outputY;
  if (outputH < 0) outputH = 0;

  // Store rects
  m_rcControlDock = { (width - 200) / 2, dockY, (width - 200) / 2 + 200, dockY + dockH };
  m_rcInputPill = { padding, inputY, padding + inputW, inputY + inputH };
  m_rcOutputArea = { padding, outputY, width - padding, outputY + outputH };

  if (m_inClarificationMode) {
    m_rcClarifyPanel = { padding, clarifyTop, width - padding, clarifyBot };
  } else {
    m_rcClarifyPanel = { 0, 0, 0, 0 };
  }

  if (showThinking) {
    m_rcThinkingStrip = { padding, thinkingY, width - padding, thinkingBot };
  } else {
    m_rcThinkingStrip = { 0, 0, 0, 0 };
  }

  // Defer window positions
  const int N = 16;
  HDWP hdwp = BeginDeferWindowPos(N);
  if (!hdwp) { m_inLayout = false; return; }

  auto defer = [&](HWND h, int x, int y, int w, int h_) {
    if (h) hdwp = DeferWindowPos(hdwp, h, nullptr, x, y, w, h_,
                                 SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOREDRAW);
  };

  const int ctrlW = 34, ctrlH = 34, ctrlGap = 14;
  const int ctrlStartX = m_rcControlDock.left + 11;
  const int ctrlY = m_rcControlDock.top + 4;

  defer(m_hStateLabel, padding + 24, stateY, width - padding * 2 - 48, headerH);
  defer(m_hBtnClose, width - padding - 20, stateY, 20, 20);
  defer(m_hBtnMic, ctrlStartX, ctrlY, ctrlW, ctrlH);
  defer(m_hBtnSpeaker, ctrlStartX + (ctrlW + ctrlGap) * 1, ctrlY, ctrlW, ctrlH);
  defer(m_hBtnCamera, ctrlStartX + (ctrlW + ctrlGap) * 2, ctrlY, ctrlW, ctrlH);
  defer(m_hBtnScreen, ctrlStartX + (ctrlW + ctrlGap) * 3, ctrlY, ctrlW, ctrlH);
  defer(m_hIpLabel, padding, ipLabelY, width - padding * 2, ipLabelH);
  defer(m_hEditInput, padding + 12, inputY + 8, inputW - 24, inputH - 16);
  defer(m_hBtnSend, width - padding - btnW, inputY, btnW, inputH);

  if (m_safetyVetoActive) {
    defer(m_hBtnSafetyOK, padding + 10, stateY + headerH + 60, 180, 26);
    defer(m_hBtnSafetyCancel, padding + 200, stateY + headerH + 60, 80, 26);
    ShowWindow(m_hBtnSafetyOK, SW_SHOW);
    ShowWindow(m_hBtnSafetyCancel, SW_SHOW);
  } else {
    ShowWindow(m_hBtnSafetyOK, SW_HIDE);
    ShowWindow(m_hBtnSafetyCancel, SW_HIDE);
  }

  if (outputH > 0) {
    defer(m_hEditOutput, padding + 10, outputY + 8, width - padding * 2 - 20, outputH - 16);
  }

  EndDeferWindowPos(hdwp);
  // FIXED: RDW_ALLCHILDREN caused a repaint cascade — every child window triggered
  // WM_PAINT which in turn triggered more invalidations. A simple InvalidateRect
  // on the parent is sufficient; child controls redraw themselves when needed.
  InvalidateRect(m_hwnd, nullptr, FALSE);
  m_inLayout = false;
}

// ── Server URL ───────────────────────────────────────────────────────────────

void PresenceShell::setServerUrl(const std::string& url) {
  m_serverUrl = url;
  if (m_hIpLabel) {
    SetWindowTextA(m_hIpLabel, url.empty() ? "Server offline" : url.c_str());
    InvalidateRect(m_hIpLabel, nullptr, TRUE);
  }
}

void PresenceShell::invalidateShell() {
  if (!m_hwnd) return;
  InvalidateRect(m_hwnd, nullptr, FALSE);
}

// ── Phase 5 APIs ─────────────────────────────────────────────────────────────

void PresenceShell::show_clarification(const std::string& question) {
  m_inClarificationMode = true;
  m_clarificationQuestion = question;

  // Dim history text
  SendMessageA(m_hEditOutput, EM_SETSEL, 0, -1);
  CHARFORMAT2A cfDim = { sizeof(cfDim) };
  cfDim.dwMask = CFM_COLOR | CFM_BACKCOLOR;
  cfDim.crTextColor = DIM_TEXT;
  cfDim.crBackColor = DARK_BG;
  SendMessageA(m_hEditOutput, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cfDim);
  SendMessageA(m_hEditOutput, EM_SETSEL, -1, -1);

  EnableWindow(m_hEditInput, TRUE);

  RECT rc{};
  GetClientRect(m_hwnd, &rc);
  layoutChildren(rc.right - rc.left, rc.bottom - rc.top);
  invalidateShell();
  SetFocus(m_hEditInput);
}

void PresenceShell::show_response(const std::string& text, const std::string& tone_hint) {
  if (!m_hEditOutput) return;
  EnableWindow(m_hEditOutput, TRUE);

  // Restore normal history colors
  SendMessageA(m_hEditOutput, EM_SETSEL, 0, -1);
  CHARFORMAT2A cfNorm = { sizeof(cfNorm) };
  cfNorm.dwMask = CFM_COLOR | CFM_BACKCOLOR;
  cfNorm.crTextColor = NORMAL_TEXT;
  cfNorm.crBackColor = DARK_BG;
  SendMessageA(m_hEditOutput, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cfNorm);
  SendMessageA(m_hEditOutput, EM_SETSEL, -1, -1);

  std::string processedText = text;
  COLORREF textColor = RGB(210, 215, 230);
  COLORREF backColor = RGB(16, 16, 18);
  bool isBold = false;
  bool isUrgent = false;

  if (tone_hint == "frustrated") {
    textColor = RGB(235, 75, 75);
    std::string shortText;
    std::string currentLine;
    std::stringstream ss(text);
    std::string word;
    while (ss >> word) {
      if (currentLine.length() + word.length() + 1 > 32) {
        shortText += currentLine + "\r\n";
        currentLine = word;
      } else {
        if (!currentLine.empty()) currentLine += " ";
        currentLine += word;
      }
    }
    if (!currentLine.empty()) shortText += currentLine;
    processedText = shortText;
  } else if (tone_hint == "urgent") {
    textColor = RGB(240, 200, 50);
    isBold = true;
    isUrgent = true;
  } else if (tone_hint == "curious") {
    processedText = text + " (more detail available)";
  }

  {
    std::lock_guard<std::mutex> lock(m_session.historyMutex);
    m_session.history.push_back({"Yuki", processedText, false});
  }

  const std::string header = "Yuki:\r\n";
  const std::string body = processedText + "\r\n\r\n";

  int len = GetWindowTextLengthA(m_hEditOutput);
  SendMessageA(m_hEditOutput, EM_SETSEL, len, len);

  CHARFORMAT2A cfHeader = { sizeof(cfHeader) };
  cfHeader.dwMask = CFM_COLOR | CFM_BOLD | CFM_BACKCOLOR;
  cfHeader.crTextColor = RGB(0, 200, 185);
  cfHeader.crBackColor = RGB(16, 16, 18);
  cfHeader.dwEffects = CFE_BOLD;
  SendMessageA(m_hEditOutput, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cfHeader);
  SendMessageA(m_hEditOutput, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(header.c_str()));

  len = GetWindowTextLengthA(m_hEditOutput);
  SendMessageA(m_hEditOutput, EM_SETSEL, len, len);

  CHARFORMAT2A cfBody = { sizeof(cfBody) };
  cfBody.dwMask = CFM_COLOR | CFM_BOLD | CFM_BACKCOLOR;
  cfBody.crTextColor = textColor;
  cfBody.dwEffects = (isBold ? CFE_BOLD : 0);
  cfBody.crBackColor = isUrgent ? RGB(60, 50, 10) : RGB(16, 16, 18);
  SendMessageA(m_hEditOutput, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cfBody);
  SendMessageA(m_hEditOutput, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(body.c_str()));
  SendMessageA(m_hEditOutput, EM_SCROLLCARET, 0, 0);

  m_lastToneUrgent = isUrgent;
  invalidateShell();
}

void PresenceShell::show_safety_warning(const std::string& reason) {
  m_safetyVetoActive = true;
  m_safetyReason = reason;
  EnableWindow(m_hEditInput, FALSE);
  invalidateShell();
  RECT rc{};
  GetClientRect(m_hwnd, &rc);
  layoutChildren(rc.right - rc.left, rc.bottom - rc.top);
}

// ── NEW: Cognitive Thinking Layers ───────────────────────────────────────────

void PresenceShell::setCognitiveLayers(const std::vector<CognitiveLayer>& layers) {
  // FIXED: This may be called from a background inference thread (TurnCoordinator).
  // Win32 layout and paint calls MUST run on the UI thread.  Store the data and
  // post a WM_APP_REFRESH_CHAT so the message loop picks it up safely.
  {
    std::lock_guard<std::mutex> lk(m_layersMutex_);
    m_cognitiveLayers   = layers;
    m_showThinkingStrip = !layers.empty();
  }
  if (m_hwnd) PostMessageA(m_hwnd, WM_APP_REFRESH_CHAT, 0, 0);
}

void PresenceShell::clearCognitiveLayers() {
  // FIXED: Same cross-thread safety fix as setCognitiveLayers.
  {
    std::lock_guard<std::mutex> lk(m_layersMutex_);
    m_cognitiveLayers.clear();
    m_showThinkingStrip = false;
  }
  if (m_hwnd) PostMessageA(m_hwnd, WM_APP_REFRESH_CHAT, 0, 0);
}

// ── Drawing Helpers ──────────────────────────────────────────────────────────

std::wstring PresenceShell::toWString(const std::string& s) {
  return std::wstring(s.begin(), s.end());
}

Gdiplus::Color PresenceShell::getLayerColor(int layerIdx, float activity, float pulse) const {
  static const Gdiplus::Color baseColors[5] = {
    Gdiplus::Color(255, 78,  205, 196),  // Sense  — teal
    Gdiplus::Color(255, 150, 206, 180),  // Recall — sage
    Gdiplus::Color(255, 255, 234, 167),  // Think  — amber
    Gdiplus::Color(255, 221, 160, 221),  // Choose — plum
    Gdiplus::Color(255, 116, 185, 255)   // Speak  — sky
  };
  if (layerIdx < 0 || layerIdx >= 5) return Gdiplus::Color(100, 45, 52, 54);
  if (activity < 0.05f) return Gdiplus::Color(90, 45, 52, 54);

  const auto& base = baseColors[layerIdx];
  BYTE a = static_cast<BYTE>(90 + 165 * activity * (0.65f + 0.35f * pulse));
  if (a > 255) a = 255;
  return Gdiplus::Color(a, base.GetR(), base.GetG(), base.GetB());
}

void PresenceShell::drawThinkingStrip(Gdiplus::Graphics& g, const RECT& rc) {
  if (rc.right <= rc.left || rc.bottom <= rc.top) return;

  Gdiplus::GraphicsPath path;
  addRoundedRectToPath(path, rc, 8);

  // Glass background
  Gdiplus::SolidBrush bg(Gdiplus::Color(180, 22, 24, 26));
  g.FillPath(&bg, &path);

  Gdiplus::Pen border(Gdiplus::Color(80, 0, 180, 165), 1.0f);
  g.DrawPath(&border, &path);

  const int layerCount = static_cast<int>(m_cognitiveLayers.size());
  if (layerCount == 0) return;

  const int margin = 8;
  const int pillGap = 6;
  const int availW = (rc.right - rc.left) - margin * 2;
  const int pillW = (availW - (layerCount - 1) * pillGap) / layerCount;
  const int pillH = (rc.bottom - rc.top) - 8;
  const int pillY = rc.top + 4;

  float pulse = (std::sin(GetTickCount() * 0.004f) + 1.0f) * 0.5f;

  for (int i = 0; i < layerCount; ++i) {
    const int pillX = rc.left + margin + i * (pillW + pillGap);
    RECT pillRc = { pillX, pillY, pillX + pillW, pillY + pillH };

    Gdiplus::GraphicsPath pillPath;
    addRoundedRectToPath(pillPath, pillRc, pillH / 2);

    auto col = getLayerColor(i, m_cognitiveLayers[i].activity, pulse);
    Gdiplus::SolidBrush pillBrush(col);
    g.FillPath(&pillBrush, &pillPath);

    Gdiplus::Color borderCol = m_cognitiveLayers[i].activity > 0.25f
      ? Gdiplus::Color(200, 255, 255, 255)
      : Gdiplus::Color(80, 80, 80, 85);
    Gdiplus::Pen pillPen(borderCol, 1.0f);
    g.DrawPath(&pillPen, &pillPath);

    // Label
    std::wstring name = toWString(m_cognitiveLayers[i].name);
    Gdiplus::Font font(L"Segoe UI", 7, Gdiplus::FontStyleBold);
    Gdiplus::SolidBrush textBrush(m_cognitiveLayers[i].activity > 0.25f
      ? Gdiplus::Color(255, 255, 255, 255)
      : Gdiplus::Color(160, 160, 160, 165));
    Gdiplus::StringFormat fmt;
    fmt.SetAlignment(Gdiplus::StringAlignmentCenter);
    fmt.SetLineAlignment(Gdiplus::StringAlignmentCenter);
    Gdiplus::RectF textRect(static_cast<float>(pillX), static_cast<float>(pillY),
                             static_cast<float>(pillW), static_cast<float>(pillH));
    g.DrawString(name.c_str(), -1, &font, textRect, &fmt, &textBrush);
  }

  // Pipeline connectors
  Gdiplus::Pen linePen(Gdiplus::Color(50, 80, 80, 85), 1.0f);
  for (int i = 0; i < layerCount - 1; ++i) {
    int x1 = rc.left + margin + (i + 1) * pillW + i * pillGap;
    int x2 = x1 + pillGap;
    int y = rc.top + (rc.bottom - rc.top) / 2;
    g.DrawLine(&linePen, x1, y, x2, y);
  }
}

void PresenceShell::drawClarificationPanel(Gdiplus::Graphics& g, const RECT& rc) {
  if (rc.right <= rc.left || rc.bottom <= rc.top) return;

  Gdiplus::GraphicsPath path;
  addRoundedRectToPath(path, rc, 10);

  Gdiplus::SolidBrush bg(Gdiplus::Color(210, 0, 48, 45));
  g.FillPath(&bg, &path);

  Gdiplus::Pen border(Gdiplus::Color(255, 0, 200, 185), 1.5f);
  g.DrawPath(&border, &path);

  // Header
  Gdiplus::Font headerFont(L"Segoe UI", 10, Gdiplus::FontStyleBold);
  Gdiplus::SolidBrush headerBrush(Gdiplus::Color(255, 0, 220, 200));
  Gdiplus::RectF headerRect(static_cast<float>(rc.left + 14), static_cast<float>(rc.top + 10),
                            static_cast<float>(rc.right - rc.left - 28), 18);
  g.DrawString(L"Clarification Required", -1, &headerFont, headerRect, nullptr, &headerBrush);

  // Body
  std::wstring text = toWString(m_clarificationQuestion);
  Gdiplus::Font bodyFont(L"Segoe UI", 10, Gdiplus::FontStyleRegular);
  Gdiplus::SolidBrush bodyBrush(Gdiplus::Color(255, 230, 230, 235));
  Gdiplus::RectF bodyRect(static_cast<float>(rc.left + 14), static_cast<float>(rc.top + 30),
                          static_cast<float>(rc.right - rc.left - 28),
                          static_cast<float>(rc.bottom - rc.top - 40));
  g.DrawString(text.c_str(), -1, &bodyFont, bodyRect, nullptr, &bodyBrush);
}

void PresenceShell::drawSafetyBanner(Gdiplus::Graphics& g, const RECT& rc) {
  if (rc.right <= rc.left || rc.bottom <= rc.top) return;

  Gdiplus::GraphicsPath path;
  addRoundedRectToPath(path, rc, 8);

  Gdiplus::SolidBrush bg(Gdiplus::Color(200, 90, 25, 25));
  g.FillPath(&bg, &path);

  Gdiplus::Pen border(Gdiplus::Color(255, 220, 50, 50), 1.2f);
  g.DrawPath(&border, &path);

  std::wstring text = L"Safety Warning:\n" + toWString(m_safetyReason);
  Gdiplus::Font font(L"Segoe UI", 10, Gdiplus::FontStyleBold);
  Gdiplus::SolidBrush brush(Gdiplus::Color(255, 255, 255, 255));
  Gdiplus::RectF rect(static_cast<float>(rc.left + 12), static_cast<float>(rc.top + 10),
                      static_cast<float>(rc.right - rc.left - 24),
                      static_cast<float>(rc.bottom - rc.top - 20));
  g.DrawString(text.c_str(), -1, &font, rect, nullptr, &brush);
}

void PresenceShell::drawGlassBorders(Gdiplus::Graphics& g) {
  if (m_rcControlDock.right > m_rcControlDock.left) {
    Gdiplus::GraphicsPath path;
    addRoundedRectToPath(path, m_rcControlDock, 22);
    Gdiplus::Pen pen(Gdiplus::Color(35, 255, 255, 255), 1.0f);
    g.DrawPath(&pen, &path);
  }
  if (m_rcOutputArea.right > m_rcOutputArea.left) {
    Gdiplus::GraphicsPath path;
    addRoundedRectToPath(path, m_rcOutputArea, 12);
    Gdiplus::Pen pen(m_lastToneUrgent
      ? Gdiplus::Color(255, 240, 200, 50)
      : Gdiplus::Color(55, 220, 210, 190),
      m_lastToneUrgent ? 2.0f : 1.0f);
    g.DrawPath(&pen, &path);
  }
  if (m_rcInputPill.right > m_rcInputPill.left) {
    Gdiplus::GraphicsPath path;
    addRoundedRectToPath(path, m_rcInputPill, 14);
    Gdiplus::Pen pen(Gdiplus::Color(65, 220, 210, 190), 1.0f);
    g.DrawPath(&pen, &path);
  }
}

// ── Window Procedure ─────────────────────────────────────────────────────────

LRESULT CALLBACK PresenceShell::windowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  PresenceShell *self = nullptr;
  if (uMsg == WM_NCCREATE) {
    auto *create = reinterpret_cast<CREATESTRUCT *>(lParam);
    self = reinterpret_cast<PresenceShell *>(create->lpCreateParams);
    SetWindowLongPtrA(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    self->m_hwnd = hwnd;
  } else {
    self = reinterpret_cast<PresenceShell *>(GetWindowLongPtrA(hwnd, GWLP_USERDATA));
  }
  if (self) return self->handleMessage(hwnd, uMsg, wParam, lParam);
  return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

LRESULT PresenceShell::handleMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  switch (uMsg) {
  case WM_CREATE:
    SetTimer(hwnd, 1, 500, nullptr);   // mic animation
    SetTimer(hwnd, 2, 80, nullptr);   // thinking pulse (~12 fps)
    return 0;

  case WM_TIMER:
    if (wParam == 1 && m_hBtnMic) InvalidateRect(m_hBtnMic, nullptr, FALSE);
    if (wParam == 2) {
      // Only repaint the thinking strip when it is visible and has active layers.
      // Firing unconditionally at 80ms was the primary cause of continuous flickering.
      bool show = false;
      bool hasActive = false;
      {
        std::lock_guard<std::mutex> lk(m_layersMutex_);
        show = m_showThinkingStrip && !m_cognitiveLayers.empty();
        if (show) {
          for (const auto& l : m_cognitiveLayers)
            if (l.activity > 0.05f) { hasActive = true; break; }
        }
      }
      if (show && hasActive) {
        // Invalidate only the thinking-strip rect — not the whole window.
        if (m_rcThinkingStrip.right > m_rcThinkingStrip.left)
          InvalidateRect(m_hwnd, &m_rcThinkingStrip, FALSE);
        updateStateLabel();
      }
    }
    return 0;

  case WM_APP_REFRESH_CHAT:
    if (m_syncCb) m_syncCb();
    updateStateLabel();
    invalidateShell();
    return 0;

  case WM_APP_SET_VOICE_DRAFT: {
    std::unique_ptr<std::string> text(reinterpret_cast<std::string *>(lParam));
    if (text) setVoiceDraftText(*text);
    return 0;
  }

  case WM_APP_CLEAR_VOICE_DRAFT:
    clearVoiceDraftText();
    return 0;

  case WM_APP_COMMIT_VOICE_DRAFT:
    commitVoiceDraftText();
    return 0;

  case WM_COMMAND: {
    const WORD id = LOWORD(wParam);
    const WORD code = HIWORD(wParam);

    if (id == 301 && code == BN_CLICKED) {
      m_safetyVetoActive = false;
      ShowWindow(m_hBtnSafetyOK, SW_HIDE);
      ShowWindow(m_hBtnSafetyCancel, SW_HIDE);
      EnableWindow(m_hEditInput, TRUE);
      invalidateShell();
      RECT rc{}; GetClientRect(hwnd, &rc);
      layoutChildren(rc.right - rc.left, rc.bottom - rc.top);
      return 0;
    }
    if (id == 302 && code == BN_CLICKED) {
      m_safetyVetoActive = false;
      ShowWindow(m_hBtnSafetyOK, SW_HIDE);
      ShowWindow(m_hBtnSafetyCancel, SW_HIDE);
      EnableWindow(m_hEditInput, TRUE);
      SetWindowTextA(m_hEditInput, "");
      invalidateShell();
      RECT rc{}; GetClientRect(hwnd, &rc);
      layoutChildren(rc.right - rc.left, rc.bottom - rc.top);
      return 0;
    }
    if (id == 104 && code == BN_CLICKED) { onSend(); return 0; }
    if (id == 105 && code == BN_CLICKED) { closeShell(false); return 0; }
    if (id == 103 && code == EN_CHANGE) {
      if (!m_inLayout) {
        RECT rc{}; GetClientRect(hwnd, &rc);
        layoutChildren(rc.right - rc.left, rc.bottom - rc.top);
      }
      return 0;
    }
    if (id == 201 && code == BN_CLICKED) {
      if (m_subsystems) m_subsystems->toggleMic();
      invalidateShell(); if (m_subsystemCb) m_subsystemCb();
      return 0;
    }
    if (id == 202 && code == BN_CLICKED) {
      if (m_subsystems) m_subsystems->toggleSpeaker();
      invalidateShell(); if (m_subsystemCb) m_subsystemCb();
      return 0;
    }
    if (id == 203 && code == BN_CLICKED) {
      if (m_subsystems) m_subsystems->toggleCamera();
      invalidateShell(); if (m_subsystemCb) m_subsystemCb();
      return 0;
    }
    if (id == 204 && code == BN_CLICKED) {
      if (m_subsystems) m_subsystems->toggleScreen();
      invalidateShell(); if (m_subsystemCb) m_subsystemCb();
      return 0;
    }
    return 0;
  }

  case WM_SIZE:
    layoutChildren(LOWORD(lParam), HIWORD(lParam));
    return 0;

  case WM_ERASEBKGND:
    return 1;

  case WM_MOUSEMOVE: {
    POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
    bool wasHovered = m_thinkingStripHovered;
    m_thinkingStripHovered = PtInRect(&m_rcThinkingStrip, pt);
    if (m_thinkingStripHovered && !wasHovered) {
      TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
      TrackMouseEvent(&tme);
      invalidateShell();
    } else if (!m_thinkingStripHovered && wasHovered) {
      invalidateShell();
    }
    return 0;
  }

  case WM_MOUSELEAVE:
    m_thinkingStripHovered = false;
    invalidateShell();
    return 0;

  case WM_PAINT: {
    PAINTSTRUCT ps{};
    HDC hdcScreen = BeginPaint(hwnd, &ps);

    RECT clientRc{};
    GetClientRect(hwnd, &clientRc);
    const int W = clientRc.right - clientRc.left;
    const int H = clientRc.bottom - clientRc.top;

    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    HBITMAP hBmp = CreateCompatibleBitmap(hdcScreen, W, H);
    HGDIOBJ hOld = SelectObject(hdcMem, hBmp);

    // Background
    HBRUSH bgBrush = CreateSolidBrush(RGB(12, 12, 14));
    FillRect(hdcMem, &clientRc, bgBrush);
    DeleteObject(bgBrush);

    {
      Gdiplus::Graphics g(hdcMem);
      g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
      g.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAlias);

      // Subtle acrylic gradient overlay
      Gdiplus::LinearGradientBrush grad(
        Gdiplus::Rect(0, 0, W, H / 3),
        Gdiplus::Color(12, 255, 255, 255),
        Gdiplus::Color(0, 255, 255, 255),
        Gdiplus::LinearGradientModeVertical);
      g.FillRectangle(&grad, 0, 0, W, H / 3);

      // Top highlight line
      Gdiplus::Pen topLine(Gdiplus::Color(25, 255, 255, 255), 1.0f);
      g.DrawLine(&topLine, 0, 0, W, 0);

      drawGlassBorders(g);

      if (m_safetyVetoActive) {
        const int padding = 16;
        const int headerH = 24;
        RECT rcBanner = { padding, padding + headerH + 4, W - padding, padding + headerH + 96 };
        drawSafetyBanner(g, rcBanner);
      }

      if (m_inClarificationMode &&
          m_rcClarifyPanel.right > m_rcClarifyPanel.left &&
          m_rcClarifyPanel.bottom > m_rcClarifyPanel.top) {
        drawClarificationPanel(g, m_rcClarifyPanel);
      }

      if (m_showThinkingStrip &&
          m_rcThinkingStrip.right > m_rcThinkingStrip.left &&
          m_rcThinkingStrip.bottom > m_rcThinkingStrip.top) {
        drawThinkingStrip(g, m_rcThinkingStrip);
      }
    }

    BitBlt(hdcScreen, 0, 0, W, H, hdcMem, 0, 0, SRCCOPY);
    SelectObject(hdcMem, hOld);
    DeleteObject(hBmp);
    DeleteDC(hdcMem);

    EndPaint(hwnd, &ps);
    return 0;
  }

  case WM_DRAWITEM: {
    auto *pdis = reinterpret_cast<LPDRAWITEMSTRUCT>(lParam);
    if (!pdis) break;

    // Safety Banner Buttons
    if (pdis->CtlID == 301 || pdis->CtlID == 302) {
      HDC hdc = pdis->hDC;
      RECT rc = pdis->rcItem;
      bool isPressed = (pdis->itemState & ODS_SELECTED) != 0;

      Gdiplus::Graphics g(hdc);
      g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
      g.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAlias);

      {
        POINT ptOff = {0, 0};
        MapWindowPoints(pdis->hwndItem, hwnd, &ptOff, 1);
        OffsetWindowOrgEx(hdc, -ptOff.x, -ptOff.y, nullptr);
        SendMessage(hwnd, WM_PRINTCLIENT, reinterpret_cast<WPARAM>(hdc), PRF_CLIENT);
        OffsetWindowOrgEx(hdc, ptOff.x, ptOff.y, nullptr);
      }

      Gdiplus::Color bg = (pdis->CtlID == 301)
        ? (isPressed ? Gdiplus::Color(255, 180, 40, 40) : Gdiplus::Color(255, 220, 50, 50))
        : (isPressed ? Gdiplus::Color(255, 80, 80, 85) : Gdiplus::Color(255, 110, 110, 115));

      Gdiplus::GraphicsPath path;
      addRoundedRectToPath(path, rc, 6);
      Gdiplus::SolidBrush br(bg);
      g.FillPath(&br, &path);

      std::wstring text = (pdis->CtlID == 301) ? L"I understand, continue" : L"Cancel";
      Gdiplus::Font font(L"Segoe UI", 9, Gdiplus::FontStyleBold);
      Gdiplus::SolidBrush textBr(Gdiplus::Color(255, 255, 255, 255));
      Gdiplus::StringFormat fmt;
      fmt.SetAlignment(Gdiplus::StringAlignmentCenter);
      fmt.SetLineAlignment(Gdiplus::StringAlignmentCenter);
      Gdiplus::RectF rect(rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top);
      g.DrawString(text.c_str(), -1, &font, rect, &fmt, &textBr);
      return TRUE;
    }

    // Icon Buttons 201-204
    if (pdis->CtlID >= 201 && pdis->CtlID <= 204) {
      HDC hdcScreen = pdis->hDC;
      RECT rc = pdis->rcItem;
      const bool isPressed = (pdis->itemState & ODS_SELECTED) != 0;
      const int w = rc.right - rc.left;
      const int h = rc.bottom - rc.top;
      const int cx = w / 2;
      const int cy = h / 2;

      HDC hdc = CreateCompatibleDC(hdcScreen);
      HBITMAP hBmp = CreateCompatibleBitmap(hdcScreen, w, h);
      HGDIOBJ hOld = SelectObject(hdc, hBmp);

      {
        Gdiplus::Graphics g(hdc);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        g.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAlias);

        {
          POINT ptOff = {0, 0};
          MapWindowPoints(pdis->hwndItem, hwnd, &ptOff, 1);
          OffsetWindowOrgEx(hdc, -ptOff.x, -ptOff.y, nullptr);
          SendMessage(hwnd, WM_PRINTCLIENT, reinterpret_cast<WPARAM>(hdc), PRF_CLIENT);
          OffsetWindowOrgEx(hdc, ptOff.x, ptOff.y, nullptr);
        }

        SubsystemStatus status{};
        bool hasStatus = false;
        if (m_subsystems) {
          hasStatus = true;
          if (pdis->CtlID == 201) status = m_subsystems->getStatus(SubsystemName::EAR);
          else if (pdis->CtlID == 202) status = m_subsystems->getStatus(SubsystemName::MOUTH);
          else if (pdis->CtlID == 203) status = m_subsystems->getStatus(SubsystemName::WORLD_EYE);
          else if (pdis->CtlID == 204) status = m_subsystems->getStatus(SubsystemName::SCREEN_EYE);
          else hasStatus = false;
        }

        bool isUnavailable = false;
        bool isActive = false;
        if (hasStatus) {
          isUnavailable = (!status.available ||
                           status.runtimeState == SubsystemRuntimeState::UNAVAILABLE ||
                           status.runtimeState == SubsystemRuntimeState::FAILED);
          isActive = (!isUnavailable && status.active &&
                      (status.runtimeState == SubsystemRuntimeState::RUNNING ||
                       status.runtimeState == SubsystemRuntimeState::STARTING));
        } else {
          isActive = (pdis->CtlID != 203);
          isUnavailable = false;
        }

        bool isDecoding = false;
        bool isListening = false;
        if (pdis->CtlID == 201 && m_subsystems) {
          SttState stt = m_subsystems->getSttState();
          isDecoding = (stt == SttState::DECODING);
          isListening = (stt == SttState::LISTENING || stt == SttState::CAPTURING_UTTERANCE);
        }

        Gdiplus::Color bgFill, border, iconCol;
        bool drawSlash = false;

        if (isUnavailable) {
          bgFill = isPressed ? Gdiplus::Color(80, 200, 60, 60) : Gdiplus::Color(50, 180, 50, 50);
          border = Gdiplus::Color(120, 200, 70, 70);
          iconCol = Gdiplus::Color(180, 200, 80, 80);
          drawSlash = true;
        } else if (!isActive) {
          bgFill = isPressed ? Gdiplus::Color(70, 160, 160, 165) : Gdiplus::Color(40, 130, 130, 135);
          border = Gdiplus::Color(80, 160, 160, 168);
          iconCol = Gdiplus::Color(200, 160, 160, 168);
          drawSlash = true;
        } else if (pdis->CtlID == 201 && (isDecoding || isListening)) {
          bgFill = isDecoding ? Gdiplus::Color(160, 210, 150, 10) : Gdiplus::Color(100, 190, 130, 10);
          border = isDecoding ? Gdiplus::Color(240, 230, 170, 30) : Gdiplus::Color(180, 200, 150, 20);
          iconCol = Gdiplus::Color(255, 255, 235, 100);
          drawSlash = false;
        } else {
          bgFill = isPressed ? Gdiplus::Color(160, 0, 170, 165) : Gdiplus::Color(110, 0, 155, 150);
          border = Gdiplus::Color(220, 0, 200, 190);
          iconCol = Gdiplus::Color(255, 255, 255, 255);
          drawSlash = false;
        }

        Gdiplus::Pen iconPen(iconCol, 1.6f);
        iconPen.SetLineCap(Gdiplus::LineCapRound, Gdiplus::LineCapRound, Gdiplus::DashCapRound);
        iconPen.SetLineJoin(Gdiplus::LineJoinRound);
        Gdiplus::SolidBrush iconBrush(iconCol);

        if (pdis->CtlID == 201) {
          g.DrawArc(&iconPen, cx - 3, cy - 7, 7, 7, 180, 180);
          g.DrawLine(&iconPen, cx - 3, cy - 3, cx - 3, cy + 1);
          g.DrawLine(&iconPen, cx + 4, cy - 3, cx + 4, cy + 1);
          g.DrawArc(&iconPen, cx - 3, cy - 2, 7, 5, 0, 180);
          Gdiplus::Pen standPen(iconCol, 1.4f);
          standPen.SetLineCap(Gdiplus::LineCapRound, Gdiplus::LineCapRound, Gdiplus::DashCapRound);
          g.DrawArc(&standPen, cx - 5, cy - 4, 11, 9, 0, 180);
          g.DrawLine(&iconPen, cx + 1, cy + 5, cx + 1, cy + 8);
          g.DrawLine(&iconPen, cx - 3, cy + 8, cx + 5, cy + 8);
          if (isDecoding) {
            Gdiplus::SolidBrush dot(Gdiplus::Color(255, 255, 220, 60));
            g.FillEllipse(&dot, cx - 1, cy - 5, 3, 3);
          }
        } else if (pdis->CtlID == 202) {
          Gdiplus::Point pts[4] = {
            Gdiplus::Point(cx - 5, cy - 3),
            Gdiplus::Point(cx - 2, cy - 3),
            Gdiplus::Point(cx + 2, cy - 6),
            Gdiplus::Point(cx + 2, cy + 6)
          };
          Gdiplus::Point pts2[4] = {
            Gdiplus::Point(cx + 2, cy + 6),
            Gdiplus::Point(cx - 2, cy + 3),
            Gdiplus::Point(cx - 5, cy + 3),
            Gdiplus::Point(cx - 5, cy - 3)
          };
          g.DrawLines(&iconPen, pts, 4);
          g.DrawLines(&iconPen, pts2, 4);
          if (isActive) {
            g.DrawArc(&iconPen, cx + 3, cy - 3, 5, 7, -50, 100);
          }
        } else if (pdis->CtlID == 203) {
          g.DrawRectangle(&iconPen, cx - 7, cy - 4, 8, 8);
          Gdiplus::Point tri[3] = {
            Gdiplus::Point(cx + 1, cy - 2),
            Gdiplus::Point(cx + 6, cy - 5),
            Gdiplus::Point(cx + 6, cy + 5)
          };
          g.DrawPolygon(&iconPen, tri, 3);
          if (isActive) {
            g.DrawEllipse(&iconPen, cx - 5, cy - 2, 4, 4);
          }
        } else if (pdis->CtlID == 204) {
          g.DrawRectangle(&iconPen, cx - 7, cy - 5, 14, 9);
          g.DrawLine(&iconPen, cx, cy + 4, cx, cy + 7);
          g.DrawLine(&iconPen, cx - 4, cy + 7, cx + 4, cy + 7);
          if (isActive) {
            Gdiplus::Pen contentPen(Gdiplus::Color(120, 255, 255, 255), 1.0f);
            g.DrawLine(&contentPen, cx - 5, cy - 2, cx + 5, cy - 2);
            g.DrawLine(&contentPen, cx - 5, cy + 1, cx + 2, cy + 1);
          }
        }

        if (drawSlash) {
          Gdiplus::Color slashCol = isUnavailable
            ? Gdiplus::Color(220, 220, 70, 70)
            : Gdiplus::Color(200, 180, 60, 60);
          Gdiplus::Pen slashPen(slashCol, 1.8f);
          slashPen.SetLineCap(Gdiplus::LineCapRound, Gdiplus::LineCapRound, Gdiplus::DashCapRound);
          g.DrawLine(&slashPen, cx - 7, cy - 7, cx + 7, cy + 7);
        }
      }

      BitBlt(hdcScreen, 0, 0, w, h, hdc, 0, 0, SRCCOPY);
      SelectObject(hdc, hOld);
      DeleteObject(hBmp);
      DeleteDC(hdc);
      return TRUE;
    }

    // Send + Close buttons (104, 105)
    if (pdis->CtlID == 104 || pdis->CtlID == 105) {
      HDC hdc = pdis->hDC;
      RECT rc = pdis->rcItem;
      const bool isPressed = (pdis->itemState & ODS_SELECTED) != 0;
      const bool isClose = (pdis->CtlID == 105);

      Gdiplus::Graphics g(hdc);
      g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);

      const int w = rc.right - rc.left;
      const int h = rc.bottom - rc.top;
      const int cx = w / 2;
      const int cy = h / 2;

      {
        POINT ptOff = {0, 0};
        MapWindowPoints(pdis->hwndItem, hwnd, &ptOff, 1);
        OffsetWindowOrgEx(hdc, -ptOff.x, -ptOff.y, nullptr);
        SendMessage(hwnd, WM_PRINTCLIENT, reinterpret_cast<WPARAM>(hdc), PRF_CLIENT);
        OffsetWindowOrgEx(hdc, ptOff.x, ptOff.y, nullptr);
      }

      if (isClose) {
        Gdiplus::Color xCol = isPressed ? Gdiplus::Color(255, 255, 60, 60)
                                        : Gdiplus::Color(255, 220, 50, 50);
        Gdiplus::Pen xPen(xCol, 2.2f);
        xPen.SetLineCap(Gdiplus::LineCapRound, Gdiplus::LineCapRound, Gdiplus::DashCapRound);
        g.DrawLine(&xPen, cx - 5, cy - 5, cx + 5, cy + 5);
        g.DrawLine(&xPen, cx - 5, cy + 5, cx + 5, cy - 5);
      } else {
        Gdiplus::Pen ap(isPressed ? Gdiplus::Color(255, 0, 200, 185)
                                  : Gdiplus::Color(220, 0, 190, 175), 2.2f);
        ap.SetLineCap(Gdiplus::LineCapRound, Gdiplus::LineCapRound, Gdiplus::DashCapRound);
        ap.SetLineJoin(Gdiplus::LineJoinRound);
        Gdiplus::Point pts3[3] = {
          Gdiplus::Point(cx - 5, cy + 2),
          Gdiplus::Point(cx, cy - 5),
          Gdiplus::Point(cx + 5, cy + 2)
        };
        g.DrawLines(&ap, pts3, 3);
        g.DrawLine(&ap, cx, cy - 5, cx, cy + 6);
      }
      return TRUE;
    }
    break;
  }

  case WM_PRINTCLIENT: {
    HDC hdcPC = reinterpret_cast<HDC>(wParam);
    RECT rcPC{};
    GetClientRect(hwnd, &rcPC);
    HBRUSH pcBg = CreateSolidBrush(RGB(12, 12, 14));
    FillRect(hdcPC, &rcPC, pcBg);
    DeleteObject(pcBg);
    {
      Gdiplus::Graphics gpc(hdcPC);
      gpc.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
      if (m_rcControlDock.right > m_rcControlDock.left) {
        Gdiplus::GraphicsPath path;
        addRoundedRectToPath(path, m_rcControlDock, 22);
        Gdiplus::Pen pen(Gdiplus::Color(35, 255, 255, 255), 1.0f);
        gpc.DrawPath(&pen, &path);
      }
      if (m_rcOutputArea.right > m_rcOutputArea.left) {
        Gdiplus::GraphicsPath path;
        addRoundedRectToPath(path, m_rcOutputArea, 12);
        Gdiplus::Pen pen(m_lastToneUrgent
          ? Gdiplus::Color(255, 240, 200, 50)
          : Gdiplus::Color(55, 220, 210, 190),
          m_lastToneUrgent ? 2.0f : 1.0f);
        gpc.DrawPath(&pen, &path);
      }
      if (m_rcInputPill.right > m_rcInputPill.left) {
        Gdiplus::GraphicsPath path;
        addRoundedRectToPath(path, m_rcInputPill, 14);
        Gdiplus::Pen pen(Gdiplus::Color(65, 220, 210, 190), 1.0f);
        gpc.DrawPath(&pen, &path);
      }
    }
    return 0;
  }

  case WM_NCHITTEST: {
    const LRESULT hit = DefWindowProcA(hwnd, uMsg, wParam, lParam);
    POINT pt{};
    pt.x = GET_X_LPARAM(lParam);
    pt.y = GET_Y_LPARAM(lParam);
    RECT rc{};
    GetWindowRect(hwnd, &rc);
    const int border = 8;
    const int dragHeight = 40;
    const bool left = pt.x < rc.left + border;
    const bool right = pt.x >= rc.right - border;
    const bool top = pt.y < rc.top + border;
    const bool bottom = pt.y >= rc.bottom - border;
    if (top && left) return HTTOPLEFT;
    if (top && right) return HTTOPRIGHT;
    if (bottom && left) return HTBOTTOMLEFT;
    if (bottom && right) return HTBOTTOMRIGHT;
    if (left) return HTLEFT;
    if (right) return HTRIGHT;
    if (top) return HTTOP;
    if (bottom) return HTBOTTOM;
    if (hit == HTCLIENT && pt.y < rc.top + dragHeight) return HTCAPTION;
    return hit;
  }

  case WM_CTLCOLORBTN: {
    HDC hdcBtn = reinterpret_cast<HDC>(wParam);
    SetBkMode(hdcBtn, TRANSPARENT);
    return reinterpret_cast<LRESULT>(GetStockObject(NULL_BRUSH));
  }

  case WM_CTLCOLOREDIT: {
    HDC hdcEdit = reinterpret_cast<HDC>(wParam);
    SetTextColor(hdcEdit, RGB(220, 222, 232));
    SetBkColor(hdcEdit, DARK_BG);
    static HBRUSH hEditBrush = CreateSolidBrush(DARK_BG);
    return reinterpret_cast<LRESULT>(hEditBrush);
  }

  case WM_CTLCOLORSTATIC: {
    HDC hdcStatic = reinterpret_cast<HDC>(wParam);
    HWND hCtrl = reinterpret_cast<HWND>(lParam);
    if (hCtrl == m_hEditOutput) {
      if (m_inClarificationMode) {
        SetTextColor(hdcStatic, DIM_TEXT);
        SetBkColor(hdcStatic, RGB(40, 40, 42));
        static HBRUSH hGrayBrush = CreateSolidBrush(RGB(40, 40, 42));
        return reinterpret_cast<LRESULT>(hGrayBrush);
      } else {
        SetTextColor(hdcStatic, NORMAL_TEXT);
        SetBkColor(hdcStatic, RGB(16, 16, 18));
        static HBRUSH hOutBrush = CreateSolidBrush(RGB(16, 16, 18));
        return reinterpret_cast<LRESULT>(hOutBrush);
      }
    }
    if (hCtrl == m_hStateLabel) {
      SetTextColor(hdcStatic, TEAL_ACCENT);
      SetBkColor(hdcStatic, RGB(12, 12, 14));
      static HBRUSH hLblBrush = CreateSolidBrush(RGB(12, 12, 14));
      return reinterpret_cast<LRESULT>(hLblBrush);
    }
    if (hCtrl == m_hIpLabel) {
      SetTextColor(hdcStatic, RGB(80, 200, 240));
      SetBkColor(hdcStatic, RGB(12, 12, 14));
      static HBRUSH hIpBrush = CreateSolidBrush(RGB(12, 12, 14));
      return reinterpret_cast<LRESULT>(hIpBrush);
    }
    break;
  }

  case WM_CLOSE:
    DestroyWindow(hwnd);
    return 0;

  case WM_DESTROY:
    PostQuitMessage(0);
    return 0;

  case WM_NCDESTROY:
    if (m_hEditInput) {
      RemovePropA(m_hEditInput, "PresenceShellPtr");
      if (m_originalInputProc) {
        SetWindowLongPtrA(m_hEditInput, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(m_originalInputProc));
      }
    }
    m_hwnd = nullptr;
    return 0;
  }

  return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

// ── Input Edit Subclass ───────────────────────────────────────────────────────

LRESULT CALLBACK PresenceShell::inputEditProc(HWND hwnd, UINT uMsg,
                                              WPARAM wParam, LPARAM lParam) {
  PresenceShell *self = static_cast<PresenceShell*>(GetPropA(hwnd, "PresenceShellPtr"));

  if (uMsg == WM_SETFOCUS || uMsg == WM_LBUTTONDOWN || uMsg == WM_CHAR || uMsg == WM_KEYDOWN) {
    if (self && self->m_focusCb) self->m_focusCb();
  }

  if (uMsg == WM_KEYDOWN && wParam == VK_RETURN) {
    if (GetKeyState(VK_SHIFT) & 0x8000) {
      if (self && self->m_originalInputProc) {
        return CallWindowProcA(self->m_originalInputProc, hwnd, uMsg, wParam, lParam);
      }
    } else {
      if (self) self->onSend();
      return 0;
    }
  }

  if (uMsg == WM_CHAR && wParam == VK_RETURN && !(GetKeyState(VK_SHIFT) & 0x8000)) {
    return 0;
  }

  if (self && self->m_originalInputProc) {
    return CallWindowProcA(self->m_originalInputProc, hwnd, uMsg, wParam, lParam);
  }
  return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}
