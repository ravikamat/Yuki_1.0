#pragma once
// ToolPermissionEngine — Grok Bot pattern: always/ask/never per tool with adminCeiling
// Maps to Grok Bot's local-tool-permission.ts

#include <string>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <functional>
#include <optional>

#include "brain/core/Logger.h"

namespace yuki {
namespace system {

enum class ToolPermission : uint8_t {
    ALWAYS  = 0,  // Allow without prompting
    ASK     = 1,  // Prompt user each time
    NEVER   = 2   // Deny always
};

enum class ToolAction : uint8_t {
    RUN_COMMAND     = 0,  // Execute shell command
    SEND_INPUT      = 1,  // Send keystrokes/mouse
    READ_FILE       = 2,  // Read file contents
    LIST_DIRECTORY  = 3,  // List directory contents
    WRITE_FILE      = 4,  // Write/modify file
    DELETE_FILE     = 5,  // Delete file
    NETWORK_REQUEST = 6,  // Make HTTP/network request
    EXECUTE_CODE    = 7   // Run code in sandbox
};

struct PermissionRule {
    ToolAction action;
    ToolPermission permission;
    std::string tool_pattern;  // Exact tool name or glob pattern
    std::string path_pattern;  // For file actions: path glob pattern
    std::string reason;        // Human-readable reason for audit

    PermissionRule() = default;
    PermissionRule(ToolAction a, ToolPermission p, std::string t = "", std::string path = "", std::string r = "")
        : action(a), permission(p), tool_pattern(std::move(t)), path_pattern(std::move(path)), reason(std::move(r)) {}
};

struct PermissionDecision {
    ToolPermission result;
    ToolPermission base_permission;
    ToolPermission admin_ceiling;
    bool overridden = false;
    std::string reason;
};

// ToolPermissionEngine runtime configuration (namespace scope so the
// constructor's default argument `Config{}` parses cleanly with nested-class
// Non-static data member initializers � valid C++20.
struct ToolPermissionEngineConfig {
    // Default permissions for unknown tools/actions
    ToolPermission default_read = ToolPermission::ALWAYS;
    ToolPermission default_write = ToolPermission::ASK;
    ToolPermission default_execute = ToolPermission::ASK;
    ToolPermission default_network = ToolPermission::ASK;

    // Admin ceiling (constitutional override) — highest permission allowed
    ToolPermission admin_ceiling = ToolPermission::ASK;

    // Persistence file
    std::string config_file = "data/brain/tool_permissions.json";

    // Auto-persist on changes
    bool auto_persist = true;
};

class ToolPermissionEngine {
public:
    using Config = ToolPermissionEngineConfig;

    explicit ToolPermissionEngine(const Config& cfg = Config{});
    ~ToolPermissionEngine();

    // Non-copyable
    ToolPermissionEngine(const ToolPermissionEngine&) = delete;
    ToolPermissionEngine& operator=(const ToolPermissionEngine&) = delete;

    // Initialize: load persisted permissions
    bool initialize();

    // Shutdown: persist
    void shutdown();

    // Check permission for a tool action
    PermissionDecision check_permission(const std::string& tool_name,
                                        ToolAction action,
                                        const std::string& path = "") const;

    // Request permission from user (async, via avatar callback)
    using PermissionCallback = std::function<void(ToolPermission)>;
    bool request_permission(const std::string& tool_name,
                            ToolAction action,
                            const std::string& path,
                            const std::string& context,
                            PermissionCallback callback);

    // Set permission rule (persists if auto_persist)
    bool set_permission(const std::string& tool_pattern,
                        ToolAction action,
                        ToolPermission permission,
                        const std::string& path_pattern = "",
                        const std::string& reason = "");

    // Remove permission rule
    bool remove_permission(const std::string& tool_pattern, ToolAction action);

    // Set admin ceiling (constitutional override)
    void set_admin_ceiling(ToolPermission ceiling) {
        admin_ceiling_.store(ceiling, std::memory_order_relaxed);
    }

    ToolPermission get_admin_ceiling() const {
        return admin_ceiling_.load(std::memory_order_relaxed);
    }

    // Get all rules for UI/config display
    std::vector<PermissionRule> get_all_rules() const;

    // Set avatar prompt callback (for ASK permission)
    using AvatarPromptCallback = std::function<void(const std::string& tool, const std::string& action, const std::string& context, PermissionCallback)>;
    void set_avatar_prompt_callback(AvatarPromptCallback cb) {
        avatar_prompt_cb_ = std::move(cb);
    }

    // Stats
    struct Stats {
        uint64_t total_checks = 0;
        uint64_t allowed = 0;
        uint64_t denied = 0;
        uint64_t prompted = 0;
        uint64_t overridden = 0;
    };
    Stats get_stats() const;

private:
    Config config_;
    mutable std::mutex rules_mutex_;
    std::vector<PermissionRule> rules_;
    std::atomic<ToolPermission> admin_ceiling_{ToolPermission::ASK};
    AvatarPromptCallback avatar_prompt_cb_;
    mutable Stats stats_;

    // Permission resolution (base + admin ceiling)
    ToolPermission resolve_permission(ToolPermission base) const;

    // Pattern matching (glob-style)
    bool matches_pattern(const std::string& pattern, const std::string& value) const;

    // Persistence
    bool load_from_file();
    bool save_to_file() const;
};

} // namespace system
} // namespace yuki