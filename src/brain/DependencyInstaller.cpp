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

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <filesystem>

bool DependencyInstaller::isToolInstalled(const std::string& tool) {
    if (tool.empty()) return false;

    const char* path_env = std::getenv("PATH");
    if (!path_env) return false;

    std::string path_str(path_env);
    std::istringstream iss(path_str);
    std::string dir;

#ifdef _WIN32
    constexpr char path_sep = ';';
    const std::vector<std::string> exts = {"", ".exe", ".cmd", ".bat"};
#else
    constexpr char path_sep = ':';
    const std::vector<std::string> exts = {""};
#endif

    while (std::getline(iss, dir, path_sep)) {
        if (dir.empty()) continue;
        for (const auto& ext : exts) {
            std::filesystem::path candidate = std::filesystem::path(dir) / (tool + ext);
            std::error_code ec;
            if (std::filesystem::exists(candidate, ec) && std::filesystem::is_regular_file(candidate, ec)) {
                return true;
            }
        }
    }
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
