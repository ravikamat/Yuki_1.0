#include "SecuritySandbox.h"
#include "brain/core/ConfigManager.h"
#include <algorithm>
#include <cctype>

namespace yuki::security {

SecuritySandbox& SecuritySandbox::instance() {
    static SecuritySandbox sandbox;
    return sandbox;
}

void SecuritySandbox::setAllowedPrefixes(const std::vector<std::filesystem::path>& prefixes) {
    std::lock_guard<std::mutex> lock(mutex_);
    allowedPrefixes_ = prefixes;
}

void SecuritySandbox::setDeniedPrefixes(const std::vector<std::filesystem::path>& prefixes) {
    std::lock_guard<std::mutex> lock(mutex_);
    deniedPrefixes_ = prefixes;
}

void SecuritySandbox::setAllowedExtensions(const std::vector<std::string>& extensions) {
    std::lock_guard<std::mutex> lock(mutex_);
    allowedExtensions_.clear();
    for (const auto& ext : extensions) {
        std::string lower = ext;
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](unsigned char c){ return std::tolower(c); });
        allowedExtensions_.insert(lower);
    }
}

void SecuritySandbox::setMaxCompilationsPerMinute(size_t limit) {
    std::lock_guard<std::mutex> lock(mutex_);
    maxCompilationsPerMinute_ = limit;
}

void SecuritySandbox::setMaxFileWritesPerTurn(size_t limit) {
    std::lock_guard<std::mutex> lock(mutex_);
    maxFileWritesPerTurn_ = limit;
}

void SecuritySandbox::resetTurnCounters() {
    std::lock_guard<std::mutex> lock(mutex_);
    fileWriteCountThisTurn_ = 0;
    turnStart_ = std::chrono::steady_clock::now();
}

std::filesystem::path SecuritySandbox::canonicalize(const std::string& path) const {
    try {
        return std::filesystem::weakly_canonical(std::filesystem::absolute(path));
    } catch (...) {
        return {};
    }
}

bool SecuritySandbox::hasAllowedExtension(const std::filesystem::path& path) const {
    if (allowedExtensions_.empty()) return true;
    std::string ext = path.extension().string();
    if (ext.empty()) return false;
    if (ext[0] == '.') ext = ext.substr(1);
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c){ return std::tolower(c); });
    return allowedExtensions_.count(ext) > 0;
}

bool SecuritySandbox::isUnderAllowedPrefix(const std::filesystem::path& path) const {
    if (allowedPrefixes_.empty()) return true;
    auto canonPath = canonicalize(path.string());
    std::string pathStr = canonPath.string();
    std::transform(pathStr.begin(), pathStr.end(), pathStr.begin(), [](unsigned char c){ return std::tolower(c); });

    for (const auto& prefix : allowedPrefixes_) {
        auto canonPrefix = canonicalize(prefix.string());
        std::string prefixStr = canonPrefix.string();
        std::transform(prefixStr.begin(), prefixStr.end(), prefixStr.begin(), [](unsigned char c){ return std::tolower(c); });

        if (pathStr.size() >= prefixStr.size() &&
            pathStr.compare(0, prefixStr.size(), prefixStr) == 0) {
            return true;
        }
    }
    return false;
}

bool SecuritySandbox::isUnderDeniedPrefix(const std::filesystem::path& path) const {
    auto canonPath = canonicalize(path.string());
    std::string pathStr = canonPath.string();
    std::transform(pathStr.begin(), pathStr.end(), pathStr.begin(), [](unsigned char c){ return std::tolower(c); });

    for (const auto& prefix : deniedPrefixes_) {
        auto canonPrefix = canonicalize(prefix.string());
        std::string prefixStr = canonPrefix.string();
        std::transform(prefixStr.begin(), prefixStr.end(), prefixStr.begin(), [](unsigned char c){ return std::tolower(c); });

        if (pathStr.size() >= prefixStr.size() &&
            pathStr.compare(0, prefixStr.size(), prefixStr) == 0) {
            return true;
        }
    }
    return false;
}

bool SecuritySandbox::isPathTraversal(const std::filesystem::path& path) const {
    return path.string().find("..") != std::string::npos;
}

