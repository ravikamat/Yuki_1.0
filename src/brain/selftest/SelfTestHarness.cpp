#include "SelfTestHarness.h"
#include "brain/security/SecuritySandbox.h"
#include "brain/core/ConfigManager.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <array>
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#endif

namespace yuki::selftest {

SelfTestHarness::SelfTestHarness() = default;

std::string SelfTestHarness::lastTempDirectory() const {
    return lastTempDir_;
}

std::string SelfTestHarness::generateTempDirName() const {
    auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    return (std::filesystem::temp_directory_path() / ("yuki_sandbox_" + std::to_string(now))).string();
}

bool SelfTestHarness::writeSourceFile(const std::string& dir, const std::string& source) const {
    auto& sandbox = yuki::security::SecuritySandbox::instance();
    std::string filePath = (std::filesystem::path(dir) / "test_source.cpp").string();

    auto decision = sandbox.validateWrite(filePath);
    if (!decision.allowed()) {
        sandbox.appendAudit(decision, "write_source", filePath);
        return false;
    }

    std::ofstream ofs(filePath);
    if (!ofs) return false;
    ofs << source;
    return ofs.good();
}

bool SelfTestHarness::compileInDirectory(const std::string& dir,
                                         const std::string& sourceFile,
                                         const std::vector<std::string>& flags,
                                         std::chrono::seconds timeout,
                                         std::string& outExePath,
                                         std::string& outErrorCode)
{
    auto& sandbox = yuki::security::SecuritySandbox::instance();

    auto compileDecision = sandbox.validateCompile();
    if (!compileDecision.allowed()) {
        sandbox.appendAudit(compileDecision, "compile", dir);
        outErrorCode = "COMPILE_RATE_LIMIT";
        return false;
    }

#ifdef _WIN32
    outExePath = (std::filesystem::path(dir) / "test_binary.exe").string();
    std::ostringstream cmd;
    if (std::getenv("INCLUDE") == nullptr) {
        std::string vcvars = "vcvars64.bat";
        const char* vsdir = std::getenv("VSINSTALLDIR");
        if (vsdir) {
            vcvars = std::string(vsdir) + "\\VC\\Auxiliary\\Build\\vcvars64.bat";
        } else {
            auto hints = yuki::ConfigManager::instance().getToolPathHints("vcvars64");
            if (!hints.empty()) vcvars = hints[0];
        }
        cmd << "cmd.exe /c \"call \"" << vcvars << "\" >nul 2>&1 && cl.exe /nologo /EHsc /Fe:\"" << outExePath << "\" \"" << sourceFile << "\"";
        for (const auto& f : flags) cmd << " " << f;
        cmd << "\"";
    } else {
        cmd << "cl.exe /nologo /EHsc /Fe:\"" << outExePath << "\" \"" << sourceFile << "\"";
        for (const auto& f : flags) cmd << " " << f;
    }
#else
    outExePath = (std::filesystem::path(dir) / "test_binary").string();
    std::ostringstream cmd;
    cmd << "g++ -o \"" << outExePath << "\" \"" << sourceFile << "\"";
    for (const auto& f : flags) cmd << " " << f;
#endif

    auto start = std::chrono::steady_clock::now();
    std::array<char, 4096> buffer{};
    std::string compileOutput;

#ifdef _WIN32
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE hRead, hWrite;
    CreatePipe(&hRead, &hWrite, &sa, 0);
    SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

    PROCESS_INFORMATION pi{};
    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.hStdError = hWrite;
    si.hStdOutput = hWrite;
    si.dwFlags = STARTF_USESTDHANDLES;

    std::string cmdStr = cmd.str();
    if (!CreateProcessA(nullptr, cmdStr.data(), nullptr, nullptr, TRUE, 0, nullptr, dir.c_str(), &si, &pi)) {
        CloseHandle(hWrite);
        CloseHandle(hRead);
        outErrorCode = "SPAWN_FAIL";
        return false;
    }

    CloseHandle(hWrite);
    DWORD waitResult = WaitForSingleObject(pi.hProcess, static_cast<DWORD>(timeout.count() * 1000));
    if (waitResult == WAIT_TIMEOUT) {
        TerminateProcess(pi.hProcess, 1);
        outErrorCode = "COMPILE_TIMEOUT";
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        CloseHandle(hRead);
        return false;
    }

    DWORD readBytes = 0;
    while (ReadFile(hRead, buffer.data(), static_cast<DWORD>(buffer.size() - 1), &readBytes, nullptr) && readBytes > 0) {
        buffer[readBytes] = '\0';
        compileOutput += buffer.data();
    }

    DWORD exitCode = 1;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hRead);

