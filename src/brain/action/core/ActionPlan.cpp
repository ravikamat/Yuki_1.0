#include "brain/action/core/ActionPlan.h"
#include <algorithm>
#include <unordered_set>
#include <cstring>
#include <string>

namespace yuki {
namespace action {

void ActionPlan::buildWaves() {
    executionWaves.clear();
    std::unordered_set<uint64_t> completed;
    std::unordered_set<uint64_t> inPlan;

    for (const auto& node : nodes) {
        inPlan.insert(node.nodeId);
    }

    while (completed.size() < nodes.size()) {
        std::vector<uint64_t> wave;
        for (const auto& node : nodes) {
            if (completed.count(node.nodeId)) continue;

            bool depsSatisfied = true;
            for (uint64_t depId : node.inputDeps) {
                if (inPlan.count(depId) && !completed.count(depId)) {
                    depsSatisfied = false;
                    break;
                }
            }

            if (depsSatisfied) {
                wave.push_back(node.nodeId);
            }
        }

        if (wave.empty() && completed.size() < nodes.size()) {
            break;
        }

        for (uint64_t nid : wave) {
            completed.insert(nid);
        }
        executionWaves.push_back(wave);
    }
}

std::vector<uint64_t> ActionPlan::getReadyNodes() const {
    std::vector<uint64_t> ready;
    std::unordered_set<uint64_t> executedIds;
    for (const auto& node : nodes) {
        if (node.executed) executedIds.insert(node.nodeId);
    }

    for (const auto& node : nodes) {
        if (node.executed) continue;
        bool canRun = true;
        for (uint64_t dep : node.inputDeps) {
            if (!executedIds.count(dep)) {
                canRun = false;
                break;
            }
        }
        if (canRun) ready.push_back(node.nodeId);
    }
    return ready;
}

bool ActionPlan::isComplete() const {
    for (const auto& node : nodes) {
        if (!node.executed && node.status != ActionStatus::ROLLED_BACK) {
            return false;
        }
    }
    return true;
}

bool ActionPlan::hasFailedNodes() const {
    for (const auto& node : nodes) {
        if (node.status == ActionStatus::FAILED ||
            node.status == ActionStatus::ROLLED_BACK) {
            return true;
        }
    }
    return false;
}

std::vector<uint64_t> ActionPlan::getFailedNodes() const {
    std::vector<uint64_t> failed;
    for (const auto& node : nodes) {
        if (node.status == ActionStatus::FAILED ||
            node.status == ActionStatus::ROLLED_BACK) {
            failed.push_back(node.nodeId);
        }
    }
    return failed;
}

// Helper binary writers & readers
namespace {

void writeUint32(std::vector<uint8_t>& buf, uint32_t val) {
    buf.push_back(static_cast<uint8_t>((val >> 0) & 0xFF));
    buf.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
    buf.push_back(static_cast<uint8_t>((val >> 16) & 0xFF));
    buf.push_back(static_cast<uint8_t>((val >> 24) & 0xFF));
}

void writeUint64(std::vector<uint8_t>& buf, uint64_t val) {
    writeUint32(buf, static_cast<uint32_t>(val & 0xFFFFFFFF));
    writeUint32(buf, static_cast<uint32_t>((val >> 32) & 0xFFFFFFFF));
}

void writeFloat(std::vector<uint8_t>& buf, float val) {
    uint32_t u = 0;
    std::memcpy(&u, &val, sizeof(float));
    writeUint32(buf, u);
}

void writeString(std::vector<uint8_t>& buf, const std::string& str) {
    writeUint32(buf, static_cast<uint32_t>(str.size()));
    buf.insert(buf.end(), str.begin(), str.end());
}

uint32_t computeFNV1a(const uint8_t* data, size_t len) {
    uint32_t hash = 0x811c9dc5;
    for (size_t i = 0; i < len; ++i) {
        hash ^= data[i];
        hash *= 0x01000193;
    }
    return hash;
}

bool readUint32(const std::vector<uint8_t>& buf, size_t& offset, uint32_t& val) {
    if (offset + 4 > buf.size()) return false;
    val = static_cast<uint32_t>(buf[offset]) |
          (static_cast<uint32_t>(buf[offset + 1]) << 8) |
          (static_cast<uint32_t>(buf[offset + 2]) << 16) |
          (static_cast<uint32_t>(buf[offset + 3]) << 24);
    offset += 4;
    return true;
}

bool readUint64(const std::vector<uint8_t>& buf, size_t& offset, uint64_t& val) {
    uint32_t low = 0, high = 0;
    if (!readUint32(buf, offset, low) || !readUint32(buf, offset, high)) return false;
    val = (static_cast<uint64_t>(high) << 32) | static_cast<uint64_t>(low);
    return true;
}

bool readFloat(const std::vector<uint8_t>& buf, size_t& offset, float& val) {
    uint32_t u = 0;
    if (!readUint32(buf, offset, u)) return false;
    std::memcpy(&val, &u, sizeof(float));
    return true;
}

bool readString(const std::vector<uint8_t>& buf, size_t& offset, std::string& str) {
    uint32_t len = 0;
    if (!readUint32(buf, offset, len)) return false;
    if (offset + len > buf.size()) return false;
    str.assign(reinterpret_cast<const char*>(&buf[offset]), len);
    offset += len;
    return true;
}

} // anonymous namespace

std::vector<uint8_t> ActionPlan::serialize() const {
    std::vector<uint8_t> payload;

    // Header metadata
    writeUint64(payload, planId);
    writeUint64(payload, parentRequestId);
    writeUint32(payload, rollbackBudget);
    writeFloat(payload, aggregateRiskScore);

    // Nodes
    writeUint32(payload, static_cast<uint32_t>(nodes.size()));
    for (const auto& node : nodes) {
        writeUint64(payload, node.nodeId);
        writeUint32(payload, static_cast<uint32_t>(node.type));
        writeUint32(payload, static_cast<uint32_t>(node.status));
        writeFloat(payload, node.confidenceThreshold);
        writeUint32(payload, node.maxRetries);
        payload.push_back(node.executed ? 1 : 0);
        payload.push_back(node.isDestructive ? 1 : 0);
        writeUint64(payload, node.checkpointBefore);
        writeUint64(payload, node.checkpointAfter);
        writeUint64(payload, node.associatedGoalId);

        // Input deps
        writeUint32(payload, static_cast<uint32_t>(node.inputDeps.size()));
        for (uint64_t dep : node.inputDeps) {
            writeUint64(payload, dep);
        }

        // Output deps
        writeUint32(payload, static_cast<uint32_t>(node.outputDeps.size()));
        for (uint64_t dep : node.outputDeps) {
            writeUint64(payload, dep);
        }
    }

    // Execution waves
    writeUint32(payload, static_cast<uint32_t>(executionWaves.size()));
    for (const auto& wave : executionWaves) {
        writeUint32(payload, static_cast<uint32_t>(wave.size()));
        for (uint64_t nid : wave) {
            writeUint64(payload, nid);
        }
    }

    // Checkpoint IDs
    writeUint32(payload, static_cast<uint32_t>(checkpointIds.size()));
    for (uint64_t cid : checkpointIds) {
        writeUint64(payload, cid);
    }

    // Build final binary packet with Schema Header
    std::vector<uint8_t> result;
    writeUint32(result, kMagic);   // "YAPL"
    writeUint32(result, kVersion); // 1
    writeUint32(result, static_cast<uint32_t>(payload.size()));

    uint32_t checksum = computeFNV1a(payload.data(), payload.size());
    writeUint32(result, checksum);

    result.insert(result.end(), payload.begin(), payload.end());
    return result;
}

std::optional<ActionPlan> ActionPlan::deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < 16) return std::nullopt; // header incomplete

