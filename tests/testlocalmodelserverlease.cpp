#include "src/brain/language/LocalModelServerLease.h"
#include <cassert>
#include <iostream>

int main() {
    std::cout << "Running testlocalmodelserverlease...\n";
    using namespace yuki::brain::language;

    LocalModelServerLease lease;
    lease.attachedToExistingServer = true;
    lease.ownedByYuki = false;
    lease.endpoint = "http://127.0.0.1:18080";

    // Non-destructive ownership contract
    assert(lease.attachedToExistingServer);
    assert(!lease.ownedByYuki);

    // YUKI-owned process contract
    LocalModelServerLease ownedLease;
    ownedLease.attachedToExistingServer = false;
    ownedLease.ownedByYuki = true;
    ownedLease.processId = 1234;
    ownedLease.ownershipToken = "yuki-owned-1234";

    assert(!ownedLease.attachedToExistingServer);
    assert(ownedLease.ownedByYuki);
    assert(ownedLease.processId == 1234);

    std::cout << "[PASS] testlocalmodelserverlease completed cleanly.\n";
    return 0;
}
