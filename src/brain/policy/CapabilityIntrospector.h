// ============================================================================
// YUKI v1.0 — Capability Introspection Engine
// Injects CapabilityGraph summary into LLM prompts for META_QUESTION intent.
// ============================================================================
#pragma once

#include <string>
#include <vector>
#include <memory>
#include <sstream>

namespace yuki {
namespace capability {
class CapabilityGraph;
struct CapabilityNode;
}

namespace policy {

// ---------------------------------------------------------------------------
// CapabilitySummary
// Structured representation of what YUKI can do, for prompt injection.
// ---------------------------------------------------------------------------
struct CapabilitySummary {
    std::string domain;           // e.g., "language", "memory", "reasoning"
    std::string capability;       // e.g., "causal_inference", "episodic_recall"
    std::string description;      // Human-readable description
    float confidence;             // 0.0–1.0, how well this capability works
    bool isActive;                // Currently loaded and functional?
    std::vector<std::string> dependencies; // Required subsystems
};

// ---------------------------------------------------------------------------
// CapabilityIntrospector
// Queries the CapabilityGraph and formats a prompt preamble.
// ---------------------------------------------------------------------------
class CapabilityIntrospector {
public:
    explicit CapabilityIntrospector(const yuki::capability::CapabilityGraph* graph);
    ~CapabilityIntrospector() = default;

    // Non-copyable
    CapabilityIntrospector(const CapabilityIntrospector&) = delete;
    CapabilityIntrospector& operator=(const CapabilityIntrospector&) = delete;

    // -----------------------------------------------------------------------
    // Generate a prompt injection block for the LLM.
    // Called by IntentResponseRouter when cognitive_intent == META_QUESTION.
    // -----------------------------------------------------------------------
    std::string generatePromptPreamble() const;

    // -----------------------------------------------------------------------
    // Generate a focused preamble for a specific domain query.
    // e.g., user asks "Can you do physics?" → only physics capabilities.
    // -----------------------------------------------------------------------
    std::string generateDomainPreamble(const std::string& domain) const;

    // -----------------------------------------------------------------------
    // JSON-serializable summary for structured logging / YNC training data.
    // -----------------------------------------------------------------------
    std::string toJsonSummary() const;

    // -----------------------------------------------------------------------
    // Runtime health check: are all critical capabilities active?
    // -----------------------------------------------------------------------
    bool isSystemHealthy() const;

private:
    const yuki::capability::CapabilityGraph* m_graph;

    // Internal formatting helpers
    std::string formatCapability(const CapabilitySummary& cap) const;
    std::vector<CapabilitySummary> collectCapabilities() const;
    std::vector<CapabilitySummary> collectDomainCapabilities(
        const std::string& domain) const;
    std::string buildPreambleFromList(
        const std::vector<CapabilitySummary>& caps) const;
};

} // namespace policy
} // namespace yuki
