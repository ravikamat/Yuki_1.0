#pragma once
// AtomicAdapter — streaming loader + query layer for the ATOMIC-2020
// if-then causal/social knowledge base.
//
// ATOMIC supplies the causal fabric ConceptNet lacks: "if X then PersonX
// feels / wants / intends Z", "X causes Y", "X before/after Y". Triples are
// parsed from TSV or CSV and can either be streamed straight into
// HdcSemanticGraph (parseStream) or indexed in memory for the small query
// surface below.
//
// Format notes: ATOMIC-2020 rows are `head <TAB> relation <TAB> tail [ ... ]`
// with relation possibly carrying a `/r/` prefix. Header rows and blank /
// comment lines are skipped. Everything is lowercased and underscore-joined
// phrases are normalized to spaces so triples match the HDC graph's concept
// naming.
#pragma once

#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace yuki::knowledge {

// A single ATOMIC assertion (subject/relation/object + provenance).
struct AtomicAssertion {
    std::string subject;
    std::string relation;
    std::string object;
    float       confidence = 1.0f;
    std::string source;      // e.g. "atomic2020"
};

class AtomicAdapter {
public:
    AtomicAdapter() = default;

    struct ParseStats {
        uint64_t parsed   = 0;   // data lines inspected
        uint64_t accepted = 0;   // valid triples emitted
        uint64_t filtered = 0;   // rejected (blank/header/short/non-english-ish)
    };

    using Callback = std::function<bool(const AtomicAssertion&)>;

    // Stream a TSV/CSV file. The callback returns false to stop early.
    // Never throws; a missing/unreadable file yields parsed == 0.
    ParseStats parseStream(const std::string& path, const Callback& cb);

    // Parse a file and index every assertion in memory.
    bool loadFromFile(const std::string& path);

    // ── Query surface (over the in-memory index) ──────────────────────────
    struct QueryResult {
        std::string subject;
        std::string relation;
        std::string object;
        float       confidence = 1.0f;
    };

    std::vector<QueryResult> querySubject(const std::string& subject,
                                          const std::string& relation = {}) const;
    std::vector<QueryResult> queryObject(const std::string& object,
                                         const std::string& relation = {}) const;
    std::vector<QueryResult> queryRelation(const std::string& relation) const;

    // Convenience social/causal lookups. All return the target concept.
    std::vector<std::string> getCauses(const std::string& event) const;
    std::vector<std::string> getEffects(const std::string& event) const;
    std::vector<std::string> getWants(const std::string& entity) const;
    std::vector<std::string> getNeeds(const std::string& entity) const;
    std::vector<std::string> getIntents(const std::string& entity) const;
    std::vector<std::string> getReactions(const std::string& entity) const;

    // Confidence that (s, r, o) is known. 0 when unseen; scaled by the
    // stored confidence when present.
    float computePlausibility(const std::string& subject,
                              const std::string& relation,
                              const std::string& object) const;
    bool  isPlausible(const std::string& subject,
                      const std::string& relation,
                      const std::string& object) const;

    struct Stats {
        size_t total_triples     = 0;
        size_t unique_subjects   = 0;
        size_t unique_objects    = 0;
        size_t unique_relations  = 0;
    };
    Stats getStats() const;

    // Normalization helpers (public so callers can match graph keys).
    static std::string normalizeRelation(const std::string& rel);
    static std::string normalizeConcept(const std::string& raw);

private:
    struct Triple {
        std::string subject;
        std::string relation;
        std::string object;
        float       confidence = 1.0f;
        std::string source;
    };

    // Caller must hold mutex_.
    size_t addTriple(const std::string& subject, const std::string& relation,
                     const std::string& object, float confidence,
                     const std::string& source);

    mutable std::mutex mutex_;
    std::vector<Triple> triples_;
    std::unordered_map<std::string, std::vector<size_t>> by_subject_;
    std::unordered_map<std::string, std::vector<size_t>> by_object_;
    std::unordered_map<std::string, std::vector<size_t>> by_relation_;
};

} // namespace yuki::knowledge
