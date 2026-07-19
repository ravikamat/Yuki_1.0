#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
yuki_browser_agent.py
Yuki_1.0 — Browser Automation Agent

Handles browser-based tasks:
  - WhatsApp Web: find contact, send message
  - General navigation: open URLs
  - Tab finding: locate open tabs by title keyword

Protocol (JSON lines via stdin/stdout):
  C++ → Python: {"cmd":"whatsapp_msg", "contact":"Lokesh", "message":"Please call me when free"}
  C++ → Python: {"cmd":"open_url",     "url":"https://..."}
  C++ → Python: {"cmd":"find_tab",     "keyword":"WhatsApp"}
  C++ → Python: {"cmd":"quit"}

  Python → C++: {"type":"result",  "success":true,  "detail":"Message sent to Lokesh"}
  Python → C++: {"type":"result",  "success":false, "detail":"WhatsApp tab not found"}
  Python → C++: {"type":"ready"}
  Python → C++: {"type":"error",   "detail":"..."}

Dependencies: pyautogui, pyperclip, pygetwindow (all pip-installable)
If not installed, falls back to webbrowser + clipboard approach.
"""

import sys
import os
import json
import time
import subprocess
import webbrowser
import urllib.parse

# ── Try to import optional automation libs ────────────────────────────────────

try:
    import pyautogui
    import pyperclip
    HAS_PYAUTOGUI = True
except ImportError:
    HAS_PYAUTOGUI = False

try:
    import pygetwindow as gw
    HAS_PYGETWINDOW = True
except ImportError:
    HAS_PYGETWINDOW = False


def emit(obj):
    sys.stdout.write(json.dumps(obj) + "\n")
    sys.stdout.flush()


def log(msg):
    sys.stderr.write(f"[BrowserAgent] {msg}\n")
    sys.stderr.flush()


# ── Windows-specific: find window by title ────────────────────────────────────

def find_window_with_title(keyword):
    """Find a window whose title contains keyword. Returns window title or None."""
    if HAS_PYGETWINDOW:
        windows = gw.getAllWindows()
        keyword_lower = keyword.lower()
        for w in windows:
            if keyword_lower in w.title.lower():
                return w
        return None

    # Fallback: use PowerShell to list window titles
    try:
        result = subprocess.run(
            ["powershell", "-Command",
             "Get-Process | Where-Object {$_.MainWindowTitle -ne ''} | "
             "Select-Object MainWindowTitle | ConvertTo-Json"],
            capture_output=True, text=True, timeout=5
        )
        if result.returncode == 0:
            import json as _json
            titles = _json.loads(result.stdout)
            if isinstance(titles, dict): titles = [titles]
            kl = keyword.lower()
            for t in titles:
                title = t.get("MainWindowTitle", "")
                if kl in title.lower():
                    return title
    except Exception as e:
        log(f"find_window error: {e}")
    return None


# ── WhatsApp Web via pyautogui ────────────────────────────────────────────────

def send_whatsapp_pyautogui(contact: str, message: str) -> dict:
    """
    Attempt to send a WhatsApp message using pyautogui.
    Requires WhatsApp Web to already be open in the browser.
    """
    if not HAS_PYAUTOGUI:
        return {"success": False, "detail": "pyautogui not installed — using URL fallback"}

    pyautogui.FAILSAFE = True
    pyautogui.PAUSE = 0.5

    # 1. Find Edge/Chrome window with WhatsApp
    wa_window = find_window_with_title("WhatsApp")
    if wa_window is None:
        return {"success": False, "detail": "WhatsApp Web tab not found in any open browser. "
                                             "Please open WhatsApp Web in Edge first."}

    # 2. Bring window to focus
    try:
        if HAS_PYGETWINDOW and hasattr(wa_window, 'activate'):
            wa_window.activate()
            time.sleep(0.8)
    except Exception as e:
        log(f"Window activate error: {e}")

    # 3. Use Ctrl+F in WhatsApp Web to find contact
    # WhatsApp Web shortcut: Ctrl+F opens search
    try:
        pyautogui.hotkey('ctrl', 'f')
        time.sleep(0.6)
        pyautogui.hotkey('ctrl', 'a')
        pyperclip.copy(contact)
        pyautogui.hotkey('ctrl', 'v')
        time.sleep(1.5)  # wait for search results

        # Press Enter to open first matching conversation
        pyautogui.press('enter')
        time.sleep(0.8)

        # 4. Click in message box — Tab to message area then type
        pyautogui.press('escape')  # close search if still open
        time.sleep(0.3)

        # WhatsApp Web: message input is at bottom, Tab often works
        pyautogui.hotkey('ctrl', 'shift', ']')  # jump to chat if shortcut available
        time.sleep(0.3)

        # Type message using clipboard (safer than direct typing for Unicode)
        pyperclip.copy(message)
        pyautogui.hotkey('ctrl', 'v')
        time.sleep(0.4)

        # Send message
        pyautogui.press('enter')
        time.sleep(0.3)

        return {"success": True,
                "detail": f"Message sent to {contact}: '{message}'"}

    except Exception as e:
        return {"success": False, "detail": f"pyautogui error: {str(e)}"}


# ── WhatsApp via URL (fallback) ───────────────────────────────────────────────

def send_whatsapp_url_fallback(contact: str, message: str) -> dict:
    """
    Opens WhatsApp Web search for a contact via URL.
    Works without pyautogui but requires user to click Send.
    Note: wa.me requires phone number, not name.
    This opens WhatsApp Web with a pre-filled message for manual send.
    """
    encoded_msg = urllib.parse.quote(message)
    # Open WhatsApp Web with new chat (user must select contact)
    url = f"https://web.whatsapp.com/send?text={encoded_msg}"
    try:
        # Try to open in Edge specifically
        edge_path = r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe"
        if os.path.exists(edge_path):
            subprocess.Popen([edge_path, url])
        else:
            webbrowser.open(url)
        return {
            "success": True,
            "detail": f"Opened WhatsApp Web with message pre-filled. "
                      f"Please find '{contact}' and click Send."
        }
    except Exception as e:
        return {"success": False, "detail": f"Browser open failed: {str(e)}"}


# ── Main dispatch ─────────────────────────────────────────────────────────────

def handle_whatsapp_msg(contact: str, message: str) -> dict:
    """Main entry: try pyautogui first, fallback to URL."""
    log(f"WhatsApp: contact='{contact}' message='{message}'")

    # Try pyautogui automation first
    result = send_whatsapp_pyautogui(contact, message)
    if result["success"]:
        return result

    # Fallback: URL approach
    log(f"Falling back to URL: {result['detail']}")
    return send_whatsapp_url_fallback(contact, message)


def handle_open_url(url: str) -> dict:
    try:
        edge_path = r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe"
        if os.path.exists(edge_path):
            subprocess.Popen([edge_path, url])
        else:
            webbrowser.open(url)
        return {"success": True, "detail": f"Opened: {url}"}
    except Exception as e:
        return {"success": False, "detail": str(e)}


def handle_find_tab(keyword: str) -> dict:
    w = find_window_with_title(keyword)
    if w:
        title = w.title if HAS_PYGETWINDOW and hasattr(w, 'title') else str(w)
        return {"success": True, "detail": f"Found: {title}"}
    return {"success": False, "detail": f"No window found with '{keyword}'"}


# ── Main loop ─────────────────────────────────────────────────────────────────

def main():
    import io
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', line_buffering=True)

    log("Browser agent started")
    log(f"pyautogui: {HAS_PYAUTOGUI}, pygetwindow: {HAS_PYGETWINDOW}")

    emit({"type": "ready", "pyautogui": HAS_PYAUTOGUI, "pygetwindow": HAS_PYGETWINDOW})

    for raw_line in sys.stdin:
        line = raw_line.strip()
        if not line:
            continue
        try:
            cmd = json.loads(line)
            action = cmd.get("cmd", "")

            if action == "whatsapp_msg":
                contact = cmd.get("contact", "").strip()
                message = cmd.get("message", "").strip()
                if not contact or not message:
                    emit({"type": "result", "success": False, "detail": "Missing contact or message"})
                else:
                    result = handle_whatsapp_msg(contact, message)
                    emit({"type": "result", **result})

            elif action == "open_url":
                url = cmd.get("url", "").strip()
                emit({"type": "result", **handle_open_url(url)})

            elif action == "find_tab":
                keyword = cmd.get("keyword", "").strip()
                emit({"type": "result", **handle_find_tab(keyword)})

            elif action == "quit":
                log("Quit received")
                break

        except json.JSONDecodeError:
            pass
        except Exception as e:
            emit({"type": "error", "detail": str(e)})


if __name__ == "__main__":
    main()
