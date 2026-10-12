#pragma once

#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include <functional>

namespace yuki {
namespace brain {
namespace autonomy {

struct ToolResult {
    bool success = false;
    std::string output;
    std::string error;
    float latency_ms = 0.0f;
    std::chrono::steady_clock::time_point timestamp;
};

class Tool {
public:
    virtual ~Tool() = default;
    virtual std::string name() const = 0;
    virtual std::string description() const = 0;
    virtual bool canExecute(const std::string& params) const = 0;
    virtual ToolResult execute(const std::string& params) = 0;
};

class ToolRegistry {
public:
    ToolRegistry() = default;
    ~ToolRegistry() = default;
    
    void registerTool(std::unique_ptr<Tool> tool);
    void unregisterTool(const std::string& name);
    
    Tool* getTool(const std::string& name) const;
    std::vector<std::string> listTools() const;
    bool hasTool(const std::string& name) const;
    
private:
    std::unordered_map<std::string, std::unique_ptr<Tool>> tools_;
    mutable std::mutex mutex_;
};

} // namespace autonomy
} // namespace brain
} // namespace yuki