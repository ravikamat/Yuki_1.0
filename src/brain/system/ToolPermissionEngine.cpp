// ToolPermissionEngine.cpp — Grok Bot pattern: always/ask/never per tool with adminCeiling

#include "ToolPermissionEngine.h"
#include "brain/core/Logger.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>
#include <algorithm>

namespace yuki {
namespace system {

ToolPermissionEngine::ToolPermissionEngine(const Config& cfg)
    : config_(cfg), admin_ceiling_(cfg.admin_ceiling) {
}

ToolPermissionEngine::~ToolPermissionEngine() {
    shutdown();
}

bool ToolPermissionEngine::initialize() {
    if (!load_from_file()) {
        yuki::core::Logger::instance().log(yuki::core::LogLevel::INFO, "ToolPermissionEngine",
            "No existing permission config, using defaults");

        // Set up sensible defaults
        set_permission("*", ToolAction::READ_FILE, ToolPermission::ALWAYS, "*", "Default: allow reading");
        set_permission("*", ToolAction::LIST_DIRECTORY, ToolPermission::ALWAYS, "*", "Default: allow listing");
        set_permission("*", ToolAction::WRITE_FILE, ToolPermission::ASK, "*", "Default: ask for writes");
        set_permission("*", ToolAction::DELETE_FILE, ToolPermission::ASK, "*", "Default: ask for deletes");
        set_permission("*", ToolAction::RUN_COMMAND, ToolPermission::ASK, "", "Default: ask for commands");
        set_permission("*", ToolAction::SEND_INPUT, ToolPermission::ASK, "", "Default: ask for input injection");
        set_permission("*", ToolAction::NETWORK_REQUEST, ToolPermission::ASK, "", "Default: ask for network");
        set_permission("*", ToolAction::EXECUTE_CODE, ToolPermission::ASK, "", "Default: ask for code execution");
    }

    yuki::core::Logger::instance().log(yuki::core::LogLevel::INFO, "ToolPermissionEngine",
        "Initialized with " + std::to_string(rules_.size()) + " rules, admin_ceiling=" +
        std::to_string(static_cast<int>(admin_ceiling_.load())));

    return true;
}

void ToolPermissionEngine::shutdown() {
    if (config_.auto_persist) {
        save_to_file();
    }
    yuki::core::Logger::instance().log(yuki::core::LogLevel::INFO, "ToolPermissionEngine",
        "Shutdown complete");
}

PermissionDecision ToolPermissionEngine::check_permission(const std::string& tool_name,
                                                                                ToolAction action,
                                                                                const std::string& path) const {
    stats_.total_checks++;

    // Find matching rule (most specific first)
    ToolPermission base_perm = ToolPermission::NEVER;
    bool found = false;

    {
        std::lock_guard<std::mutex> lock(rules_mutex_);
        for (const auto& rule : rules_) {
            if (rule.action != action) continue;
            if (!matches_pattern(rule.tool_pattern, tool_name)) continue;
            if (!rule.path_pattern.empty() && !path.empty() && !matches_pattern(rule.path_pattern, path)) continue;

            base_perm = rule.permission;
            found = true;
            break; // First match wins (rules ordered by specificity)
        }
    }

    if (!found) {
        // Apply defaults based on action type
        switch (action) {
            case ToolAction::READ_FILE:
            case ToolAction::LIST_DIRECTORY:
                base_perm = config_.default_read;
                break;
            case ToolAction::WRITE_FILE:
            case ToolAction::DELETE_FILE:
                base_perm = config_.default_write;
                break;
            case ToolAction::RUN_COMMAND:
            case ToolAction::SEND_INPUT:
            case ToolAction::EXECUTE_CODE:
                base_perm = config_.default_execute;
                break;
            case ToolAction::NETWORK_REQUEST:
                base_perm = config_.default_network;
                break;
        }
    }

    ToolPermission ceiling = admin_ceiling_.load(std::memory_order_relaxed);
    ToolPermission final_perm = resolve_permission(base_perm);

    PermissionDecision decision;
    decision.base_permission = base_perm;
    decision.admin_ceiling = ceiling;
    decision.result = final_perm;
    decision.overridden = (final_perm != base_perm);

    if (decision.overridden) {
        decision.reason = "Overridden by admin ceiling (" +
            std::to_string(static_cast<int>(ceiling)) + " < " +
            std::to_string(static_cast<int>(base_perm)) + ")";
        stats_.overridden++;
    }

    // Update stats
    switch (final_perm) {
        case ToolPermission::ALWAYS: stats_.allowed++; break;
        case ToolPermission::NEVER: stats_.denied++; break;
        case ToolPermission::ASK: stats_.prompted++; break;
    }

    return decision;
}

bool ToolPermissionEngine::request_permission(const std::string& tool_name,
                                              ToolAction action,
                                              const std::string& path,
                                              const std::string& context,
                                              PermissionCallback callback) {
    if (!avatar_prompt_cb_) {
        yuki::core::Logger::instance().log(yuki::core::LogLevel::WARN, "ToolPermissionEngine",
            "No avatar prompt callback set, defaulting to DENY for " + tool_name);
        callback(ToolPermission::NEVER);
        return false;
    }

    // Build action string for prompt
    static const char* action_names[] = {
        "run command", "send input", "read file", "list directory",
        "write file", "delete file", "network request", "execute code"
    };

    std::string prompt = "Tool '" + tool_name + "' wants to " + action_names[static_cast<int>(action)];
    if (!path.empty()) prompt += " on '" + path + "'";
    if (!context.empty()) prompt += " (" + context + ")";
    prompt += ". Allow?";

    yuki::core::Logger::instance().log(yuki::core::LogLevel::INFO, "ToolPermissionEngine",
        "Requesting user permission: " + prompt);

    stats_.prompted++;

    // Async prompt via avatar
    avatar_prompt_cb_(tool_name, action_names[static_cast<int>(action)], context,
        [this, tool_name, action, path, callback](ToolPermission user_choice) {
            // Apply admin ceiling to user choice
            ToolPermission ceiling = admin_ceiling_.load(std::memory_order_relaxed);
            ToolPermission final_choice = (static_cast<int>(user_choice) <= static_cast<int>(ceiling))
                ? user_choice : ceiling;

            yuki::core::Logger::instance().log(yuki::core::LogLevel::INFO, "ToolPermissionEngine",
                "User chose " + std::to_string(static_cast<int>(user_choice)) +
                ", final " + std::to_string(static_cast<int>(final_choice)));

            callback(final_choice);
        });

    return true;
}

bool ToolPermissionEngine::set_permission(const std::string& tool_pattern,
                                          ToolAction action,
                                          ToolPermission permission,
                                          const std::string& path_pattern,
                                          const std::string& reason) {
    PermissionRule rule(action, permission, tool_pattern, path_pattern, reason);

    {
        std::lock_guard<std::mutex> lock(rules_mutex_);

        // Remove existing rule for same tool/action/pattern
        auto it = std::remove_if(rules_.begin(), rules_.end(),
            [&](const PermissionRule& r) {
                return r.action == action && r.tool_pattern == tool_pattern && r.path_pattern == path_pattern;
            });
        rules_.erase(it, rules_.end());

        // Insert at front (higher priority = more specific checked first)
        // For exact matches, they should come before glob patterns
        bool is_exact = tool_pattern.find('*') == std::string::npos && tool_pattern.find('?') == std::string::npos;
        if (is_exact) {
            rules_.insert(rules_.begin(), std::move(rule));
        } else {
            rules_.push_back(std::move(rule));
        }
    }

    if (config_.auto_persist) {
        save_to_file();
    }

    yuki::core::Logger::instance().log(yuki::core::LogLevel::INFO, "ToolPermissionEngine",
        "Set permission: " + tool_pattern + " " + std::to_string(static_cast<int>(action)) +
        " -> " + std::to_string(static_cast<int>(permission)));

    return true;
}

bool ToolPermissionEngine::remove_permission(const std::string& tool_pattern, ToolAction action) {
    std::lock_guard<std::mutex> lock(rules_mutex_);
    auto it = std::remove_if(rules_.begin(), rules_.end(),
        [&](const PermissionRule& r) {
            return r.action == action && r.tool_pattern == tool_pattern;
        });
    bool removed = (it != rules_.end());
    rules_.erase(it, rules_.end());

    if (removed && config_.auto_persist) {
        save_to_file();
    }

    return removed;
}

std::vector<PermissionRule> ToolPermissionEngine::get_all_rules() const {
    std::lock_guard<std::mutex> lock(rules_mutex_);
    return rules_;
}

ToolPermission ToolPermissionEngine::resolve_permission(ToolPermission base) const {
    ToolPermission ceiling = admin_ceiling_.load(std::memory_order_relaxed);

    // Admin ceiling is MAXIMUM allowed permission
    // NEVER(2) > ASK(1) > ALWAYS(0) in restrictiveness
    // So ceiling must be >= base in enum value (less restrictive)
    if (static_cast<int>(base) > static_cast<int>(ceiling)) {
        return ceiling; // Base is more permissive than ceiling allows
    }
    return base;
}

bool ToolPermissionEngine::matches_pattern(const std::string& pattern, const std::string& value) const {
    if (pattern == "*" || pattern.empty()) return true;
    if (pattern == value) return true;

    // Simple glob matching: * = any chars, ? = single char
    size_t p = 0, v = 0;
    while (p < pattern.size() && v < value.size()) {
        if (pattern[p] == '*') {
            // Skip consecutive *
            while (p < pattern.size() && pattern[p] == '*') p++;
            if (p == pattern.size()) return true; // * at end matches everything
            // Match rest of pattern against remaining value
            char next = pattern[p];
            while (v < value.size()) {
                if (value[v] == next && matches_pattern(pattern.substr(p), value.substr(v))) {
                    return true;
                }
                v++;
            }
            return false;
        } else if (pattern[p] == '?' || pattern[p] == value[v]) {
            p++; v++;
        } else {
            return false;
        }
    }

    // Handle trailing * in pattern
    while (p < pattern.size() && pattern[p] == '*') p++;

    return p == pattern.size() && v == value.size();
}

bool ToolPermissionEngine::load_from_file() {
    namespace fs = std::filesystem;

    if (!fs::exists(config_.config_file)) {
        return false;
    }

    std::ifstream ifs(config_.config_file);
    if (!ifs) {
        yuki::core::Logger::instance().log(yuki::core::LogLevel::ERROR, "ToolPermissionEngine",
            "Failed to open config file: " + config_.config_file);
        return false;
    }

    nlohmann::json j;
    try {
        ifs >> j;
    } catch (const std::exception& e) {
        yuki::core::Logger::instance().log(yuki::core::LogLevel::ERROR, "ToolPermissionEngine",
            "JSON parse error: " + std::string(e.what()));
        return false;
    }

    if (!j.contains("schemaVersion") || !j["schemaVersion"].is_number_unsigned()) {
        yuki::core::Logger::instance().log(yuki::core::LogLevel::ERROR, "ToolPermissionEngine",
            "Missing schemaVersion");
        return false;
    }

    uint32_t version = j["schemaVersion"].get<uint32_t>();
    if (version != 1) {
        yuki::core::Logger::instance().log(yuki::core::LogLevel::ERROR, "ToolPermissionEngine",
            "Unsupported schema version: " + std::to_string(version));
        return false;
    }

    // Load admin ceiling
    if (j.contains("adminCeiling") && j["adminCeiling"].is_number_unsigned()) {
        admin_ceiling_.store(static_cast<ToolPermission>(j["adminCeiling"].get<uint8_t>()),
                             std::memory_order_relaxed);
    }

    // Load rules
    if (!j.contains("rules") || !j["rules"].is_array()) {
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(rules_mutex_);
        rules_.clear();

        for (const auto& r : j["rules"]) {
            try {
                PermissionRule rule;
                rule.action = static_cast<ToolAction>(r.value("action", 0));
                rule.permission = static_cast<ToolPermission>(r.value("permission", 2));
                rule.tool_pattern = r.value("toolPattern", "");
                rule.path_pattern = r.value("pathPattern", "");
                rule.reason = r.value("reason", "");
                rules_.push_back(std::move(rule));
            } catch (...) {
                // Skip malformed rules
            }
        }
    }

    return true;
}

bool ToolPermissionEngine::save_to_file() const {
    namespace fs = std::filesystem;

    fs::create_directories(fs::path(config_.config_file).parent_path());

    nlohmann::json j;
    j["schemaVersion"] = 1;
    j["adminCeiling"] = static_cast<uint8_t>(admin_ceiling_.load(std::memory_order_relaxed));

    j["rules"] = nlohmann::json::array();

    {
        std::lock_guard<std::mutex> lock(rules_mutex_);
        for (const auto& rule : rules_) {
            nlohmann::json r;
            r["action"] = static_cast<uint8_t>(rule.action);
            r["permission"] = static_cast<uint8_t>(rule.permission);
            r["toolPattern"] = rule.tool_pattern;
            r["pathPattern"] = rule.path_pattern;
            r["reason"] = rule.reason;
            j["rules"].push_back(std::move(r));
        }
    }

    // Atomic write
    auto tmp = config_.config_file + ".tmp";
    std::ofstream ofs(tmp);
    if (!ofs) {
        yuki::core::Logger::instance().log(yuki::core::LogLevel::ERROR, "ToolPermissionEngine",
            "Failed to open temp file for write");
        return false;
    }

    ofs << j.dump(2);
    if (!ofs) return false;
    ofs.flush();
    ofs.close();

    try {
        fs::rename(tmp, config_.config_file);
    } catch (const std::exception& e) {
        yuki::core::Logger::instance().log(yuki::core::LogLevel::ERROR, "ToolPermissionEngine",
            "Atomic rename failed: " + std::string(e.what()));
        return false;
    }

    return true;
}

ToolPermissionEngine::Stats ToolPermissionEngine::get_stats() const {
    Stats s;
    s.total_checks = stats_.total_checks;
    s.allowed = stats_.allowed;
    s.denied = stats_.denied;
    s.prompted = stats_.prompted;
    s.overridden = stats_.overridden;
    return s;
}

} // namespace system
} // namespace yuki