#include "DependencyInstaller.h"
#include <iostream>

std::vector<DependencyCheckResult> DependencyInstaller::ensure(const std::vector<std::string>& toolChecks) {
    std::vector<DependencyCheckResult> results;
    for (const auto& tool : toolChecks) {
        if (isToolInstalled(tool)) {
            DependencyCheckResult res;
            res.toolName = tool;
            res.alreadyInstalled = true;
            results.push_back(res);
        } else {
            results.push_back(buildApprovalRequest(tool));
        }
    }
    return results;
}

bool DependencyInstaller::isToolInstalled(const std::string& tool) {
    // Stub: always assume not installed unless it's a known tool we mock
    return false;
}

DependencyCheckResult DependencyInstaller::buildApprovalRequest(const std::string& tool) {
    DependencyCheckResult res;
    res.toolName = tool;
    res.alreadyInstalled = false;
    res.approvalRequested = true;
    res.approvalSummary = "Dependency required: " + tool;
    return res;
}
