#include "brain/testing/data/SyntheticDataSource.h"
#include "brain/testing/metrics/MetricCalculator.h"
#include <cassert>

int main() {
    yuki::testing::SyntheticDataSource ds(50);
    assert(ds.hasNext());

    auto batch = ds.fetchBatch(10);
    assert(batch.size() == 10);

    yuki::testing::MetricCalculator calc;
    auto metrics = calc.compute(batch);
    assert(metrics.mean >= 0.0f);

    return 0;
}
