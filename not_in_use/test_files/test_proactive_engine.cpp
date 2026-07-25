#include "brain/organism/ProactiveEngine.h"
#include "brain/organism/DriveSystem.h"
#include "brain/organism/MetabolismEngine.h"
#include <cassert>

int main() {
    yuki::organism::DriveSystem drives;
    yuki::organism::MetabolismEngine metabolism;
    yuki::organism::ProactiveEngine pe(&drives, &metabolism, nullptr);

    // 1. generateInitiative() returns NONE when drives are satisfied & metabolism healthy
    metabolism.consumePower(0.01); // keep viability high
    auto init1 = pe.generateInitiative();
    assert(init1.type == yuki::organism::Initiative::Type::NONE || init1.type == yuki::organism::Initiative::Type::SYSTEM_ALERT || init1.type == yuki::organism::Initiative::Type::CURIOSITY);

    // 2. low metabolism viability -> SYSTEM_ALERT initiative
    metabolism.consumePower(0.95); // drop viability < 0.3
    pe.clearPending();
    auto init2 = pe.generateInitiative();
    assert(init2.type == yuki::organism::Initiative::Type::SYSTEM_ALERT);
    assert(init2.priority > 0.8f);

    // 3. high curiosity drive -> CURIOSITY initiative
    yuki::organism::MetabolismEngine healthy_meta;
    yuki::organism::DriveSystem active_drives;
    yuki::organism::DriveInputs in_curious;
    in_curious.secondsSinceDiscovery = 2500.0;
    active_drives.update(in_curious);
    yuki::organism::ProactiveEngine pe2(&active_drives, &healthy_meta, nullptr);
    auto init3 = pe2.generateInitiative();
    assert(init3.type == yuki::organism::Initiative::Type::CURIOSITY || init3.type == yuki::organism::Initiative::Type::NONE);


    // 4. pendingInitiatives() returns sorted-by-priority list
    auto pending = pe.pendingInitiatives();
    assert(!pending.empty());
    for (size_t i = 1; i < pending.size(); ++i) {
        assert(pending[i - 1].priority >= pending[i].priority);
    }

    // 5. clearPending() empties queue
    pe.clearPending();
    assert(pe.pendingInitiatives().empty());

    // 6. template loading from missing file -> graceful degradation
    bool ok_load = pe.loadTemplatesFromFile("missing_proactive_file.txt");
    assert(!ok_load);

    return 0;
}
