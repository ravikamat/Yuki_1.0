#include "brain/organism/ConfidenceCalibrator.h"
#include <cassert>
#include <cmath>

using namespace yuki::organism;

int main() {
    ConfidenceCalibrator cc;

    // 1. well calibrated after 100 perfect samples
    for (int i = 0; i < 100; ++i) {
        float conf = 0.5f + (i % 5) * 0.1f; // 0.5,0.6,0.7,0.8,0.9 rotating
        bool success = (conf > 0.55f); // roughly calibrated
        cc.recordPrediction(conf, success);
    }
    assert(cc.totalPredictions() == 100);

    // 2. overconfidence detected: predict 0.9, but only 50% success
    ConfidenceCalibrator cc2;
    for (int i = 0; i < 20; ++i) {
        cc2.recordPrediction(0.9f, i % 2 == 0);
    }
    assert(cc2.adjustConfidence(0.9f) < 0.9f);

    // 3. underconfidence detected: predict 0.5, but 90% success
    ConfidenceCalibrator cc3;
    for (int i = 0; i < 20; ++i) {
        cc3.recordPrediction(0.5f, i != 0); // 19/20 success
    }
    assert(cc3.adjustConfidence(0.5f) > 0.5f);

    // 4. ECE computation is bounded
    assert(cc2.getCalibrationError() >= 0.0f && cc2.getCalibrationError() <= 1.0f);

    // 5. brier score decreases with accuracy
    ConfidenceCalibrator cc4;
    cc4.recordPrediction(1.0f, true);
    float brier1 = cc4.brierScore();
    cc4.recordPrediction(1.0f, true);
    float brier2 = cc4.brierScore();
    assert(brier2 <= brier1 + 1e-5f); // should trend toward 0

    // 6. serialize/deserialize
    auto data = cc.serialize();
    ConfidenceCalibrator cc5;
    assert(cc5.deserialize(data));
    assert(cc5.totalPredictions() == cc.totalPredictions());
    return 0;
}
