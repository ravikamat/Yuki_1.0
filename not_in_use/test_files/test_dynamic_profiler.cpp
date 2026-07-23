#include "brain/introspection/DynamicProfiler.h"
#include <cassert>

int main() {
    yuki::introspection::DynamicProfiler profiler;

    auto sysProf = profiler.profileSystem();
    assert(sysProf.ramUsageMb > 0.0f);

    auto causes = profiler.backtrack("high_cpu_spike");
    assert(!causes.empty());

    return 0;
}
