#pragma once
// TruthGate — Claim extraction, verification, and uncertainty marking
// Stage 2.3: Anti-hallucination gate before emission

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <chrono>

#include "brain/memory/CognitiveMemoryFabric.h"
#include "brain/core/Logger.h"

namespace yuki {
namespace truth {

enum class ClaimType : uint8_t {
    FACTUAL       = 0,  // "Paris is the capital of France"
    QUANTITATIVE  = 1,  // "Population is 2.1M"
    TEMPORAL      = 2,  // "Event happened in 2020"
    CAUSAL        = 3,  // "X causes Y"
    PROCEDURAL    = 4,  // "To do X, run command Y"
    DEFINITIONAL  = 5,  // "A bracket is a syntax element"
    OPINION       = 6,  // "This is the best approach"
    UNKNOWN       = 7
};

enum class VerificationStatus : uint8_t {
    VERIFIED      = 0,  // Found in T1/T2 with high confidence
    PARTIAL       = 1,  // Partial match, needs qualification
    CONTRADICTED  = 2,  // T1/T2 says otherwise
    UNVERIFIED    = 3,  // No evidence found in memory
    UNVERIFIABLE  = 4   // Opinion, prediction, or uncheckable
};

struct Claim {
    std::string text;                    // Original claim text
    ClaimType type = ClaimType::UNKNOWN;
    std::string subject;                 // Entity being claimed about
    std::string predicate;               // Relation/property
    std::string object;                  // Value/claim
    float confidence = 0.0f;             // Extractor confidence
    std::string source_span;             // Where in response this claim appears
};

struct VerificationResult {
    Claim claim;
    VerificationStatus status;
    float evidence_confidence = 0.0f;    // Confidence from memory
    std::vector<std::string> evidence_ids; // T1 episode IDs or T2 concept names
    std::string uncertainty_marker;      // Human-readable uncertainty phrase
    bool should_emit = true;             // False if claim should be suppressed
};

struct TruthGateConfig {
    float min_verification_confidence = 0.7f;  // Threshold for VERIFIED
    float partial_threshold = 0.4f;            // Threshold for PARTIAL
    bool emit_uncertain = true;                // Emit with uncertainty marker
    bool suppress_unverified = false;          // Suppress UNVERIFIED claims
    bool suppress_unverifiable = false;        // Suppress UNVERIFIABLE claims
    size_t max_claims_per_response = 20;       // Limit claims processed
};

class TruthGate {
public:
    explicit TruthGate(const TruthGateConfig& cfg = {});
    ~TruthGate() = default;

    // Initialize with memory fabric
    bool initialize(yuki::brain::memory::CognitiveMemoryFabric* cmf);

    // Process a response before emission
    // Returns modified response with uncertainty markers inserted
    std::string process_response(const std::string& raw_response);

    // Get verification results for audit
    std::vector<VerificationResult> get_last_results() const;

    // Stats
    struct Stats {
        uint64_t total_claims = 0;
        uint64_t verified = 0;
        uint64_t partial = 0;
        uint64_t contradicted = 0;
        uint64_t unverified = 0;
        uint64_t unverifiable = 0;
        uint64_t suppressed = 0;
    };
    Stats get_stats() const;

private:
    TruthGateConfig config_;
    yuki::brain::memory::CognitiveMemoryFabric* cmf_ = nullptr;
    mutable std::mutex results_mutex_;
    std::vector<VerificationResult> last_results_;
    Stats stats_;

    // Claim extraction (simple pattern-based, replaceable with ML)
    std::vector<Claim> extract_claims(const std::string& text) const;

    // Classify claim type
    ClaimType classify_claim(const std::string& claim_text) const;

    // Verify single claim against T1/T2
    VerificationResult verify_claim(const Claim& claim) const;

    // Query T1 episodic memory
    std::optional<float> query_episodic(const Claim& claim) const;

    // Query T2 semantic graph
    std::optional<float> query_semantic(const Claim& claim) const;

    // Generate uncertainty marker for status
    std::string make_uncertainty_marker(VerificationStatus status, const Claim& claim) const;

    // Insert markers into response
    std::string insert_markers(const std::string& response,
                               const std::vector<VerificationResult>& results) const;
};

} // namespace truth
} // namespace yuki