bool SecuritySandbox::isSourceTreePath(const std::string& path) const {
    try {
        auto base = std::filesystem::weakly_canonical(std::filesystem::absolute("."));
        auto target = std::filesystem::weakly_canonical(std::filesystem::absolute(path));
        auto bs = base.string();
        auto ts = target.string();
        return ts.size() >= bs.size() && ts.compare(0, bs.size(), bs) == 0;
    } catch (...) {
        return false;
    }
}

void SecuritySandbox::ensureMinuteWindow() const {
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::minutes>(now - minuteWindowStart_).count() >= 1) {
        compilationCountThisMinute_ = 0;
        minuteWindowStart_ = now;
    }
}

void SecuritySandbox::appendAudit(const SandboxDecision& decision,
                                  const std::string& operation,
                                  const std::string& path) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auditTrail_.push_back({
        std::chrono::steady_clock::now(),
        decision.verdict,
        decision.code,
        operation,
        path
    });
}

std::vector<AuditRecord> SecuritySandbox::getAuditTrail() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return auditTrail_;
}

void SecuritySandbox::setSandboxBasePath(const std::filesystem::path& base) {
    std::lock_guard<std::mutex> lock(mutex_);
    sandboxBasePath_ = base;
}

void SecuritySandbox::buildCache() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (sandboxBasePath_.empty()) {
        sandboxBasePath_ = std::filesystem::current_path();
    }
    if (allowedPrefixes_.empty()) {
        allowedPrefixes_ = { sandboxBasePath_ / "workspace", sandboxBasePath_ / "data" };
    }
    if (deniedPrefixes_.empty()) {
        std::vector<std::string> deniedWin, deniedUnix;
        yuki::ConfigManager::instance().loadSecurityPaths("data/security_paths.txt", deniedWin, deniedUnix);
#ifdef _WIN32
        for (const auto& p : deniedWin) deniedPrefixes_.emplace_back(p);
        if (deniedPrefixes_.empty()) deniedPrefixes_ = { "C:\\Windows", "C:\\Program Files", "C:\\System32" };
#else
        for (const auto& p : deniedUnix) deniedPrefixes_.emplace_back(p);
        if (deniedPrefixes_.empty()) deniedPrefixes_ = { "/etc", "/usr/bin", "/bin" };
#endif
    }
}

SandboxDecision SecuritySandbox::validateWrite(const std::string& path) const {
    std::lock_guard<std::mutex> lock(mutex_);
    ensureMinuteWindow();

    if (fileWriteCountThisTurn_ >= maxFileWritesPerTurn_) {
        return {SandboxVerdict::DENY, DenyReasonCode::RATE_LIMIT_WRITE};
    }

    auto base = sandboxBasePath_.empty() ? std::filesystem::current_path() : sandboxBasePath_;
    auto norm = PathNormalizer::normalize(path, base);
    if (!norm.is_valid) {
        return {SandboxVerdict::DENY, DenyReasonCode::PATH_TRAVERSAL};
    }

    if (isPathTraversal(norm.absolute)) {
        return {SandboxVerdict::DENY, DenyReasonCode::PATH_TRAVERSAL};
    }

    if (isSourceTreePath(norm.absolute.string())) {
        return {SandboxVerdict::DENY, DenyReasonCode::READONLY_VIOLATION};
    }

    if (isUnderDeniedPrefix(norm.absolute)) {
        return {SandboxVerdict::DENY, DenyReasonCode::PATH_NOT_ALLOWED};
    }

    if (!isUnderAllowedPrefix(norm.absolute)) {
        return {SandboxVerdict::DENY, DenyReasonCode::PATH_NOT_ALLOWED};
    }

    if (!hasAllowedExtension(norm.absolute)) {
        return {SandboxVerdict::DENY, DenyReasonCode::EXTENSION_NOT_ALLOWED};
    }

    fileWriteCountThisTurn_++;
    return {SandboxVerdict::ALLOW, DenyReasonCode::NONE};
}

