#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <functional>
#include <mutex>
#include <gdiplus.h>

#include "SessionState.h"

class PresenceShell {
public:
    struct CognitiveLayer {
        std::string name;      // e.g. "Sense", "Recall", "Think", "Choose", "Speak"
        std::string detail;    // e.g. "Analyzing audio spectrum..."
        float activity = 0.0f; // 0.0 = idle, 1.0 = fully active
    };

    explicit PresenceShell(SessionState& session);
    ~PresenceShell();

    bool create(HINSTANCE instance);
    void show(int cmd_show);
    int run();
    void closeShell(bool quitProcess = false);

    void setState(const std::string& stateText);
    void appendMessage(const std::string& speaker, const std::string& text, bool isVoice = false);
    void rebuildHistory(const std::vector<ChatEntry>& history);
    void postRefresh();

    // ── Phase 5 additions ─────────────────────────────────────────────────────
    void show_clarification(const std::string& question);
    void show_response(const std::string& text, const std::string& tone_hint);
    void show_safety_warning(const std::string& reason);

    // ── NEW: Real-time cognitive thinking layer visualization ────────────────
    void setCognitiveLayers(const std::vector<CognitiveLayer>& layers);
    void clearCognitiveLayers();

    // Thread-safe voice draft input bridge.
    void postSetVoiceDraftText(const std::string& text);
    void postClearVoiceDraftText();
    void postCommitVoiceDraftText();

    // Optional direct UI-thread helpers.
    void setVoiceDraftText(const std::string& text);
    void clearVoiceDraftText();
    void commitVoiceDraftText();

    using ProcessCallback = std::function<std::string(const std::string&)>;
    void setProcessCallback(ProcessCallback cb) { m_processCb = std::move(cb); }

    using SyncCallback = std::function<void()>;
    void setSyncCallback(SyncCallback cb) { m_syncCb = std::move(cb); }

    using FocusCallback = std::function<void()>;
    void setFocusCallback(FocusCallback cb) { m_focusCb = std::move(cb); }

    class SubsystemControl* m_subsystems = nullptr;
    using SubsystemCallback = std::function<void()>;
    void setSubsystems(class SubsystemControl* sub) { m_subsystems = sub; }
    void setSubsystemCallback(SubsystemCallback cb) { m_subsystemCb = std::move(cb); }

    // Live server URL shown in the shell (call after MobileServer starts)
    void setServerUrl(const std::string& url);

private:
    static LRESULT CALLBACK windowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK inputEditProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    LRESULT handleMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    void onSend();
    void layoutChildren(int width, int height);
    void invalidateShell();
    void updateStateLabel();
    void applyEditTextPreservingCaret(const std::string& text);

    std::string getInputText() const;
    static std::string normalizeDraftText(const std::string& text);

    // ── Drawing helpers ───────────────────────────────────────────────────────
    void drawThinkingStrip(Gdiplus::Graphics& g, const RECT& rc);
    void drawClarificationPanel(Gdiplus::Graphics& g, const RECT& rc);
    void drawSafetyBanner(Gdiplus::Graphics& g, const RECT& rc);
    void drawGlassBorders(Gdiplus::Graphics& g);
    Gdiplus::Color getLayerColor(int layerIdx, float activity, float pulse) const;
    static std::wstring toWString(const std::string& s);

private:
    HWND m_hwnd = nullptr;
    HWND m_hStateLabel = nullptr;
    HWND m_hIpLabel    = nullptr;
    HWND m_hEditOutput = nullptr;
    HWND m_hEditInput  = nullptr;
    HWND m_hBtnSend = nullptr;
    HWND m_hBtnClose = nullptr;
    HWND m_hBtnMic = nullptr;
    HWND m_hBtnSpeaker = nullptr;
    HWND m_hBtnCamera = nullptr;
    HWND m_hBtnScreen = nullptr;

    // Safety veto controls
    HWND m_hBtnSafetyOK = nullptr;
    HWND m_hBtnSafetyCancel = nullptr;

    HFONT m_hFontState = nullptr;
    HFONT m_hFontChat = nullptr;
    HFONT m_hFontSmall = nullptr;   // NEW: for thinking strip / IP label

    std::string m_currentState = "IDLE";
    WNDPROC m_originalInputProc = nullptr;

    ProcessCallback m_processCb;
    SyncCallback m_syncCb;
    SubsystemCallback m_subsystemCb;
    FocusCallback m_focusCb;

    RECT m_rcInputPill {0, 0, 0, 0};
    RECT m_rcOutputArea {0, 0, 0, 0};
    RECT m_rcControlDock {0, 0, 0, 0};
    RECT m_rcClarifyPanel {0, 0, 0, 0};
    RECT m_rcThinkingStrip {0, 0, 0, 0};  // NEW

    ULONG_PTR m_gdiplusToken = 0;

    bool m_applyingProgrammaticInput = false;
    bool m_voiceDraftActive          = false;
    bool m_inLayout                  = false;
    std::string m_voiceDraftText;
    std::string m_inputBeforeVoiceDraft;
    std::string m_serverUrl;

    // ── Phase 5 states ────────────────────────────────────────────────────────
    bool m_inClarificationMode       = false;
    std::string m_clarificationQuestion;

    bool m_safetyVetoActive          = false;
    std::string m_safetyReason;

    bool m_lastToneUrgent            = false;

    // ── NEW: Cognitive thinking layers ───────────────────────────────────────
    std::vector<CognitiveLayer> m_cognitiveLayers;
    bool m_showThinkingStrip = false;
    bool m_thinkingStripHovered = false;
    mutable std::mutex m_layersMutex_;   // protects m_cognitiveLayers + m_showThinkingStrip

    // ── Color constants ───────────────────────────────────────────────────────
    static constexpr COLORREF DARK_BG     = RGB(16, 16, 18);
    static constexpr COLORREF DIM_TEXT    = RGB(130, 130, 135);
    static constexpr COLORREF NORMAL_TEXT = RGB(210, 215, 230);
    static constexpr COLORREF TEAL_ACCENT = RGB(0, 200, 185);

    SessionState& m_session;
};
