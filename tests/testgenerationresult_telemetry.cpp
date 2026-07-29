#include "src/brain/language/GenerationBackend.h"
#include "src/brain/predictive/TurnState.h"
#include <cassert>
#include <iostream>

int main() {
    std::cout << "Running testgenerationresult_telemetry...\n";
    using namespace yuki::brain::language;
    using namespace yuki;

    GenerationResult result;
    result.backendName = "llama_cpp_sycl";
    result.backendKind = BackendKind::LOCAL_TRANSFORMER_SYCL;
    result.accelerated = true;
    result.deviceName = "Intel Iris Xe Graphics";
    result.promptTokensPerSecond = 45.0f;
    result.decodeTokensPerSecond = 12.5f;

    assert(result.accelerated);
    assert(result.deviceName == "Intel Iris Xe Graphics");
    assert(result.decodeTokensPerSecond == 12.5f);

    PredictionState state;
    state.acceleratedLocalAttempted = true;
    state.acceleratedLocalUsed = true;
    state.localAccelerationDevice = result.deviceName;
    state.localDecodeTokensPerSecond = result.decodeTokensPerSecond;

    assert(state.acceleratedLocalAttempted);
    assert(state.acceleratedLocalUsed);
    assert(state.localAccelerationDevice == "Intel Iris Xe Graphics");

    std::cout << "[PASS] testgenerationresult_telemetry completed cleanly.\n";
    return 0;
}
