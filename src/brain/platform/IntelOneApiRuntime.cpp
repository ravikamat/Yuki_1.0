#include "src/brain/platform/IntelOneApiRuntime.h"
#include "src/brain/platform/RuntimeProcess.h"
#include <fstream>
#include <algorithm>
#include <cctype>

namespace yuki::brain::platform {

IntelOneApiRuntimeStatus IntelOneApiRuntime::probe(const OneApiRuntimeConfig& config) const {
    IntelOneApiRuntimeStatus status;
    status.configured = config.enabled;
    if (!config.enabled) {
        status.diagnostic = "oneAPI runtime is disabled in configuration";
        return status;
    }

    // Check environment script existence if specified
    if (!config.environmentScript.empty()) {
        std::ifstream scriptFile(config.environmentScript);
        status.environmentScriptExists = scriptFile.good();
    } else {
        status.environmentScriptExists = true;
    }

    std::string probeCmd = config.syclRuntimeProbe.empty() ? "sycl-ls.exe" : config.syclRuntimeProbe;

    RuntimeProcess proc;
    auto procRes = proc.runAndCapture(probeCmd, {}, "", 5000);
    if (!procRes.started) {
        status.syclProbeFound = false;
        status.diagnostic = "SYCL probe executable not found or failed to start: " + probeCmd;
        return status;
    }

    status.syclProbeFound = true;
    status.syclProbeSucceeded = procRes.completed && procRes.exitCode == 0;

    std::string output = procRes.stdoutText + "\n" + procRes.stderrText;
    std::string lowerOutput = output;
    std::transform(lowerOutput.begin(), lowerOutput.end(), lowerOutput.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    if (lowerOutput.find("intel") != std::string::npos &&
        (lowerOutput.find("gpu") != std::string::npos || lowerOutput.find("graphics") != std::string::npos || lowerOutput.find("sycl") != std::string::npos)) {
        status.intelGpuDetected = true;
        status.detectedDevices.push_back("Intel SYCL GPU (" + probeCmd + ")");
        status.diagnostic = "Intel SYCL device detected successfully via " + probeCmd;
    } else if (status.syclProbeSucceeded) {
        status.diagnostic = "SYCL probe executed cleanly but no Intel GPU string found in output";
    } else {
        status.diagnostic = "SYCL probe returned exit code " + std::to_string(procRes.exitCode);
    }

    return status;
}

} // namespace yuki::brain::platform
