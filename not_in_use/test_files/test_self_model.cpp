#include "brain/self/SelfModel.h"
#include <cassert>
#include <cmath>

using namespace yuki::self;

int main() {
    // 1. serialize/deserialize roundtrip
    SelfModel m1;
    std::array<float, 11> cap = {0.1f,0.2f,0.3f,0.4f,0.5f,0.6f,0.7f,0.8f,0.9f,1.0f,0.0f};
    m1.update(cap, 0.8f, {0.5f,0.5f,0.5f,0.5f}, true, 0.9f);
    auto data = m1.serialize();
    SelfModel m2;
    assert(m2.deserialize(data));
    assert(m2.turnCount() == m1.turnCount());
    assert(std::abs(m2.energyLevel() - m1.energyLevel()) < 1e-5f);

    // 2. capability update reflected
    assert(m1.capabilityVector()[0] == 0.1f);

    // 3. success rate EMA bounded
    assert(m1.recentSuccessRate() >= 0.0f && m1.recentSuccessRate() <= 1.0f);

    // 4. identity stability increases with identical updates
    SelfModel m3;
    m3.update(cap, 1.0f, {0,0,0,0}, true, 1.0f);
    m3.update(cap, 1.0f, {0,0,0,0}, true, 1.0f);
    assert(m3.identityStability() < 0.01f); // very stable

    // 5. identity drift after change
    m3.checkpoint();
    std::array<float, 11> cap2 = {0.9f,0.8f,0.7f,0.6f,0.5f,0.4f,0.3f,0.2f,0.1f,0.0f,0.5f};
    m3.update(cap2, 1.0f, {0,0,0,0}, true, 1.0f);
    assert(m3.identityDrift() > 0.3f);

    // 6. hash determinism
    assert(m3.identityHash() == m3.identityHash());
    return 0;
}
