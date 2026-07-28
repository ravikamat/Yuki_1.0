#include "brain/self/SelfModel.h"
#include "brain/self/TheoryOfMind.h"
#include "brain/emotion/ValenceArousalModel.h"
#include "brain/organism/ConfidenceCalibrator.h"

#include <iostream>
#include <cassert>

int main() {
    using namespace yuki::self;
    using namespace yuki::emotion;
    using namespace yuki::organism;

    std::cout << "[TEST] IdentityPersistence starting..." << std::endl;

    IdentityPersistence pers("data/brain/test_identity.db");
    assert(pers.initializeSchema());

    SelfModel self;
    TheoryOfMind tom;
    ValenceArousalModel emotion;
    ConfidenceCalibrator calibrator;

    // Test saving identity snapshot
    bool saved = pers.saveIdentity(self, tom, emotion, calibrator, "1.0.0");
    assert(saved);
    assert(pers.getSnapshotCount() >= 1);

    // Test loading identity
    SelfModel self2;
    TheoryOfMind tom2;
    ValenceArousalModel emotion2;
    ConfidenceCalibrator calibrator2;

    bool loaded = pers.loadLatestIdentity(self2, tom2, emotion2, calibrator2);
    assert(loaded);

    // Test autobiographical entry
    std::vector<uint8_t> content = {'H', 'E', 'L', 'L', 'O'};
    bool addedEntry = pers.addAutobiographicalEntry("SESSION_START", content, 1);
    assert(addedEntry);
    assert(pers.getEntryCount() >= 1);

    auto entries = pers.getAutobiographicalEntries(10);
    assert(!entries.empty());
    assert(entries[0].entryType == "SESSION_START");

    std::string summary = pers.generateNarrativeSummary();
    assert(!summary.empty());

    // Hash chain verification
    assert(pers.verifyHashChain());

    std::cout << "[TEST] IdentityPersistence PASSED!" << std::endl;
    return 0;
}
