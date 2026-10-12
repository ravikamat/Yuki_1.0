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

    // Shape Controls
    void nextShape() { m_renderer.nextShape(); m_manualOverrideTicks = 300; postRefresh(); }
    void setShape(AvatarRenderer::BotShapeId shape) { m_renderer.setShape(shape); postRefresh(); }
    const char* getShapeName() const { return m_renderer.getShapeName(m_renderer.getShape()); }

    // Emotional Drive (from Yuki's brain → avatar body expression)
    void setEmotionalDrive(float valence, float arousal, float curiosity, float fatigue, int dominantEmotion);

private:
    static LRESULT CALLBACK windowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT handleMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    
    void updateRoaming();
    void applyStateShape(const std::string& state);
    void applyEmotionShape(int dominantEmotion, float valence, float curiosity, float fatigue);

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

    // Manual shape override (right-click sets this to ~5 seconds, decays each tick)
    int m_manualOverrideTicks = 0;
};
