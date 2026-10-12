#pragma once
#include <vector>
#include <memory>
#include <unordered_set>
#include <chrono>
#include <mutex>
#include "yuki_shared_mutex_wrapper.h"
#include <string>
#include <iostream>
#include "brain/types/response_types.h"
#include "brain/memory/TurnMemory.h"

struct ToolParam {
    std::string name;
    std::string type;
    bool required = true;
};

class Tool {
public:
    virtual ~Tool() = default;
    virtual std::string id() const = 0;
    virtual std::string name() const = 0;
    virtual std::string description() const = 0;
    virtual std::vector<ToolParam> parameters() const = 0;
    virtual std::string examples() const = 0;
    virtual bool handles(IntentType intent) const = 0;
    virtual float match_score(const SlotMap& slots) const = 0;
    virtual float capability_score(const SlotMap& slots) const = 0;
    virtual bool is_idempotent() const { return false; }
    virtual ToolResult execute(const SlotMap& slots, int timeout_ms) = 0;
};

struct DynamicToolRecord {
    std::string name;
    std::string binary_path;
    float confidence = 0.50f;
    uint64_t updated_at = 0;
};

class DynamicTool : public Tool {
public:
    DynamicTool(const std::string& name, const std::string& binary_path, float confidence = 0.50f);

    std::string id() const override { return "dynamic_" + name_; }
    std::string name() const override { return name_; }
    std::string description() const override { return "Synthesized dynamic tool: " + name_; }
    std::vector<ToolParam> parameters() const override { return {{"query", "string", true}}; }
    std::string examples() const override { return "{\"query\":\"execute\"}"; }
    bool handles(IntentType /*intent*/) const override { return true; }
    float match_score(const SlotMap& /*slots*/) const override { return confidence_; }
    float capability_score(const SlotMap& /*slots*/) const override { return confidence_; }
    ToolResult execute(const SlotMap& slots, int timeout_ms) override;

    float confidence() const { return confidence_; }
    void setConfidence(float conf) { confidence_ = conf; }
    std::string binaryPath() const { return binary_path_; }

private:
    std::string name_;
    std::string binary_path_;
    float confidence_;
};

class ToolRegistry {
public:
    static ToolRegistry& instance();

    void register_tool(std::shared_ptr<Tool> tool);
    std::shared_ptr<Tool> resolve(IntentType intent, const SlotMap& slots);
    std::vector<std::shared_ptr<Tool>> active_tools() const;

    // Task 4.4 Dynamic Tool Management & SQLite Persistence
    bool registerDynamicTool(const std::string& name, const std::string& binary_path, float confidence = 0.50f);
    bool unregisterTool(const std::string& name);
    float getToolConfidence(const std::string& name) const;
    void updateToolConfidence(const std::string& name, bool success, float alpha = 0.15f);

    void initDatabase(const std::string& dbPath = "data/yuki_tools.db");
    void loadDynamicToolsFromDB();

    // Analytics
    void record_gap(IntentType intent, const SlotMap& slots, const std::string& reason);

private:
    ToolRegistry();
    ~ToolRegistry();

    void saveDynamicToolToDB(const DynamicToolRecord& rec);
    void deleteDynamicToolFromDB(const std::string& name);

    std::vector<std::shared_ptr<Tool>> tools_;
    std::unordered_map<std::string, std::shared_ptr<DynamicTool>> dynamic_tools_;
    mutable yuki::concurrency::shared_mutex mutex_;

    std::string db_path_{"data/yuki_tools.db"};
    void* db_ptr_{nullptr};
};

// Static helper for gap logging
class CapabilityGapLogger {
public:
    static void log(IntentType intent, const SlotMap& slots, const std::string& reason) {
        ToolRegistry::instance().record_gap(intent, slots, reason);
    }
};
