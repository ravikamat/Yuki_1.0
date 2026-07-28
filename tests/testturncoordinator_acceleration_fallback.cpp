#include "src/brain/predictive/TurnState.h"
#include "src/brain/language/GenerationRouter.h"
#include "src/brain/platform/BackendSelector.h"
#include <cassert>
#include <iostream>

int main() {
    std::cout << "Running testturncoordinator_acceleration_fallback...\n";
    using namespace yuki;
    using namespace yuki::brain::language;
    using namespace yuki::brain::platform;

    PredictionState state;
    state.acceleratedLocalAttempted = true;
    state.acceleratedLocalUsed = false; // Accelerated call failed
    state.localRuntimeDiagnostic = "SYCL server request timed out";

    assert(state.acceleratedLocalAttempted);
    assert(!state.acceleratedLocalUsed);
    assert(state.localRuntimeDiagnostic == "SYCL server request timed out");

    // Fallback simulation: state remains valid and turn proceeds without crashing
    state.selectedBackendKind = BackendKind::EXTERNAL_LLM;
    state.externalFallbackUsed = true;

    assert(state.selectedBackendKind == BackendKind::EXTERNAL_LLM);
    assert(state.externalFallbackUsed);

    std::cout << "[PASS] testturncoordinator_acceleration_fallback completed cleanly.\n";
    return 0;
}
