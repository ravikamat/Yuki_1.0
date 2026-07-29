#include "src/brain/platform/RuntimeProcess.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace yuki::brain::platform {

RuntimeProcess::~RuntimeProcess() {
    terminate();
}

bool RuntimeProcess::startDetached(const std::string& executable,
                                   const std::vector<std::string>& arguments,
                                   const std::string& workingDirectory,
                                   std::string* error) {
    std::string cmd = "\"" + executable + "\"";
    for (const auto& arg : arguments) {
        cmd += " \"" + arg + "\"";
    }
    return startDetached(cmd, workingDirectory);
}

bool RuntimeProcess::startDetached(const std::string& commandLine,
                                   const std::string& workingDirectory) {
#ifdef _WIN32
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    std::vector<char> cmdBuffer(commandLine.begin(), commandLine.end());
    cmdBuffer.push_back('\0');

    LPCSTR workDir = workingDirectory.empty() ? NULL : workingDirectory.c_str();

    BOOL success = CreateProcessA(NULL, cmdBuffer.data(), NULL, NULL, FALSE,
                                  CREATE_NO_WINDOW, NULL, workDir, &si, &pi);
    if (!success) {
        return false;
    }

    processHandle_ = pi.hProcess;
    threadHandle_ = pi.hThread;
    processId_ = static_cast<uint32_t>(pi.dwProcessId);
    return true;
#else
    return false;
#endif
}

ProcessResult RuntimeProcess::runAndCapture(const std::string& executable,
                                            const std::vector<std::string>& arguments,
                                            const std::string& workingDirectory,
                                            uint32_t timeoutMs) const {
    ProcessResult res;
    res.started = false;
    return res;
}

bool RuntimeProcess::isRunning() const {
#ifdef _WIN32
    if (!processHandle_) return false;
    DWORD exitCode = 0;
    if (GetExitCodeProcess((HANDLE)processHandle_, &exitCode)) {
        return exitCode == STILL_ACTIVE;
    }
    return false;
#else
    return false;
#endif
}

void RuntimeProcess::terminate() {
#ifdef _WIN32
    if (processHandle_) {
        TerminateProcess((HANDLE)processHandle_, 0);
        CloseHandle((HANDLE)processHandle_);
        processHandle_ = nullptr;
    }
    if (threadHandle_) {
        CloseHandle((HANDLE)threadHandle_);
        threadHandle_ = nullptr;
    }
    processId_ = 0;
#endif
}

} // namespace yuki::brain::platform
