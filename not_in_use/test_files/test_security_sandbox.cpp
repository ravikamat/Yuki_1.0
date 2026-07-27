#include <cassert>
#include <iostream>
#include <string>
#include "brain/security/SecuritySandbox.h"
#include "brain/security/PathNormalizer.h"

using namespace yuki::security;

int main() {
    std::cout << "[TEST] Running test_security_sandbox..." << std::endl;

    auto& sandbox = SecuritySandbox::instance();

    sandbox.setAllowedPrefixes({"C:\\temp\\yuki_sandbox", "/tmp/yuki_sandbox"});
    sandbox.setDeniedPrefixes({"C:\\Windows", "/etc", "/usr"});
    sandbox.setAllowedExtensions({"cpp", "h", "txt", "json"});
    sandbox.setMaxCompilationsPerMinute(5);
    sandbox.setMaxFileWritesPerTurn(10);
    sandbox.resetTurnCounters();

    // GAP-02: PathNormalizer tests
    auto norm1 = PathNormalizer::normalize("../../etc/passwd");
    assert(!norm1.is_valid || norm1.rejection_reason.find("TRAVERSAL") != std::string::npos);

    auto norm2 = PathNormalizer::normalize("\\\\.\\C:\\secret");
    assert(!norm2.is_valid && norm2.rejection_reason == "DEVICE_PATH_INJECTION");

    std::string nullStr = "foo";
    nullStr.push_back('\0');
    nullStr += "bar.txt";
    auto norm3 = PathNormalizer::normalize(nullStr);
    assert(!norm3.is_valid && norm3.rejection_reason == "NULL_BYTE_INJECTION");

    // SecuritySandbox validation
    assert(!sandbox.validateWrite("../../etc/passwd").allowed());
    assert(!sandbox.validateWrite("C:\\temp\\yuki_sandbox\\..\\Windows\\test.cpp").allowed());
    assert(!sandbox.validateWrite("C:\\temp\\yuki_sandbox\\test.exe").allowed());

    sandbox.resetTurnCounters();
    for (int i = 0; i < 10; ++i) {
        sandbox.validateWrite("C:\\temp\\yuki_sandbox\\file" + std::to_string(i) + ".cpp");
    }
    assert(!sandbox.validateWrite("C:\\temp\\yuki_sandbox\\overflow.cpp").allowed());

    for (int i = 0; i < 5; ++i) sandbox.validateCompile();
    assert(!sandbox.validateCompile().allowed());

    assert(sandbox.validateRead("src\\brain\\inference\\PrecisionPredictor.h").allowed());

    auto trail = sandbox.getAuditTrail();
    assert(!trail.empty());

    std::cout << "[TEST] test_security_sandbox PASSED." << std::endl;
    return 0;
}
