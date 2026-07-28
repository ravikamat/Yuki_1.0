#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace yuki::brain::platform {

struct ProcessResult {
    bool started{false};
    bool completed{false};
    bool timedOut{false};
    uint32_t exitCode{0};
    std::string stdoutText;
    std::string stderrText;
};

class RuntimeProcess {
public:
    RuntimeProcess() = default;
    ~RuntimeProcess();

    RuntimeProcess(const RuntimeProcess&) = delete;
    RuntimeProcess& operator=(const RuntimeProcess&) = delete;

    bool startDetached(const std::string& executable,
                       const std::vector<std::string>& arguments,
                       const std::string& workingDirectory,
                       std::string* error);

    ProcessResult runAndCapture(const std::string& executable,
                                const std::vector<std::string>& arguments,
                                const std::string& workingDirectory,
                                uint32_t timeoutMs) const;

    bool isRunning() const;
    void terminate();

private:
#ifdef _WIN32
    void* processHandle_{nullptr};
    void* threadHandle_{nullptr};
#endif
};

} // namespace yuki::brain::platform
