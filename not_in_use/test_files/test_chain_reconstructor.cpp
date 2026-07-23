#include "brain/memory/ChainReconstructor.h"
#include <cassert>

int main() {
    yuki::memory::ChainReconstructor recon;

    auto chain = recon.buildPrerequisiteChain("Build Android App");
    assert(!chain.nodes.empty());
    assert(chain.overallCoherence > 0.0f);

    return 0;
}
