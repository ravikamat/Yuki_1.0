#pragma once
#include <string>
#include <vector>
#include <unordered_set>
#include <mutex>
#include <chrono>
#include <filesystem>

namespace yuki::security {

enum class SandboxVerdict {
    ALLOW,
    DENY
};

enum class DenyReasonCode {
    NONE,
    PATH_NOT_ALLOWED,
    EXTENSION_NOT_ALLOWED,
    RATE_LIMIT_COMPILE,
    RATE_LIMIT_WRITE,
    READONLY_VIOLATION,
    PATH_TRAVERSAL
};

struct SandboxDecision {
    SandboxVerdict verdict;
    DenyReasonCode code;
    bool allowed() const { return verdict == SandboxVerdict::ALLOW; }
};

struct AuditRecord {
    std::chrono::steady_clock::time_point timestamp;
    SandboxVerdict verdict;
    DenyReasonCode code;
    std::string operation;  // "write", "read", "compile", "execute"
    std::string path;       // canonical path
};

class SecuritySandbox {
public:
    static SecuritySandbox& instance();

    void setAllowedPrefixes(const std::vector<std::string>& prefixes);
    void setDeniedPrefixes(const std::vector<std::string>& prefixes);
    void setAllowedExtensions(const std::vector<std::string>& extensions);
    void setMaxCompilationsPerMinute(size_t limit);
    void setMaxFileWritesPerTurn(size_t limit);

    SandboxDecision validateWrite(const std::string& path) const;
    SandboxDecision validateRead(const std::string& path) const;
    SandboxDecision validateCompile() const;
    SandboxDecision validateExecute(const std::string& executablePath) const;

    SandboxDecision verifyModule(const std::string& modulePath) const;
    bool isModuleAllowed(uint64_t moduleId) const;

    // Structured audit trail — NO human text. Stored as records for later synthesis.
    void appendAudit(const SandboxDecision& decision,
                     const std::string& operation,
                     const std::string& path) const;

    std::vector<AuditRecord> getAuditTrail() const;

    void resetTurnCounters();

    bool isSourceTreePath(const std::string& path) const;

private:
    SecuritySandbox() = default;

    mutable std::mutex mutex_;
    mutable std::vector<AuditRecord> auditTrail_;

    std::vector<std::filesystem::path> allowedPrefixes_;
    std::vector<std::filesystem::path> deniedPrefixes_;
    std::unordered_set<std::string> allowedExtensions_;
    size_t maxCompilationsPerMinute_ = 5;
    size_t maxFileWritesPerTurn_ = 20;

    mutable size_t compilationCountThisMinute_ = 0;
    mutable size_t fileWriteCountThisTurn_ = 0;
    mutable std::chrono::steady_clock::time_point minuteWindowStart_;
    mutable std::chrono::steady_clock::time_point turnStart_;

    std::filesystem::path canonicalize(const std::string& path) const;
    bool hasAllowedExtension(const std::filesystem::path& path) const;
    bool isUnderAllowedPrefix(const std::filesystem::path& path) const;
    bool isUnderDeniedPrefix(const std::filesystem::path& path) const;
    bool isPathTraversal(const std::filesystem::path& path) const;
    void ensureMinuteWindow() const;
};

} // namespace yuki::security
