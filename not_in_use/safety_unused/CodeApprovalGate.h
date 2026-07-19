#pragma once
#include <string>
#include <map>
#include <mutex>
#include <vector>
#include <cstdint>

enum class ApprovalState { PENDING, USER_APPROVED, EXECUTING, COMMITTED, ROLLED_BACK };

struct ApprovalRequest {
    std::string id;
    std::string path;
    std::string content;
    std::string backupPath;
    ApprovalState state = ApprovalState::PENDING;
    int64_t timestamp = 0;
};

class CodeApprovalGate {
public:
    static CodeApprovalGate& instance();
    
    // Returns request ID. If path is not in src/, auto-approves.
    std::string requestWrite(const std::string& path, const std::string& content);
    
    ApprovalState getState(const std::string& requestId) const;
    void approve(const std::string& requestId);
    void reject(const std::string& requestId);
    void commit(const std::string& requestId);    // executes write
    void rollback(const std::string& requestId);  // restores backup
    
private:
    CodeApprovalGate() = default;
    std::map<std::string, ApprovalRequest> requests_;
    mutable std::mutex mutex_;
};
