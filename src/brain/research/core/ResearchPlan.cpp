#include "brain/research/core/ResearchPlan.h"
#include <algorithm>
#include <unordered_set>

namespace yuki {
namespace research {

void ResearchPlan::buildWaves() {
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
            for (uint64_t depId : node.inputNodeIds) {
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

std::vector<uint64_t> ResearchPlan::getReadyNodes() const {
    std::vector<uint64_t> ready;
    std::unordered_set<uint64_t> executedIds;
    for (const auto& node : nodes) {
        if (node.executed) executedIds.insert(node.nodeId);
    }

    for (const auto& node : nodes) {
        if (node.executed) continue;
        bool canRun = true;
        for (uint64_t dep : node.inputNodeIds) {
            if (!executedIds.count(dep)) {
                canRun = false;
                break;
            }
        }
        if (canRun) ready.push_back(node.nodeId);
    }
    return ready;
}

bool ResearchPlan::isComplete() const {
    for (const auto& node : nodes) {
        if (!node.executed && node.type != NodeType::HUMAN_CLARIFICATION) {
            return false;
        }
    }
    return true;
}

} // namespace research
} // namespace yuki
