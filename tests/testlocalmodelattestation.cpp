#include "src/brain/language/LocalModelAttestation.h"
#include <cassert>
#include <iostream>

int main() {
    std::cout << "Running testlocalmodelattestation...\n";
    using namespace yuki::brain::language;

    LocalModelAttestationRecord record;
    record.schemaVersion = 2;
    record.verified = true;
    record.modelFingerprintSha256 = "model-hash-12345";
    record.llamaServerFingerprintSha256 = "server-hash-67890";
    record.deviceLuid = "luid-intel-gpu";
    record.expiresAtUnixMs = 2000000000000ULL; // Far future

    assert(!record.isExpired(1000000000000ULL));
    assert(record.matchesRuntime("model-hash-12345", "server-hash-67890", "luid-intel-gpu"));

    // Changed model fingerprint invalidates attestation
    assert(!record.matchesRuntime("model-hash-CHANGED", "server-hash-67890", "luid-intel-gpu"));

    // Changed server fingerprint invalidates attestation
    assert(!record.matchesRuntime("model-hash-12345", "server-hash-CHANGED", "luid-intel-gpu"));

    // Expiration invalidates attestation
    assert(record.isExpired(3000000000000ULL));

    std::cout << "[PASS] testlocalmodelattestation completed cleanly.\n";
    return 0;
}
