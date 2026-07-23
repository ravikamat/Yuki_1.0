#include "CognitiveAuditLog.h"
#include <algorithm>
#include <sstream>

namespace yuki {
namespace metacognition {

CognitiveAuditLog::CognitiveAuditLog() = default;

uint64_t CognitiveAuditLog::append(AuditRecord record) {
    record.audit_id = next_id_++;
    record.timestamp_ms = nowMs();
    records_.push_back(std::move(record));
    return records_.back().audit_id;
}

void CognitiveAuditLog::updateOutcome(uint64_t audit_id, bool success,
                                       float precision, bool clarification) {
    for (auto& r : records_) {
        if (r.audit_id == audit_id) {
            r.outcome_success = success;
            r.outcome_precision = precision;
            r.clarification_triggered = clarification;
            return;
        }
    }
}

bool CognitiveAuditLog::get(uint64_t audit_id, AuditRecord* out) const {
    if (!out) return false;
    for (const auto& r : records_) {
        if (r.audit_id == audit_id) {
            *out = r;
            return true;
        }
    }
    return false;
}

std::vector<AuditRecord> CognitiveAuditLog::lastN(size_t n) const {
    std::vector<AuditRecord> result;
    size_t count = std::min(n, records_.size());
    result.reserve(count);
    for (size_t i = records_.size() - count; i < records_.size(); ++i) {
        result.push_back(records_[i]);
    }
    return result;
}

std::vector<AuditRecord> CognitiveAuditLog::lowCompetenceRecords(float threshold) const {
    std::vector<AuditRecord> result;
    for (const auto& r : records_) {
        if (r.relevant_competence < threshold) {
            result.push_back(r);
        }
    }
    return result;
}

std::vector<AuditRecord> CognitiveAuditLog::anomalousPrecisionRecords(float tolerance) const {
    std::vector<AuditRecord> result;
    for (const auto& r : records_) {
        if (std::abs(r.precision_used - r.outcome_precision) > tolerance) {
            result.push_back(r);
        }
    }
    return result;
}

std::vector<AuditRecord> CognitiveAuditLog::query(uint64_t startTime, uint64_t endTime, const std::string& /*category*/) const {
    std::vector<AuditRecord> result;
    for (const auto& r : records_) {
        if (r.timestamp_ms >= startTime && (endTime == 0 || r.timestamp_ms <= endTime)) {
            result.push_back(r);
        }
    }
    return result;
}

std::string CognitiveAuditLog::serialize() const {
    std::ostringstream oss;
    oss << records_.size() << "\n";
    for (const auto& r : records_) {
        oss << r.audit_id << "|" << r.timestamp_ms << "|"
            << r.precision_used << "|" << r.belief_entropy << "|"
            << r.intent_entropy << "|" << r.selected_intent << "|"
            << r.intent_confidence << "|" << r.relevant_domain << "|"
            << r.relevant_competence << "|" << r.selected_policy << "|"
            << static_cast<int>(r.execution_mode) << "|"
            << (r.outcome_success ? 1 : 0) << "|" << r.outcome_precision << "|"
            << (r.clarification_triggered ? 1 : 0) << "|"
            << r.predictor_weights.size();
        for (float w : r.predictor_weights) {
            oss << "|" << w;
        }
        oss << "|" << r.predictor_bias << "\n";
    }
    return oss.str();
}

void CognitiveAuditLog::deserialize(const std::string& data) {
    records_.clear();
    next_id_ = 1;
    if (data.empty()) return;

    std::istringstream iss(data);
    size_t count = 0;
    iss >> count;
    std::string line;
    std::getline(iss, line);

    for (size_t i = 0; i < count; ++i) {
        std::getline(iss, line);
        if (line.empty()) continue;

        AuditRecord r;
        std::istringstream ls(line);
        std::string token;

        auto next_token = [&ls, &token]() -> bool {
            return static_cast<bool>(std::getline(ls, token, '|'));
        };


        if (!next_token()) continue;
        r.audit_id = std::stoull(token);
        if (r.audit_id >= next_id_) next_id_ = r.audit_id + 1;

        if (next_token()) r.timestamp_ms = std::stoull(token);
        if (next_token()) r.precision_used = std::stof(token);
        if (next_token()) r.belief_entropy = std::stof(token);
        if (next_token()) r.intent_entropy = std::stof(token);
        if (next_token()) r.selected_intent = std::stoi(token);
        if (next_token()) r.intent_confidence = std::stof(token);
        if (next_token()) r.relevant_domain = static_cast<uint32_t>(std::stoul(token));
        if (next_token()) r.relevant_competence = std::stof(token);
        if (next_token()) r.selected_policy = std::stoi(token);
        if (next_token()) r.execution_mode = static_cast<uint8_t>(std::stoi(token));
        if (next_token()) r.outcome_success = (std::stoi(token) != 0);
        if (next_token()) r.outcome_precision = std::stof(token);
        if (next_token()) r.clarification_triggered = (std::stoi(token) != 0);
        if (next_token()) {
            size_t wcount = std::stoull(token);
            r.predictor_weights.reserve(wcount);
            for (size_t j = 0; j < wcount; ++j) {
                if (next_token()) {
                    r.predictor_weights.push_back(std::stof(token));
                }
            }
        }
        if (next_token()) r.predictor_bias = std::stof(token);

        records_.push_back(std::move(r));
    }
}

uint64_t CognitiveAuditLog::nowMs() const {
    using namespace std::chrono;
    return static_cast<uint64_t>(
        duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count()
    );
}

} // namespace metacognition
} // namespace yuki
