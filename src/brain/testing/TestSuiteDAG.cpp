#include "brain/testing/TestSuiteDAG.h"
#include <unordered_set>

namespace yuki {
namespace testing {

void TestSuiteDAG::buildWaves() {
    waves.clear();
    std::unordered_set<uint64_t> completed;

    while (completed.size() < nodes.size()) {
        std::vector<uint64_t> wave;
        for (const auto& node : nodes) {
            if (completed.count(node.testId)) continue;

            bool depsSatisfied = true;
            for (uint64_t depId : node.dependencies) {
                if (!completed.count(depId)) {
                    depsSatisfied = false;
                    break;
                }
            }

            if (depsSatisfied) {
                wave.push_back(node.testId);
            }
        }

        if (wave.empty()) break;

        for (uint64_t tid : wave) {
            completed.insert(tid);
        }
        waves.push_back(wave);
    }
}

bool TestSuiteDAG::isComplete() const {
    for (const auto& node : nodes) {
        if (!node.executed) return false;
    }
    return true;
}

} // namespace testing
} // namespace yuki
