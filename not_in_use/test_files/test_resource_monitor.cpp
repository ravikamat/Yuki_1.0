#include "brain/system/ResourceMonitor.h"
#include <cassert>

int main() {
    yuki::system::ResourceMonitor monitor;

    auto metrics = monitor.sampleMetrics();
    assert(metrics.cpuPercent >= 0.0f);
    assert(monitor.recommendParallelism() > 0);
    assert(!monitor.isStarvationImminent());

    return 0;
}
