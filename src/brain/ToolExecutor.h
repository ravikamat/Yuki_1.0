#pragma once
// ToolExecutor.h — Actually does things on the user's laptop
// Strictly permission-gated. All actions logged. All reversible where possible.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>
#include <vector>
#include <functional>

struct LocalToolResult {
    bool        success = false;
    std::string output;          // stdout / file path / error message
    std::string toolUsed;
    int         exitCode = 0;
};

class ToolExecutor {
public:
    // ── File system ──────────────────────────────────────────────────────────
    LocalToolResult createFolder(const std::string& path);
    LocalToolResult writeFile(const std::string& path, const std::string& content);
    LocalToolResult readFile(const std::string& path);
    LocalToolResult deleteFile(const std::string& path);  // moves to Recycle Bin

    // ── Process execution ────────────────────────────────────────────────────
    // Runs cmd.exe /c <command>, captures stdout, timeoutMs=0 means no timeout
    LocalToolResult runCommand(const std::string& command, int timeoutMs = 10000);

    // Runs python <scriptPath> with optional args, captures output
    LocalToolResult runPython(const std::string& scriptPath,
                          const std::string& args = "",
                          int timeoutMs = 30000);

    // ── Browser / UI ─────────────────────────────────────────────────────────
    LocalToolResult openInBrowser(const std::string& path);  // file:// or http://
    LocalToolResult openUrl(const std::string& url);

    // ── System introspection ─────────────────────────────────────────────────
    LocalToolResult listDirectory(const std::string& path);
    LocalToolResult getSystemInfo();

    // ── Action log ───────────────────────────────────────────────────────────
    const std::vector<std::string>& actionLog() const { return log_; }

private:
    void log(const std::string& action, bool success, const std::string& detail);
    std::vector<std::string> log_;
};
