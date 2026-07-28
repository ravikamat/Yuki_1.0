#include "src/brain/platform/RuntimeProcess.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include <iostream>
#include <sstream>

namespace yuki::brain::platform {

#ifdef _WIN32
static std::wstring utf8ToUtf16(const std::string& str) {
    if (str.empty()) return L"";
    int reqLen = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()), nullptr, 0);
    if (reqLen <= 0) return L"";
    std::wstring wstr(reqLen, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()), &wstr[0], reqLen);
    return wstr;
}

static std::string buildCommandLine(const std::string& executable, const std::vector<std::string>& arguments) {
    std::ostringstream cmd;
    cmd << "\"" << executable << "\"";
    for (const auto& arg : arguments) {
        cmd << " ";
        if (arg.find(' ') != std::string::npos || arg.find('\t') != std::string::npos) {
            cmd << "\"" << arg << "\"";
        } else {
            cmd << arg;
        }
    }
    return cmd.str();
}
#endif

RuntimeProcess::~RuntimeProcess() {
    terminate();
}

bool RuntimeProcess::startDetached(const std::string& executable,
                                   const std::vector<std::string>& arguments,
                                   const std::string& workingDirectory,
                                   std::string* error) {
#ifdef _WIN32
    terminate();

    std::string cmdLine = buildCommandLine(executable, arguments);
    std::wstring wCmdLine = utf8ToUtf16(cmdLine);
    std::wstring wWorkDir = utf8ToUtf16(workingDirectory);
    LPCWSTR pWorkDir = wWorkDir.empty() ? nullptr : wWorkDir.c_str();

    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = {};

    BOOL ok = CreateProcessW(
        nullptr,
        &wCmdLine[0],
        nullptr,
        nullptr,
        FALSE,
        CREATE_NO_WINDOW,
        nullptr,
        pWorkDir,
        &si,
        &pi
    );

    if (!ok) {
        if (error) *error = "CreateProcessW failed with error " + std::to_string(GetLastError());
        return false;
    }

    processHandle_ = pi.hProcess;
    threadHandle_ = pi.hThread;
    return true;
#else
    if (error) *error = "RuntimeProcess currently Windows-only";
    return false;
#endif
}

ProcessResult RuntimeProcess::runAndCapture(const std::string& executable,
                                             const std::vector<std::string>& arguments,
                                             const std::string& workingDirectory,
                                             uint32_t timeoutMs) const {
    ProcessResult result;
#ifdef _WIN32
    HANDLE hReadOut = nullptr;
    HANDLE hWriteOut = nullptr;

    SECURITY_ATTRIBUTES saAttr = {};
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = nullptr;

    if (!CreatePipe(&hReadOut, &hWriteOut, &saAttr, 0)) {
        result.stderrText = "Failed to create stdout pipe";
        return result;
    }
    SetHandleInformation(hReadOut, HANDLE_FLAG_INHERIT, 0);

    std::string cmdLine = buildCommandLine(executable, arguments);
    std::wstring wCmdLine = utf8ToUtf16(cmdLine);
    std::wstring wWorkDir = utf8ToUtf16(workingDirectory);
    LPCWSTR pWorkDir = wWorkDir.empty() ? nullptr : wWorkDir.c_str();

    STARTUPINFOW si = { sizeof(si) };
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hWriteOut;
    si.hStdError = hWriteOut;

    PROCESS_INFORMATION pi = {};

    BOOL created = CreateProcessW(
        nullptr,
        &wCmdLine[0],
        nullptr,
        nullptr,
        TRUE,
        CREATE_NO_WINDOW,
        nullptr,
        pWorkDir,
        &si,
        &pi
    );

    CloseHandle(hWriteOut); // Close write end in parent process so read will hit EOF when process finishes

    if (!created) {
        CloseHandle(hReadOut);
        result.stderrText = "CreateProcessW failed with error " + std::to_string(GetLastError());
        return result;
    }

    result.started = true;
    DWORD waitResult = WaitForSingleObject(pi.hProcess, timeoutMs);

    if (waitResult == WAIT_TIMEOUT) {
        result.timedOut = true;
        TerminateProcess(pi.hProcess, 1);
    } else {
        result.completed = true;
        DWORD exitCode = 0;
        if (GetExitCodeProcess(pi.hProcess, &exitCode)) {
            result.exitCode = exitCode;
        }
    }

    // Read captured stdout/stderr
    char buffer[1024];
    DWORD bytesRead = 0;
    while (ReadFile(hReadOut, buffer, sizeof(buffer) - 1, &bytesRead, nullptr) && bytesRead > 0) {
        buffer[bytesRead] = '\0';
        result.stdoutText += buffer;
    }

    CloseHandle(hReadOut);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
#else
    result.stderrText = "RuntimeProcess currently Windows-only";
#endif
    return result;
}

bool RuntimeProcess::isRunning() const {
#ifdef _WIN32
    if (!processHandle_) return false;
    DWORD exitCode = 0;
    if (GetExitCodeProcess(processHandle_, &exitCode)) {
        return exitCode == STILL_ACTIVE;
    }
#endif
    return false;
}

void RuntimeProcess::terminate() {
#ifdef _WIN32
    if (processHandle_) {
        if (isRunning()) {
            TerminateProcess(processHandle_, 0);
        }
        CloseHandle(processHandle_);
        processHandle_ = nullptr;
    }
    if (threadHandle_) {
        CloseHandle(threadHandle_);
        threadHandle_ = nullptr;
    }
#endif
}

} // namespace yuki::brain::platform
