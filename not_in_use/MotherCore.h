#pragma once
#include <string>

struct MotherCoreResult {
    std::string finalText = "Mock build plan: Approved and ready to run.";
};

class MotherCore {
public:
    MotherCoreResult handleInput(const std::string& input) {
        (void)input;
        return MotherCoreResult{};
    }
};
