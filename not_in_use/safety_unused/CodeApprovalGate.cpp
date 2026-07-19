#include "CodeApprovalGate.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <chrono>
#include <iomanip>

namespace fs = std::filesystem;

CodeApprovalGate& CodeApprovalGate::instance() {
    static CodeApprovalGate inst;
    return inst;
}

std::string CodeApprovalGate::requestWrite(const std::string& path, const std::string& content) {
    std::string normPath = fs::absolute(path).string();
    
    // Auto-approve non-source paths
    if (normPath.find("\\src\\") == std::string::npos && normPath.find("/src/") == std::string::npos) {
        std::ofstream f(path);
        f << content;
        return "AUTO_APPROVED";
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Generate request ID
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    std::string id = "REQ_" + std::to_string(ms);
    
    // Create backup
    fs::create_directories("data/review/backups");
    std::string backupPath = "data/review/backups/" + id + ".bak";
    if (fs::exists(path)) {
        fs::copy_file(path, backupPath, fs::copy_options::overwrite_existing);
    }
    
    ApprovalRequest req;
    req.id = id;
    req.path = path;
    req.content = content;
    req.backupPath = backupPath;
    req.state = ApprovalState::PENDING;
    req.timestamp = ms;
    
    requests_[id] = req;
    return id;
}

ApprovalState CodeApprovalGate::getState(const std::string& requestId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = requests_.find(requestId);
    return (it != requests_.end()) ? it->second.state : ApprovalState::ROLLED_BACK;
}

void CodeApprovalGate::approve(const std::string& requestId) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = requests_.find(requestId);
    if (it != requests_.end()) it->second.state = ApprovalState::USER_APPROVED;
}

void CodeApprovalGate::commit(const std::string& requestId) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = requests_.find(requestId);
    if (it == requests_.end() || it->second.state != ApprovalState::USER_APPROVED) return;
    
    std::ofstream f(it->second.path);
    f << it->second.content;
    it->second.state = ApprovalState::COMMITTED;
}

void CodeApprovalGate::rollback(const std::string& requestId) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = requests_.find(requestId);
    if (it == requests_.end()) return;
    
    if (fs::exists(it->second.backupPath)) {
        fs::copy_file(it->second.backupPath, it->second.path, fs::copy_options::overwrite_existing);
    }
    it->second.state = ApprovalState::ROLLED_BACK;
}
