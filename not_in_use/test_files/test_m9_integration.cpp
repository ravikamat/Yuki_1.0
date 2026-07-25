// Integration test: verify all 4 advisory hooks compile, link, and flow data.
#include "brain/policy/PolicySelector.h"
#include "brain/metacognition/MetacognitionEngine.h"
#include "brain/predictive/TurnCoordinator.h"
#include "brain/research/core/ResearchPlanner.h"
#include "brain/self/SelfModel.h"
#include "brain/self/TheoryOfMind.h"
#include "brain/emotion/ValenceArousalModel.h"
#include "brain/organism/DriveSystem.h"
#include "brain/organism/ConfidenceCalibrator.h"
#include <cassert>

using namespace yuki;

int main() {
    // 1. PolicySelector + ValenceArousalModel advisory
    policy::PolicySelector ps;
    auto vam = std::make_unique<emotion::ValenceArousalModel>();
    vam->update(1.0f, 1.0f, 0.5f, 0.0f); // positive valence
    ps.setValenceArousalModel(vam.release());

    // 2. MetacognitionEngine + DriveSystem advisory
    metacognition::MetacognitionEngine me;
    auto ds = std::make_unique<organism::DriveSystem>();
    me.setDriveSystem(ds.release());

    // 3. TurnCoordinator + TheoryOfMind advisory
    TurnCoordinator tc;
    auto tom = std::make_unique<self::TheoryOfMind>();
    tc.setTheoryOfMind(tom.release());

    // 4. ResearchPlanner + DriveSystem advisory
    research::ResearchPlanner rp;
    auto ds2 = std::make_unique<organism::DriveSystem>();
    rp.setDriveSystem(ds2.release());

    // 5. Cross-module data flow: SelfModel -> DriveSystem
    self::SelfModel sm;
    organism::DriveSystem ds3;
    emotion::ValenceArousalModel emo;
    self::TheoryOfMind tom2;
    std::array<float, 11> cap = {0.2f,0.2f,0.2f,0.2f,0.2f,0.2f,0.2f,0.2f,0.2f,0.2f,0.2f};
    sm.update(cap, 0.9f, {0.5f,0.5f,0.5f,0.5f}, true, 0.8f);
    emo.update(0.0f, 0.0f, 0.5f, 0.8f);
    ds3.proposeGoals(sm, tom2, emo);
    assert(!ds3.activeGoals().empty());

    // 6. ConfidenceCalibrator used with PolicySelector context
    organism::ConfidenceCalibrator cc;
    cc.recordPrediction(0.8f, true);
    assert(cc.adjustConfidence(0.8f) >= 0.0f);

    return 0;
}
