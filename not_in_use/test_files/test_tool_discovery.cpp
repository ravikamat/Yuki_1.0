#include "brain/research/discovery/ToolDiscovery.h"
#include <cassert>

int main() {
    yuki::research::ToolDiscovery discovery;
    discovery.scanPathEnvironment();

    assert(discovery.getDiscoveredCount() > 0);

    return 0;
}
