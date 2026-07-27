#include <cassert>
#include "brain/action/core/ExecutionReport.h"
#include "brain/action/core/ActionGoal.h"

using namespace yuki::action;

int main() {
    ExecutionReport report;
    report.reportId = 42;

    ActionResult r1;
    r1.status = ActionStatus::SUCCESS;
    r1.confidence = 0.9f;
    report.results.push_back(r1);

    ActionResult r2;
    r2.status = ActionStatus::FAILED;
    r2.confidence = 0.1f;
    report.results.push_back(r2);

    report.computeOverallSuccess();
    assert(report.overallSuccess == 0.5f);

    auto serialized = report.serialize();
    assert(!serialized.empty());

    return 0;
}
