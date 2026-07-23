#include "brain/memory/MemoryFabric.h"
#include <cassert>

int main() {
    yuki::memory::MemoryFabric fabric;

    yuki::memory::MemoryItem item;
    item.itemId = 1;
    item.key = "test_key";
    item.tier = yuki::memory::MemoryTier::T0_WORKING;

    fabric.store(item);
    assert(fabric.getItemCount(yuki::memory::MemoryTier::T0_WORKING) == 1);

    fabric.consolidateT0toT1();
    assert(fabric.getItemCount(yuki::memory::MemoryTier::T0_WORKING) == 0);
    assert(fabric.getItemCount(yuki::memory::MemoryTier::T1_EPISODIC) == 1);

    return 0;
}