    size_t offset = 0;
    uint32_t magic = 0, version = 0, payloadSize = 0, expectedChecksum = 0;
    if (!readUint32(data, offset, magic) || magic != kMagic) return std::nullopt;
    if (!readUint32(data, offset, version) || version != kVersion) return std::nullopt;
    if (!readUint32(data, offset, payloadSize)) return std::nullopt;
    if (!readUint32(data, offset, expectedChecksum)) return std::nullopt;

    if (data.size() - offset < payloadSize) return std::nullopt;

    uint32_t actualChecksum = computeFNV1a(data.data() + offset, payloadSize);
    if (actualChecksum != expectedChecksum) return std::nullopt; // Checksum corrupted

    ActionPlan plan;
    if (!readUint64(data, offset, plan.planId)) return std::nullopt;
    if (!readUint64(data, offset, plan.parentRequestId)) return std::nullopt;
    if (!readUint32(data, offset, plan.rollbackBudget)) return std::nullopt;
    if (!readFloat(data, offset, plan.aggregateRiskScore)) return std::nullopt;

    uint32_t nodeCount = 0;
    if (!readUint32(data, offset, nodeCount)) return std::nullopt;
    plan.nodes.reserve(nodeCount);

    for (uint32_t i = 0; i < nodeCount; ++i) {
        ActionNode node;
        uint32_t typeU32 = 0, statusU32 = 0;
        uint8_t execU8 = 0, destrU8 = 0;

        if (!readUint64(data, offset, node.nodeId)) return std::nullopt;
        if (!readUint32(data, offset, typeU32)) return std::nullopt;
        if (!readUint32(data, offset, statusU32)) return std::nullopt;
        if (!readFloat(data, offset, node.confidenceThreshold)) return std::nullopt;
        if (!readUint32(data, offset, node.maxRetries)) return std::nullopt;
        if (offset >= data.size()) return std::nullopt;
        execU8 = data[offset++];
        if (offset >= data.size()) return std::nullopt;
        destrU8 = data[offset++];

        node.type = static_cast<ActionType>(typeU32);
        node.status = static_cast<ActionStatus>(statusU32);
        node.executed = (execU8 != 0);
        node.isDestructive = (destrU8 != 0);

        if (!readUint64(data, offset, node.checkpointBefore)) return std::nullopt;
        if (!readUint64(data, offset, node.checkpointAfter)) return std::nullopt;
        if (!readUint64(data, offset, node.associatedGoalId)) return std::nullopt;

        uint32_t inDepCount = 0;
        if (!readUint32(data, offset, inDepCount)) return std::nullopt;
        node.inputDeps.reserve(inDepCount);
        for (uint32_t d = 0; d < inDepCount; ++d) {
            uint64_t depId = 0;
            if (!readUint64(data, offset, depId)) return std::nullopt;
            node.inputDeps.push_back(depId);
        }

        uint32_t outDepCount = 0;
        if (!readUint32(data, offset, outDepCount)) return std::nullopt;
        node.outputDeps.reserve(outDepCount);
        for (uint32_t d = 0; d < outDepCount; ++d) {
            uint64_t depId = 0;
            if (!readUint64(data, offset, depId)) return std::nullopt;
            node.outputDeps.push_back(depId);
        }

        plan.nodes.push_back(std::move(node));
    }

