#ifndef YUKI_TOOL_INTERFACE_H
#define YUKI_TOOL_INTERFACE_H

#include <cstdint>
#include <string>
#include <vector>
#include <memory>

namespace yuki {
namespace research {

enum class ToolRiskLevel : uint8_t {
    NONE = 0,
    LOW,
    MEDIUM,
    HIGH,
    CRITICAL
};

struct ToolSchema {
    uint64_t              inputSchemaHash = 0;
    uint64_t              outputSchemaHash = 0;
    std::vector<uint64_t> requiredCapabilityHashes;
};

struct ToolMetadata {
    std::string   toolId;
    ToolSchema    schema;
    float         reliability = 0.0f;
    uint32_t      cost = 1;
    ToolRiskLevel riskLevel = ToolRiskLevel::NONE;
};

enum class ToolStatus : uint8_t {
    SUCCESS = 0,
    UNKNOWN_ERROR,
    TIMEOUT,
    RATE_LIMITED,
    PERMISSION_DENIED,
    INVALID_INPUT,
    NETWORK_ERROR,
    SANDBOX_VIOLATION
};

class ToolResult {
public:
    uint64_t             nodeId = 0;
    ToolStatus           status = ToolStatus::UNKNOWN_ERROR;
    float                confidence = 0.0f;
    std::vector<uint8_t> payload;
    uint64_t             timestamp = 0;
    uint32_t             retryCount = 0;

    bool isSuccess() const { return status == ToolStatus::SUCCESS; }
};

class ToolInterface {
public:
    virtual ~ToolInterface() = default;
    virtual ToolResult execute(const std::vector<uint8_t>& input) = 0;
    virtual ToolMetadata getMetadata() const = 0;
    virtual bool isAvailable() const { return true; }
};

// M4: ActionTool marker for rollback-aware tools
class ActionTool : public ToolInterface {
public:
    virtual bool supportsRollback() const { return false; }
    virtual std::vector<uint8_t> getRollbackState() const { return {}; }
};

using ToolPtr = std::shared_ptr<ToolInterface>;

} // namespace research
} // namespace yuki

#endif
