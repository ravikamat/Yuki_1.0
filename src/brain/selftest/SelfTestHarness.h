#pragma once
#include <string>
#include <vector>
#include <chrono>

namespace yuki::selftest {

struct TestResult {
    bool compiled = false;
    bool passed = false;
    int exitCode = -1;
    std::string stdoutCapture;
    std::string stderrCapture;
    std::string errorCode; // empty = no error. Otherwise: "COMPILE_TIMEOUT", "RUN_TIMEOUT", "COMPILE_FAIL", "SPAWN_FAIL"
    std::chrono::milliseconds compileTime{0};
    std::chrono::milliseconds runTime{0};
};

struct TestConfig {
    std::string sourceCode;
    std::chrono::seconds compileTimeout{60};
    std::chrono::seconds runTimeout{10};
    std::vector<std::string> compilerFlags;
};

class SelfTestHarness {
public:
    SelfTestHarness();

    TestResult runTest(const TestConfig& config);

    std::string lastTempDirectory() const;

private:
    std::string lastTempDir_;

    std::string generateTempDirName() const;
    bool writeSourceFile(const std::string& dir, const std::string& source) const;
    bool compileInDirectory(const std::string& dir,
                            const std::string& sourceFile,
                            const std::vector<std::string>& flags,
                            std::chrono::seconds timeout,
                            std::string& outExePath,
                            std::string& outErrorCode);
    TestResult executeInDirectory(const std::string& exePath,
                                  std::chrono::seconds timeout);
    bool cleanupDirectory(const std::string& dir) const;
};

} // namespace yuki::selftest
