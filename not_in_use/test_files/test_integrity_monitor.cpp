#include "brain/security/IntegrityMonitor.h"
#include <cassert>

int main() {
    yuki::security::IntegrityMonitor monitor;

    assert(monitor.verifyModule("SecuritySandbox", 0x12345678));
    assert(monitor.rollbackToCheckpoint("sleep_cycle"));

    return 0;
}
