#include <iostream>
#include <cmath>
#include "brain/inference/PrecisionPredictor.h"

using namespace yuki::inference;

int main() {
    std::cout << "Starting test_precision_predictor..." << std::endl;

    // Test 1: ColdStartUniform — predictor with zero weights returns ~0.55 for any input
    PrecisionPredictor predictor;
    float p1 = predictor.predict("it", "", {});
    float p2 = predictor.predict("do you know python tts code", "", {});
    std::cout << "ColdStart p1='it': " << p1 << ", p2='python tts': " << p2 << std::endl;
    if (std::abs(p1 - 0.55f) >= 0.05f || std::abs(p2 - 0.55f) >= 0.05f) {
        std::cerr << "FAIL: ColdStartUniform bounds out of range" << std::endl;
        return 1;
    }

    // Test 2: TrainClarification — train with target 0.1 on "it", then predict on same input
    float p_before = predictor.predict("it", "", {});
    for (int i = 0; i < 50; ++i) {
        predictor.trainStep(p_before, 0.1f, "it", "", {});
        p_before = predictor.predict("it", "", {});
    }
    std::cout << "After TrainClarification p='it': " << p_before << std::endl;
    if (p_before >= 0.50f) {
        std::cerr << "FAIL: TrainClarification precision did not drop below 0.50" << std::endl;
        return 1;
    }

    // Test 3: TrainDirectResponse — FRESH predictor, train with target 0.7 on "python tts code"
    PrecisionPredictor predictor3;
    p_before = predictor3.predict("do you know python tts code", "", {});
    for (int i = 0; i < 50; ++i) {
        predictor3.trainStep(p_before, 0.7f, "do you know python tts code", "", {});
        p_before = predictor3.predict("do you know python tts code", "", {});
    }
    std::cout << "After TrainDirectResponse p='python tts': " << p_before << std::endl;
    if (p_before <= 0.60f) {
        std::cerr << "FAIL: TrainDirectResponse precision did not rise above 0.60" << std::endl;
        return 1;
    }

    // Test 4: SerializeRoundTrip — serialize weights, deserialize, predict identical values
    PrecisionPredictor predictor1;
    predictor1.trainStep(0.55f, 0.1f, "it", "", {});
    std::string json = predictor1.serialize();
    std::cout << "Serialized weights JSON: " << json << std::endl;
    if (json.empty()) {
        std::cerr << "FAIL: Serialized JSON empty" << std::endl;
        return 1;
    }

    PrecisionPredictor predictor2;
    predictor2.deserialize(json);

    float p1_orig = predictor1.predict("it", "", {});
    float p2_deser = predictor2.predict("it", "", {});
    std::cout << "Original: " << p1_orig << ", Deserialized: " << p2_deser << std::endl;
    if (std::abs(p1_orig - p2_deser) >= 1e-5f) {
        std::cerr << "FAIL: Deserialized prediction mismatch" << std::endl;
        return 1;
    }

    std::cout << "ALL PRECISION PREDICTOR TESTS PASSED!" << std::endl;
    return 0;
}