    if (exitCode != 0) {
        outErrorCode = "COMPILE_FAIL";
        return false;
    }
#else
    int stdoutPipe[2];
    pipe(stdoutPipe);
    pid_t pid = fork();
    if (pid == 0) {
        close(stdoutPipe[0]);
        dup2(stdoutPipe[1], STDOUT_FILENO);
        dup2(stdoutPipe[1], STDERR_FILENO);
        close(stdoutPipe[1]);
        chdir(dir.c_str());
        std::vector<char*> args;
        args.push_back(const_cast<char*>("g++"));
        args.push_back(const_cast<char*>("-o"));
        args.push_back(const_cast<char*>(outExePath.c_str()));
        args.push_back(const_cast<char*>(sourceFile.c_str()));
        for (const auto& f : flags) args.push_back(const_cast<char*>(f.c_str()));
        args.push_back(nullptr);
        execvp("g++", args.data());
        _exit(1);
    } else if (pid < 0) {
        close(stdoutPipe[0]); close(stdoutPipe[1]);
        outErrorCode = "SPAWN_FAIL";
        return false;
    }
    close(stdoutPipe[1]);
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(stdoutPipe[0], &fds);
    struct timeval tv;
    tv.tv_sec = timeout.count();
    tv.tv_usec = 0;
    int selectResult = select(stdoutPipe[0] + 1, &fds, nullptr, nullptr, &tv);
    if (selectResult <= 0) {
        kill(pid, SIGKILL);
        outErrorCode = "COMPILE_TIMEOUT";
        close(stdoutPipe[0]);
        return false;
    }
    ssize_t count = 0;
    while ((count = read(stdoutPipe[0], buffer.data(), buffer.size() - 1)) > 0) {
        buffer[count] = '\0';
        compileOutput += buffer.data();
    }
    close(stdoutPipe[0]);
    int status = 0;
    waitpid(pid, &status, 0);
    if (WEXITSTATUS(status) != 0) {
        outErrorCode = "COMPILE_FAIL";
        return false;
    }
#endif

    return true;
}

TestResult SelfTestHarness::executeInDirectory(const std::string& exePath,
                                               std::chrono::seconds timeout)
{
    TestResult result;
    auto& sandbox = yuki::security::SecuritySandbox::instance();

    auto execDecision = sandbox.validateExecute(exePath);
    if (!execDecision.allowed()) {
        sandbox.appendAudit(execDecision, "execute", exePath);
        result.errorCode = "EXEC_DENIED";
        return result;
    }

    auto start = std::chrono::steady_clock::now();
    std::string stdoutStr, stderrStr;

#ifdef _WIN32
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    HANDLE hStdOutRead, hStdOutWrite;
    HANDLE hStdErrRead, hStdErrWrite;
    CreatePipe(&hStdOutRead, &hStdOutWrite, &sa, 0);
    CreatePipe(&hStdErrRead, &hStdErrWrite, &sa, 0);
    SetHandleInformation(hStdOutRead, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(hStdErrRead, HANDLE_FLAG_INHERIT, 0);

    PROCESS_INFORMATION pi{};
    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.hStdOutput = hStdOutWrite;
    si.hStdError = hStdErrWrite;
    si.dwFlags = STARTF_USESTDHANDLES;

    if (!CreateProcessA(nullptr, const_cast<char*>(exePath.c_str()),
                        nullptr, nullptr, TRUE, 0, nullptr, nullptr, &si, &pi)) {
        CloseHandle(hStdOutWrite); CloseHandle(hStdOutRead);
        CloseHandle(hStdErrWrite); CloseHandle(hStdErrRead);
        result.errorCode = "SPAWN_FAIL";
        return result;
    }

    CloseHandle(hStdOutWrite);
    CloseHandle(hStdErrWrite);

    DWORD waitResult = WaitForSingleObject(pi.hProcess, static_cast<DWORD>(timeout.count() * 1000));
    if (waitResult == WAIT_TIMEOUT) {
        TerminateProcess(pi.hProcess, 1);
        result.errorCode = "RUN_TIMEOUT";
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        CloseHandle(hStdOutRead);
        CloseHandle(hStdErrRead);
        return result;
    }

    std::array<char, 4096> buffer{};
    DWORD readBytes = 0;
    while (ReadFile(hStdOutRead, buffer.data(), static_cast<DWORD>(buffer.size() - 1), &readBytes, nullptr) && readBytes > 0) {
        buffer[readBytes] = '\0';
        stdoutStr += buffer.data();
    }
    while (ReadFile(hStdErrRead, buffer.data(), static_cast<DWORD>(buffer.size() - 1), &readBytes, nullptr) && readBytes > 0) {
        buffer[readBytes] = '\0';
        stderrStr += buffer.data();
    }

    DWORD exitCode = 1;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hStdOutRead);
    CloseHandle(hStdErrRead);

    result.exitCode = static_cast<int>(exitCode);
    result.stdoutCapture = stdoutStr;
    result.stderrCapture = stderrStr;
    result.runTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start);
    result.passed = (result.exitCode == 0);
