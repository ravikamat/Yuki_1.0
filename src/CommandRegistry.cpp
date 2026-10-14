// CommandRegistry.cpp — Dynamic command registration implementation

#include "CommandRegistry.h"
#include <algorithm>

CommandRegistry& CommandRegistry::instance() {
    static CommandRegistry registry;
    return registry;
}

bool CommandRegistry::registerCommand(const CommandInfo& cmd) {
    if (cmd.name.empty() || cmd.triggers.empty() || !cmd.handler) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Check for duplicate name
    if (commands_.find(cmd.name) != commands_.end()) {
        return false;
    }
    
    // Check for duplicate triggers
    for (const auto& trigger : cmd.triggers) {
        for (const auto& [name, existing] : commands_) {
            if (std::find(existing.triggers.begin(), existing.triggers.end(), trigger) != existing.triggers.end()) {
                return false;  // Trigger already registered
            }
        }
    }
    
    auto* stored = &commands_.emplace(cmd.name, cmd).first->second;
    triggers_dirty_ = true;
    return true;
}

bool CommandRegistry::unregisterCommand(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = commands_.find(name);
    if (it == commands_.end()) return false;
    commands_.erase(it);
    triggers_dirty_ = true;
    return true;
}

void CommandRegistry::rebuildTriggerIndex() const {
    if (!triggers_dirty_) return;
    
    sorted_triggers_.clear();
    sorted_triggers_.reserve(commands_.size() * 3);  // estimate
    
    for (const auto& [name, cmd] : commands_) {
        for (const auto& trigger : cmd.triggers) {
            sorted_triggers_.emplace_back(trigger, &cmd);
        }
    }
    
    // Sort by trigger length descending (longer matches first) then priority descending
    std::sort(sorted_triggers_.begin(), sorted_triggers_.end(),
        [](const auto& a, const auto& b) {
            if (a.first.size() != b.first.size()) {
                return a.first.size() > b.first.size();  // longer first
            }
            return a.second->priority > b.second->priority;
        });
    
    triggers_dirty_ = false;
}

std::pair<CommandResult, const CommandInfo*> CommandRegistry::findCommand(const std::string& normalizedInput) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (triggers_dirty_) rebuildTriggerIndex();
    
    CommandResult result;
    for (const auto& [trigger, cmd] : sorted_triggers_) {
        if (normalizedInput.find(trigger) != std::string::npos) {
            // Found matching trigger
            result = cmd->handler(normalizedInput);
            result.handled = true;
            return {result, cmd};
        }
    }
    
    return {{false, false, ""}, nullptr};
}

std::vector<CommandInfo> CommandRegistry::getAllCommands() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<CommandInfo> result;
    result.reserve(commands_.size());
    for (const auto& [name, cmd] : commands_) {
        result.push_back(cmd);
    }
    return result;
}

std::vector<CommandInfo> CommandRegistry::getCommandsByCategory(const std::string& category) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<CommandInfo> result;
    for (const auto& [name, cmd] : commands_) {
        if (cmd.category == category) {
            result.push_back(cmd);
        }
    }
    return result;
}

void CommandRegistry::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    commands_.clear();
    triggers_dirty_ = true;
}