// ToolExecutor.cpp — Real laptop actions, strictly logged
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include "ToolExecutor.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <chrono>
#include <iostream>

#pragma comment(lib, "shell32.lib")

// ── Logging ───────────────────────────────────────────────────────────────────

void ToolExecutor::log(const std::string& action, bool success, const std::string& detail) {
    using namespace std::chrono;
    auto ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    std::string entry = "[ToolExecutor:" + std::to_string(ms) + "] "
                      + (success ? "OK" : "FAIL") + " " + action
                      + (detail.empty() ? "" : " | " + detail);
    log_.push_back(entry);
    std::cout << entry << "\n";

    // Append to action log file
    std::ofstream f("data/traces/tool_actions.log", std::ios::app);
    if (f.is_open()) f << entry << "\n";
}

// ── File system ───────────────────────────────────────────────────────────────

ToolResult ToolExecutor::createFolder(const std::string& path) {
    ToolResult r; r.toolUsed = "CreateFolder";
    try {
        bool created = std::filesystem::create_directories(path);
        r.success = true;
        r.output  = created ? "Created: " + path : "Already exists: " + path;
    } catch (const std::exception& e) {
        r.output = std::string("Error: ") + e.what();
    }
    log("CreateFolder(" + path + ")", r.success, r.output);
    return r;
}

ToolResult ToolExecutor::writeFile(const std::string& path, const std::string& content) {
    ToolResult r; r.toolUsed = "WriteFile";
    // Ensure parent directory exists
    try {
        auto parent = std::filesystem::path(path).parent_path();
        if (!parent.empty()) std::filesystem::create_directories(parent);
    } catch (...) {}

    std::ofstream f(path);
    if (!f.is_open()) {
        r.output = "Cannot open: " + path;
        log("WriteFile(" + path + ")", false, r.output);
        return r;
    }
    f << content;
    r.success = true;
    r.output  = "Written: " + path + " (" + std::to_string(content.size()) + " bytes)";
    log("WriteFile(" + path + ")", true, r.output);
    return r;
}

ToolResult ToolExecutor::readFile(const std::string& path) {
    ToolResult r; r.toolUsed = "ReadFile";
    std::ifstream f(path);
    if (!f.is_open()) {
        r.output = "Cannot read: " + path;
        log("ReadFile(" + path + ")", false, r.output);
        return r;
    }
    r.output  = std::string((std::istreambuf_iterator<char>(f)),
                              std::istreambuf_iterator<char>());
    r.success = true;
    log("ReadFile(" + path + ")", true, std::to_string(r.output.size()) + " bytes");
    return r;
}

ToolResult ToolExecutor::deleteFile(const std::string& path) {
    ToolResult r; r.toolUsed = "DeleteFile";
    // Use SHFileOperation to move to Recycle Bin (reversible)
    std::string absPath = std::filesystem::absolute(path).string();
    // Must be double-null terminated
    std::vector<char> buf(absPath.size() + 2, 0);
    std::copy(absPath.begin(), absPath.end(), buf.begin());

    SHFILEOPSTRUCTA op{};
    op.wFunc  = FO_DELETE;
    op.pFrom  = buf.data();
    op.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_SILENT;
    int ret = SHFileOperationA(&op);
    r.success = (ret == 0);
    r.output  = r.success ? "Moved to Recycle Bin: " + path : "Failed (code " + std::to_string(ret) + ")";
    log("DeleteFile(" + path + ")", r.success, r.output);
    return r;
}

// ── Process execution ─────────────────────────────────────────────────────────