    uint32_t waveCount = 0;
    if (!readUint32(data, offset, waveCount)) return std::nullopt;
    plan.executionWaves.reserve(waveCount);
    for (uint32_t w = 0; w < waveCount; ++w) {
        uint32_t waveSize = 0;
        if (!readUint32(data, offset, waveSize)) return std::nullopt;
        std::vector<uint64_t> wave;
        wave.reserve(waveSize);
        for (uint32_t nid = 0; nid < waveSize; ++nid) {
            uint64_t nodeVal = 0;
            if (!readUint64(data, offset, nodeVal)) return std::nullopt;
            wave.push_back(nodeVal);
        }
        plan.executionWaves.push_back(std::move(wave));
    }

    uint32_t cidCount = 0;
    if (!readUint32(data, offset, cidCount)) return std::nullopt;
    plan.checkpointIds.reserve(cidCount);
    for (uint32_t c = 0; c < cidCount; ++c) {
        uint64_t cid = 0;
        if (!readUint64(data, offset, cid)) return std::nullopt;
        plan.checkpointIds.push_back(cid);
    }

    return plan;
}

// ══════════════════════════════════════════════════════════════════════════════
// ExecutionReport
// ══════════════════════════════════════════════════════════════════════════════

void ExecutionReport::computeOverallSuccess() {
    if (results.empty()) {
        overallSuccess = 0.0f;
        return;
    }

    uint32_t successCount = 0;
    for (const auto& result : results) {
        if (result.status == ActionStatus::SUCCESS) {
            successCount++;
        }
    }

    overallSuccess = static_cast<float>(successCount) / static_cast<float>(results.size());
}

std::vector<uint8_t> ExecutionReport::serialize() const {
    std::vector<uint8_t> data;
    data.resize(sizeof(uint64_t) * 3 + sizeof(float) * 2 + sizeof(uint32_t));
    size_t offset = 0;

    std::memcpy(data.data() + offset, &reportId, sizeof(reportId));
    offset += sizeof(reportId);
    std::memcpy(data.data() + offset, &startTime, sizeof(startTime));
    offset += sizeof(startTime);
    std::memcpy(data.data() + offset, &endTime, sizeof(endTime));
    offset += sizeof(endTime);
    std::memcpy(data.data() + offset, &overallSuccess, sizeof(overallSuccess));
    offset += sizeof(overallSuccess);
    std::memcpy(data.data() + offset, &totalDurationMs, sizeof(totalDurationMs));
    offset += sizeof(totalDurationMs);

    return data;
}

} // namespace action
} // namespace yuki
