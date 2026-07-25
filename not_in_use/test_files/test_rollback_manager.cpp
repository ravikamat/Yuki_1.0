#include <cassert>
#include "brain/action/core/RollbackManager.h"

using namespace yuki::action;

int main() {
    RollbackManager manager;

    auto id1 = manager.createCheckpoint("test_checkpoint_1");
    assert(id1 != 0);

    auto checkpoints = manager.listCheckpoints();
    assert(!checkpoints.empty());

    assert(manager.validateCheckpoint(id1));
    assert(manager.rollbackTo(id1));

    manager.invalidateCheckpoint(id1);
    assert(!manager.validateCheckpoint(id1));

    manager.clearCheckpoints();
    assert(manager.listCheckpoints().empty());

    return 0;
}
