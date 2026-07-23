#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <chrono>

namespace yuki {
namespace metacognition {

struct AuditRecord {
    uint64_t audit_id = 0;
    uint64_t timestamp_ms = 0;
    std::string raw_input;
    std::string previous_raw_input;
    float precision_used = 0.0f;
    float belief_entropy = 0.0f;
    std::vector<float> intent_distribution;
    float intent_entropy = 0.0f;
    int selected_intent = -1;
    float intent_confidence = 0.0f;
    std::vector<float> domain_competences;
    uint32_t relevant_domain = 0;
    float relevant_competence = 0.0f;
    int selected_policy = -1;
    uint8_t execution_mode = 0;
    bool outcome_success = false;
    float outcome_precision = 0.0f;
    bool clarification_triggered = false;
    std::vector<float> predictor_weights;
    float predictor_bias = 0.0f;
};

struct AuditEntry {
    uint64_t timestamp = 0;
    std::string category;
    std::string details;
    uint64_t audit_id = 0;
};

class CognitiveAuditLog {
public:
    CognitiveAuditLog();
    uint64_t append(AuditRecord record);
    void updateOutcome(uint64_t audit_id, bool success, float precision,
                       bool clarification);
    bool get(uint64_t audit_id, AuditRecord* out) const;
    std::vector<AuditRecord> lastN(size_t n) const;
    std::vector<AuditRecord> lowCompetenceRecords(float threshold) const;
    std::vector<AuditRecord> anomalousPrecisionRecords(float tolerance) const;
    std::vector<AuditRecord> query(uint64_t startTime, uint64_t endTime, const std::string& category = "") const;
    size_t size() const noexcept { return records_.size(); }
    void clear() noexcept { records_.clear(); next_id_ = 1; }
    std::string serialize() const;
    void deserialize(const std::string& data);

private:
    std::vector<AuditRecord> records_;
    uint64_t next_id_ = 1;
    uint64_t nowMs() const;
};

} // namespace metacognition
} // namespace yuki
