#include <cassert>
#include <string>
#include "brain/security/SecuritySandbox.h"

using namespace yuki::security;

int main() {
    auto& sandbox = SecuritySandbox::instance();

    sandbox.setAllowedPrefixes({"C:\\temp\\yuki_sandbox", "/tmp/yuki_sandbox"});
    sandbox.setDeniedPrefixes({"C:\\Windows", "/etc", "/usr"});
    sandbox.setAllowedExtensions({"cpp", "h", "txt", "json"});
    sandbox.setMaxCompilationsPerMinute(5);
    sandbox.setMaxFileWritesPerTurn(10);
    sandbox.resetTurnCounters();

    assert(sandbox.validateWrite("C:\\temp\\yuki_sandbox\\test.cpp").allowed());
    assert(!sandbox.validateWrite("src\\brain\\test.cpp").allowed());
    assert(!sandbox.validateWrite("C:\\temp\\yuki_sandbox\\test.exe").allowed());
    assert(!sandbox.validateWrite("C:\\temp\\yuki_sandbox\\..\\Windows\\test.cpp").allowed());

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

    return 0;
}