SandboxDecision SecuritySandbox::validateRead(const std::string& path) const {
    auto base = sandboxBasePath_.empty() ? std::filesystem::current_path() : sandboxBasePath_;
    auto norm = PathNormalizer::normalize(path, base);
    if (!norm.is_valid) {
        return {SandboxVerdict::DENY, DenyReasonCode::PATH_TRAVERSAL};
    }
    if (isPathTraversal(norm.absolute)) {
        return {SandboxVerdict::DENY, DenyReasonCode::PATH_TRAVERSAL};
    }
    return {SandboxVerdict::ALLOW, DenyReasonCode::NONE};
}

SandboxDecision SecuritySandbox::validateCompile() const {
    std::lock_guard<std::mutex> lock(mutex_);
    ensureMinuteWindow();

    if (compilationCountThisMinute_ >= maxCompilationsPerMinute_) {
        return {SandboxVerdict::DENY, DenyReasonCode::RATE_LIMIT_COMPILE};
    }

    compilationCountThisMinute_++;
    return {SandboxVerdict::ALLOW, DenyReasonCode::NONE};
}

SandboxDecision SecuritySandbox::validateExecute(const std::string& executablePath) const {
    auto base = sandboxBasePath_.empty() ? std::filesystem::current_path() : sandboxBasePath_;
    auto norm = PathNormalizer::normalize(executablePath, base);
    if (!norm.is_valid) {
        return {SandboxVerdict::DENY, DenyReasonCode::PATH_TRAVERSAL};
    }
    if (isPathTraversal(norm.absolute)) {
        return {SandboxVerdict::DENY, DenyReasonCode::PATH_TRAVERSAL};
    }
    if (isSourceTreePath(norm.absolute.string())) {
        return {SandboxVerdict::DENY, DenyReasonCode::READONLY_VIOLATION};
    }
    if (isUnderDeniedPrefix(norm.absolute)) {
        return {SandboxVerdict::DENY, DenyReasonCode::PATH_NOT_ALLOWED};
    }
    return {SandboxVerdict::ALLOW, DenyReasonCode::NONE};
}

bool SecuritySandbox::isActionAllowed(const std::string& actionType) const {
    (void)actionType;
    return true;
}

bool SecuritySandbox::validateActionPath(const std::string& path) const {
    auto base = sandboxBasePath_.empty() ? std::filesystem::current_path() : sandboxBasePath_;
    auto norm = PathNormalizer::normalize(path, base);
    if (!norm.is_valid) return false;

    const auto& abs = norm.absolute;
    std::string absStr = abs.string();
    std::transform(absStr.begin(), absStr.end(), absStr.begin(), [](unsigned char c){ return static_cast<char>(::tolower(c)); });

    for (const auto& denied : deniedPrefixes_) {
        auto dNorm = PathNormalizer::normalize(denied, base);
        if (dNorm.is_valid) {
            std::string dStr = dNorm.absolute.string();
            std::transform(dStr.begin(), dStr.end(), dStr.begin(), [](unsigned char c){ return static_cast<char>(::tolower(c)); });
            if (absStr.size() >= dStr.size() && absStr.compare(0, dStr.size(), dStr) == 0) {
                return false;
            }
        }
    }

    if (!allowedPrefixes_.empty()) {
        bool in_allowed = false;
        for (const auto& allowed : allowedPrefixes_) {
            auto aNorm = PathNormalizer::normalize(allowed, base);
            if (aNorm.is_valid) {
                std::string aStr = aNorm.absolute.string();
                std::transform(aStr.begin(), aStr.end(), aStr.begin(), [](unsigned char c){ return static_cast<char>(::tolower(c)); });
                if (absStr.size() >= aStr.size() && absStr.compare(0, aStr.size(), aStr) == 0) {
                    in_allowed = true;
                    break;
                }
            }
        }
        if (!in_allowed) return false;
    }

    if (std::filesystem::exists(abs)) {
        try {
            if (std::filesystem::is_symlink(abs)) {
                auto link_target = std::filesystem::read_symlink(abs);
                auto link_norm = PathNormalizer::normalize(link_target, abs.parent_path());
                if (!link_norm.is_valid) return false;
                if (!validateActionPath(link_norm.absolute.string())) return false;
            }
        } catch (...) {
            return false;
        }
    }

    return true;
}

bool SecuritySandbox::validateActionCommand(const std::string& command) const {
    (void)command;
    return true;
}

} // namespace yuki::security
