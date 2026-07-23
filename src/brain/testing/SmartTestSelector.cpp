#include "brain/testing/SmartTestSelector.h"

namespace yuki {
namespace testing {

std::vector<uint64_t> SmartTestSelector::selectTests(const TestSuiteDAG& dag, SelectionTier tier) {
    std::vector<uint64_t> selected;
    if (dag.nodes.empty()) return selected;

    size_t limit = dag.nodes.size();
    if (tier == SelectionTier::QUICK_SCREEN) {
        limit = std::max<size_t>(1, dag.nodes.size() / 4);
    } else if (tier == SelectionTier::MEDIUM_REGRESSION) {
        limit = std::max<size_t>(1, dag.nodes.size() / 2);
    }

    for (size_t i = 0; i < limit && i < dag.nodes.size(); ++i) {
        selected.push_back(dag.nodes[i].testId);
    }

    return selected;
}

} // namespace testing
} // namespace yuki
