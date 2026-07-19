#pragma once
#include "ExecutionTypes.h"
#include <vector>
#include <string>

class DependencyInstaller {
public:
    std::vector<DependencyCheckResult> ensure(const std::vector<std::string>& toolChecks);

private:
    bool isToolInstalled(const std::string& tool);
    DependencyCheckResult buildApprovalRequest(const std::string& tool);
};
