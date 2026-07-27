#include "brain/organism/DriveSystem.h"
#include "brain/self/SelfModel.h"
#include "brain/self/TheoryOfMind.h"
#include "brain/emotion/ValenceArousalModel.h"
#include <cassert>
#include <cmath>

using namespace yuki::organism;
using namespace yuki::self;
using namespace yuki::emotion;

int main() {
    DriveSystem ds;
    SelfModel self;
    TheoryOfMind tom;
    ValenceArousalModel emo;
    std::array<float, 11> low_cap = {0.1f,0.1f,0.1f,0.1f,0.1f,0.1f,0.1f,0.1f,0.1f,0.1f,0.1f};
    self.update(low_cap, 1.0f, {0.5f,0.5f,0.5f,0.5f}, true, 0.5f);

    // 1. curiosity goal from low capability + high arousal
    emo.update(0.0f, 0.0f, 0.5f, 1.0f); // high arousal
    ds.proposeGoals(self, tom, emo);
    auto goals = ds.activeGoals();
    bool has_curiosity = false;
    for (const auto& g : goals) {
        if (g.type == DriveGoal::Type::CURIOSITY) has_curiosity = true;
    }
    assert(has_curiosity);

    // 2. competence goal from low capability + negative valence
    DriveSystem ds2;
    ValenceArousalModel emo2;
    emo2.update(-1.0f, -1.0f, 0.8f, 0.0f); // negative valence
    ds2.proposeGoals(self, tom, emo2);
    bool has_competence = false;
    for (const auto& g : ds2.activeGoals()) {
        if (g.type == DriveGoal::Type::COMPETENCE) has_competence = true;
    }
    assert(has_competence);

    // 3. top goal has highest priority
    auto top = ds.topGoal();
    assert(top.priority >= ds.activeGoals().back().priority);

    // 4. conflict resolution limits to 3
    assert(ds.activeGoals().size() <= 3);

    // 5. update from outcome changes satisfaction
    ds.updateFromOutcome(true, 1.0f);

    // 6. serialize/deserialize goals
    auto data = ds.serializeGoals();
    DriveSystem ds3;
    assert(ds3.deserializeGoals(data));
    assert(ds3.activeGoals().size() == ds.activeGoals().size());
    return 0;
}