ToolResult ToolExecutor::runCommand(const std::string& command, int timeoutMs) {
    ToolResult r; r.toolUsed = "RunCommand";

    // Set up pipes for stdout capture
    HANDLE hReadPipe  = INVALID_HANDLE_VALUE;
    HANDLE hWritePipe = INVALID_HANDLE_VALUE;
    SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
        r.output = "Pipe creation failed";
        log("RunCommand", false, r.output);
        return r;
    }
    SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si{};
    si.cb          = sizeof(si);
    si.dwFlags     = STARTF_USESTDHANDLES;
    si.hStdOutput  = hWritePipe;
    si.hStdError   = hWritePipe;
    si.hStdInput   = GetStdHandle(STD_INPUT_HANDLE);

    std::string cmd = "cmd.exe /c " + command;
    PROCESS_INFORMATION pi{};

    BOOL ok = CreateProcessA(nullptr,
                              const_cast<char*>(cmd.c_str()),
                              nullptr, nullptr,
                              TRUE, CREATE_NO_WINDOW,
                              nullptr, nullptr,
                              &si, &pi);
    CloseHandle(hWritePipe);

    if (!ok) {
        CloseHandle(hReadPipe);
        r.output = "CreateProcess failed: " + command;
        log("RunCommand", false, r.output);
        return r;
    }

    // Wait with timeout
    DWORD waitRes = WaitForSingleObject(pi.hProcess,
                                         timeoutMs > 0 ? (DWORD)timeoutMs : INFINITE);

    // Read all stdout
    std::string out;
    char buf[1024];
    DWORD bytesRead;
    while (ReadFile(hReadPipe, buf, sizeof(buf)-1, &bytesRead, nullptr) && bytesRead > 0) {
        buf[bytesRead] = 0;
        out += buf;
    }
    CloseHandle(hReadPipe);

    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    r.success  = (exitCode == 0 && waitRes == WAIT_OBJECT_0);
    r.output   = out.empty() ? "(no output)" : out;
    r.exitCode = (int)exitCode;

    if (waitRes == WAIT_TIMEOUT) {
        r.output   += "\n[TIMEOUT after " + std::to_string(timeoutMs) + "ms]";
        r.success  = false;
    }

    log("RunCommand(" + command.substr(0,60) + ")", r.success,
        "exit=" + std::to_string(exitCode));
    return r;
}

ToolResult ToolExecutor::runPython(const std::string& scriptPath,
                                    const std::string& args,
                                    int timeoutMs) {
    std::string cmd = "python \"" + scriptPath + "\"";
    if (!args.empty()) cmd += " " + args;
    ToolResult r = runCommand(cmd, timeoutMs);
    r.toolUsed = "RunPython";
    return r;
}

// ── Browser / UI ──────────────────────────────────────────────────────────────

ToolResult ToolExecutor::openInBrowser(const std::string& path) {
    ToolResult r; r.toolUsed = "OpenBrowser";
    std::string url = "file:///" + std::filesystem::absolute(path).string();
    // Replace backslashes
    std::replace(url.begin(), url.end(), '\\', '/');
    return openUrl(url);
}

ToolResult ToolExecutor::openUrl(const std::string& url) {
    ToolResult r; r.toolUsed = "OpenUrl";
    HINSTANCE res = ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    r.success = ((INT_PTR)res > 32);
    r.output  = r.success ? "Opened: " + url : "ShellExecute failed for: " + url;
    log("OpenUrl(" + url.substr(0,80) + ")", r.success, "");
    return r;
}

// ── System info ───────────────────────────────────────────────────────────────

ToolResult ToolExecutor::listDirectory(const std::string& path) {
    ToolResult r; r.toolUsed = "ListDirectory";
    std::ostringstream ss;
    try {
        for (const auto& entry : std::filesystem::directory_iterator(path)) {
            ss << (entry.is_directory() ? "[DIR]  " : "[FILE] ")
               << entry.path().filename().string();
            if (entry.is_regular_file())
                ss << " (" << entry.file_size() << " bytes)";
            ss << "\n";
        }
        r.success = true;
        r.output  = ss.str().empty() ? "(empty)" : ss.str();
    } catch (const std::exception& e) {
        r.output = std::string("Error: ") + e.what();
    }
    log("ListDirectory(" + path + ")", r.success, "");
    return r;
}

ToolResult ToolExecutor::getSystemInfo() {
    ToolResult r; r.toolUsed = "GetSystemInfo";
    MEMORYSTATUSEX ms{sizeof(ms)};
    GlobalMemoryStatusEx(&ms);

    ULARGE_INTEGER freeBytes{}, totalBytes{};
    GetDiskFreeSpaceExA("C:\\", &freeBytes, &totalBytes, nullptr);

    std::ostringstream ss;
    ss << "RAM: " << ms.dwMemoryLoad << "% used ("
       << (ms.ullTotalPhys / 1024 / 1024) << " MB total, "
       << (ms.ullAvailPhys / 1024 / 1024) << " MB free)\n"
       << "Disk C: " << (freeBytes.QuadPart / 1024 / 1024 / 1024) << " GB free of "
       << (totalBytes.QuadPart / 1024 / 1024 / 1024) << " GB\n";

    r.success = true;
    r.output  = ss.str();
    log("GetSystemInfo", true, "");
    return r;
}