#else
    int outPipe[2], errPipe[2];
    pipe(outPipe); pipe(errPipe);
    pid_t pid = fork();
    if (pid == 0) {
        close(outPipe[0]); close(errPipe[0]);
        dup2(outPipe[1], STDOUT_FILENO);
        dup2(errPipe[1], STDERR_FILENO);
        close(outPipe[1]); close(errPipe[1]);
        execl(exePath.c_str(), exePath.c_str(), nullptr);
        _exit(1);
    } else if (pid < 0) {
        close(outPipe[0]); close(outPipe[1]);
        close(errPipe[0]); close(errPipe[1]);
        result.errorCode = "SPAWN_FAIL";
        return result;
    }
    close(outPipe[1]); close(errPipe[1]);
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(outPipe[0], &fds);
    FD_SET(errPipe[0], &fds);
    int maxFd = std::max(outPipe[0], errPipe[0]) + 1;
    struct timeval tv;
    tv.tv_sec = timeout.count();
    tv.tv_usec = 0;
    int selectResult = select(maxFd, &fds, nullptr, nullptr, &tv);
    if (selectResult <= 0) {
        kill(pid, SIGKILL);
        result.errorCode = "RUN_TIMEOUT";
        close(outPipe[0]); close(errPipe[0]);
        return result;
    }
    std::array<char, 4096> buffer{};
    ssize_t count = 0;
    while ((count = read(outPipe[0], buffer.data(), buffer.size() - 1)) > 0) {
        buffer[count] = '\0';
        stdoutStr += buffer.data();
    }
    while ((count = read(errPipe[0], buffer.data(), buffer.size() - 1)) > 0) {
        buffer[count] = '\0';
        stderrStr += buffer.data();
    }
    close(outPipe[0]); close(errPipe[0]);
    int status = 0;
    waitpid(pid, &status, 0);
    result.exitCode = WEXITSTATUS(status);
    result.stdoutCapture = stdoutStr;
    result.stderrCapture = stderrStr;
    result.runTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start);
    result.passed = (result.exitCode == 0);
#endif

    return result;
}

bool SelfTestHarness::cleanupDirectory(const std::string& dir) const {
    try {
        std::filesystem::remove_all(dir);
        return true;
    } catch (...) {
        return false;
    }
}

TestResult SelfTestHarness::runTest(const TestConfig& config) {
    TestResult result;

    lastTempDir_ = generateTempDirName();
    try {
        std::filesystem::create_directories(lastTempDir_);
    } catch (...) {
        result.errorCode = "TEMP_DIR_FAIL";
        return result;
    }

    std::string sourcePath = (std::filesystem::path(lastTempDir_) / "test_source.cpp").string();
    if (!writeSourceFile(lastTempDir_, config.sourceCode)) {
        result.errorCode = "WRITE_DENIED";
        cleanupDirectory(lastTempDir_);
        return result;
    }

    std::string exePath;
    std::string compileError;
    auto compileStart = std::chrono::steady_clock::now();
    bool compiled = compileInDirectory(lastTempDir_, sourcePath, config.compilerFlags,
                                       config.compileTimeout, exePath, compileError);
    result.compileTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - compileStart);
    result.compiled = compiled;

    if (!compiled) {
        result.errorCode = compileError.empty() ? "COMPILE_FAIL" : compileError;
        cleanupDirectory(lastTempDir_);
        return result;
    }

    result = executeInDirectory(exePath, config.runTimeout);
    cleanupDirectory(lastTempDir_);
    return result;
}

} // namespace yuki::selftest
