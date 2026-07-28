#include <iostream>
#include <cassert>
#include "src/brain/language/CandidateCritiqueEngine.h"
#include "src/brain/language/ExternalLlmBackend.h"

int main() {
    using yuki::brain::language::CandidateCritiqueEngine;
    using yuki::brain::language::ExternalLlmBackend;
    using yuki::brain::language::CritiqueInput;

    ExternalLlmBackend backend;
    CandidateCritiqueEngine engine(backend);

    CritiqueInput in;
    in.userQuery = "Write a C++ function";
    in.candidateText = "int main() { return 0; }";
    in.requiresCodeExactness = true;

    auto res = engine.critique(in);
    if (!res.success) {
        std::cerr << "[FAIL] testcritiqueloop: critique failed\n";
        return 1;
    }

    std::cout << "[PASS] testcritiqueloop\n";
    return 0;
}
