#include <iostream>
#include <cassert>
#include "src/brain/language/SelfEvaluationGate.h"

int main() {
    using yuki::brain::language::SelfEvaluationGate;
    using yuki::brain::language::SelfEvalInput;

    SelfEvaluationGate gate;
    SelfEvalInput in;
    in.query = "Explain C++20 constexpr";
    in.candidate = "C++20 constexpr enables compile-time evaluation for algorithms and data structures.";
    in.localConfidence = 0.88f;
    in.localFluency = 0.85f;

    auto res = gate.evaluate(in);
    if (!res.approved) {
        std::cerr << "[FAIL] testselfevalgate: expected local candidate approval\n";
        return 1;
    }

    std::cout << "[PASS] testselfevalgate\n";
    return 0;
}
