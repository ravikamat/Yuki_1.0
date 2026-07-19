#pragma once
#include <windows.h>
#include <string>
#include <gdiplus.h>
#include "AvatarRenderer.h"

class AvatarBody {
public:
    AvatarBody();
    ~AvatarBody();

    bool create(HINSTANCE instance);
    void show(int cmd_show);
    void closeView(bool quitProcess);
    
    void setState(const std::string& state);
    std::string getState() const { return m_currentState; }
    void setSpeech(const std::string& speech);
    void postRefresh();
    void renderLayeredWindow();

private:
    static LRESULT CALLBACK windowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT handleMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    
    void updateRoaming();

    HWND m_hwnd;
    ULONG_PTR m_gdiplusToken;
    
    std::string m_currentState;
    std::string m_speechBubble;
    
    int m_animationTick;
    int m_speakTime;

    // Decoupled part-based animation renderer
    AvatarRenderer m_renderer;

    // Smooth Desktop Roaming State Machine
    int m_currentX;
    int m_currentY;
    int m_targetX;
    int m_targetY;
    bool m_isMoving;
    int m_moveDelay;
    int m_stepCount;
};
