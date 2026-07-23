#include <cassert>
#include <string>
#include <filesystem>
#include "brain/selftest/SelfTestHarness.h"
#include "brain/security/SecuritySandbox.h"

using namespace yuki::selftest;

int main() {
    auto& sandbox = yuki::security::SecuritySandbox::instance();
    sandbox.setAllowedPrefixes({std::filesystem::temp_directory_path().string()});
    sandbox.setAllowedExtensions({"cpp", "exe", "obj", "o"});
    sandbox.setMaxCompilationsPerMinute(10);
    sandbox.setMaxFileWritesPerTurn(50);
    sandbox.resetTurnCounters();

    SelfTestHarness harness;

    // Test 1: Compile and run simple program
    TestConfig config;
    config.sourceCode = R"(
#include <iostream>
int main() {
    std::cout << "HELLO_FROM_SANDBOX" << std::endl;
    return 0;
}
)";
    config.runTimeout = std::chrono::seconds(5);
    config.compileTimeout = std::chrono::seconds(30);

    TestResult result = harness.runTest(config);
    assert(result.compiled);
    assert(result.passed);
    assert(result.exitCode == 0);
    assert(result.stdoutCapture.find("HELLO_FROM_SANDBOX") != std::string::npos);
    assert(result.errorCode.empty());

    // Test 2: Compile failure
    TestConfig badConfig;
    badConfig.sourceCode = "int main() { this is not valid c++ !@#$% return 0; }";
    badConfig.compileTimeout = std::chrono::seconds(30);
    TestResult badResult = harness.runTest(badConfig);
    assert(!badResult.compiled);
    assert(!badResult.errorCode.empty());

    // Test 3: Runtime failure
    TestConfig failConfig;
    failConfig.sourceCode = "#include <cstdlib>\nint main() { return 42; }";
    failConfig.runTimeout = std::chrono::seconds(5);
    failConfig.compileTimeout = std::chrono::seconds(30);
    TestResult failResult = harness.runTest(failConfig);
    assert(failResult.exitCode == 42);
    assert(!failResult.passed);

    // Test 4: Timeout
    TestConfig timeoutConfig;
    timeoutConfig.sourceCode = "int main() { while (true) {} return 0; }";
    timeoutConfig.runTimeout = std::chrono::seconds(2);
    timeoutConfig.compileTimeout = std::chrono::seconds(30);
    TestResult timeoutResult = harness.runTest(timeoutConfig);
    assert(!timeoutResult.passed);
    assert(timeoutResult.errorCode == "RUN_TIMEOUT");

    // Test 5: Cleanup
    std::string tempDir = harness.lastTempDirectory();
    assert(tempDir.empty() || !std::filesystem::exists(tempDir));

    return 0;
}
