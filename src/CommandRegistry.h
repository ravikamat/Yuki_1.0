#pragma once
// CommandRegistry.h — Dynamic command registration for CommandRouter
// Replaces hardcoded command lists with extensible plugin architecture

#include <string>
#include <functional>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <memory>

struct CommandResult {
    bool handled = false;
    bool success = false;
    std::string responseText;
};

using CommandHandler = std::function<CommandResult(const std::string& normalizedInput)>;

struct CommandInfo {
    std::string name;
    std::vector<std::string> triggers;  // normalized trigger phrases
    CommandHandler handler;
    std::string description;
    std::string category;  // "subsystem", "ui", "mobile", "custom"
    int priority = 0;      // higher = checked first
};

class CommandRegistry {
public:
    static CommandRegistry& instance();

    // Register a new command (plugins call this at startup)
    bool registerCommand(const CommandInfo& cmd);

    // Unregister a command
    bool unregisterCommand(const std::string& name);

    // Find matching command for normalized input
    // Returns CommandResult with handled=true if matched, plus the command info
    std::pair<CommandResult, const CommandInfo*> findCommand(const std::string& normalizedInput) const;

    // Get all registered commands (for help/status)
    std::vector<CommandInfo> getAllCommands() const;

    // Get commands by category
    std::vector<CommandInfo> getCommandsByCategory(const std::string& category) const;

    // Clear all commands (for testing/reset)
    void clear();

private:
    CommandRegistry() = default;
    mutable std::mutex mutex_;
    std::unordered_map<std::string, CommandInfo> commands_;  // name -> CommandInfo
    
    // For efficient prefix matching, also maintain sorted triggers
    mutable std::vector<std::pair<std::string, const CommandInfo*>> sorted_triggers_;
    mutable bool triggers_dirty_ = true;
    
    void rebuildTriggerIndex() const;
};