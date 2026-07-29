#include "src/brain/language/LocalModelHealth.h"
#include <cassert>
#include <iostream>

int main() {
    std::cout << "Running testlocalmodelserverreadiness...\n";
    using namespace yuki::brain::language;

    LocalModelHealthStatus status;
    status.reachable = true;
    status.statusCode = 500; // Server reachable but internal error
    status.usable = (status.statusCode == 200);

    assert(status.reachable);
    assert(!status.usable);

    status.statusCode = 200;
    status.usable = (status.statusCode == 200);

    assert(status.reachable);
    assert(status.usable);

    std::cout << "[PASS] testlocalmodelserverreadiness completed cleanly.\n";
    return 0;
}
