#pragma once
#include <windows.h>
#include <string>
#include <gdiplus.h>
#include <functional>

class DetailView {
public:
    DetailView();
    ~DetailView();

    bool create(HINSTANCE instance);
    void show(int cmd_show);
    void closeView(bool quitProcess);
    
    void setContent(const std::string& text);
    void postRefresh();
    
    void setSyncCallback(std::function<void()> cb) { m_syncCb = cb; }

private:
    static LRESULT CALLBACK windowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT handleMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    
    HWND m_hwnd;
    HWND m_hEditContent;
    HWND m_hStateLabel;
    HWND m_hBtnClose;
    
    ULONG_PTR m_gdiplusToken;
    std::function<void()> m_syncCb;
    
    std::string m_pendingContent;
};
