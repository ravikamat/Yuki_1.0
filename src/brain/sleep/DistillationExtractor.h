// ============================================================================
// YUKI v1.0 - DistillationExtractor
// Extracts high-quality episodic memory pairs into JSONL distillation corpora
// for SFT (Supervised Fine-Tuning) and DPO (Direct Preference Optimization).
// Produces llama.cpp compatible training data.
// ============================================================================
#pragma once

#include <string>
#include <vector>
#include <memory>
#include <fstream>
#include <cstdint>
#include <optional>
#include "brain/memory/EpisodicStore.h"


namespace yuki {
namespace sleep {

// ─────────────────────────────────────────────────────────────────────────────
// SFT (Supervised Fine-Tuning) Sample — ShareGPT / OpenAI messages format
// ─────────────────────────────────────────────────────────────────────────────
struct SftSample {
    // Conversation in OpenAI/ShareGPT messages format
    struct Message {
        std::string role;      // "system" | "user" | "assistant" | "tool"
        std::string content;
        std::string name;      // tool name for tool calls
        std::string tool_call_id; // for tool responses
    };
    
    std::vector<Message> messages;
    
    // Metadata (not part of training input, for filtering/analysis)
    uint64_t episode_id = 0;
    uint64_t timestamp = 0;
    std::string task_type;      // "dialogue", "tool_use", "clarification", "failure_correction", "social"
    float quality_score = 0.0f; // [0.0, 1.0]
    float delta_g = 0.0f;       // Counterfactual free energy improvement
    float precision = 0.5f;     // VSE precision confidence
    bool safety_ok = true;
};

// ─────────────────────────────────────────────────────────────────────────────
// DPO (Direct Preference Optimization) Preference Pair
// ─────────────────────────────────────────────────────────────────────────────
struct DpoSample {
    std::string system_prompt;
    std::string user_prompt;        // The context/question
    std::string chosen_response;    // High-quality response (confidence >= 0.8)
    std::string rejected_response;  // Lower-quality response (confidence < 0.6 or user-corrected)
    
    uint64_t episode_id = 0;
    uint64_t timestamp = 0;
    std::string task_type;
    float chosen_quality = 0.0f;
    float rejected_quality = 0.0f;
    float delta_g = 0.0f;
};

// ─────────────────────────────────────────────────────────────────────────────
// DistillationExtractor — Main extraction engine
// ─────────────────────────────────────────────────────────────────────────────
class DistillationExtractor {
public:
    explicit DistillationExtractor(const yuki::brain::memory::EpisodicStore* store = nullptr);
    ~DistillationExtractor() = default;

    void setEpisodicStore(const yuki::brain::memory::EpisodicStore* store) noexcept {
        episodic_store_ = store;
    }

    // ─────────────────────────────────────────────────────────────────────────
    // SFT Extraction — produces conversation format for llama.cpp fine-tuning
    // ─────────────────────────────────────────────────────────────────────────
    
    /// Extract high-quality episodes as SFT conversation samples
    std::vector<SftSample> extractSftSamples(
        size_t maxEpisodes = 5000,
        float minQuality = 0.70f) const;

    /// Extract and write SFT JSONL directly to file
    void extractSftToJsonl(
        const std::string& out_path,
        size_t maxEpisodes = 5000,
        float minQuality = 0.70f) const;

    // ─────────────────────────────────────────────────────────────────────────
    // DPO Extraction — produces preference pairs for preference optimization
    // ─────────────────────────────────────────────────────────────────────────
    
    /// Extract preference pairs from failure-correction episodes
    std::vector<DpoSample> extractDpoPairs(
        size_t maxPairs = 1000,
        float minChosenQuality = 0.80f,
        float maxRejectedQuality = 0.60f) const;

    /// Extract and write DPO JSONL directly to file
    void extractDpoToJsonl(
        const std::string& out_path,
        size_t maxPairs = 1000,
        float minChosenQuality = 0.80f,
        float maxRejectedQuality = 0.60f) const;

    // ─────────────────────────────────────────────────────────────────────────
    // Combined Extraction — both SFT and DPO in single pass
    // ─────────────────────────────────────────────────────────────────────────
    
    struct ExtractionResult {
        std::vector<SftSample> sft_samples;
        std::vector<DpoSample> dpo_pairs;
        size_t total_inspected = 0;
        size_t filtered_out = 0;
    };
    
    /// Full extraction: SFT + DPO from episodic memory
    ExtractionResult extractAll(
        size_t maxEpisodes = 5000,
        float sftMinQuality = 0.70f,
        float dpoMinChosen = 0.80f,
        float dpoMaxRejected = 0.60f) const;

    /// Write both SFT and DPO JSONL files
    void extractAllToJsonl(
        const std::string& sft_out_path,
        const std::string& dpo_out_path,
        size_t maxEpisodes = 5000,
        float sftMinQuality = 0.70f,
        float dpoMinChosen = 0.80f,
        float dpoMaxRejected = 0.60f) const;

    // ─────────────────────────────────────────────────────────────────────────
    // Specialized Extractors for Curriculum Seeds (Stage 0.2)
    // ─────────────────────────────────────────────────────────────────────────
    
    /// Extract tool-use episodes with full tool call traces
    std::vector<SftSample> extractToolUseEpisodes(size_t maxEpisodes = 1000) const;
    
    /// Extract clarification dialogue episodes (coreference/reference resolution)
    std::vector<SftSample> extractClarificationEpisodes(size_t maxEpisodes = 500) const;
    
    /// Extract failure-correction episode pairs (error-driven learning)
    std::vector<DpoSample> extractFailureCorrectionPairs(size_t maxPairs = 500) const;
    
    /// Extract social episode seeds (drive calibration)
    std::vector<SftSample> extractSocialEpisodes(size_t maxEpisodes = 500) const;

    // ─────────────────────────────────────────────────────────────────────────
    // Low-level: direct store extraction (for testing without store wiring)
    // ─────────────────────────────────────────────────────────────────────────
    
    std::vector<SftSample> extractSftFromStore(
        const yuki::brain::memory::EpisodicStore& store,
        size_t maxEpisodes = 500,
        float minConfidence = 0.60f) const;

    std::vector<DpoSample> extractDpoFromStore(
        const yuki::brain::memory::EpisodicStore& store,
        size_t maxPairs = 500,
        float minChosen = 0.80f,
        float maxRejected = 0.60f) const;

private:
    const yuki::brain::memory::EpisodicStore* episodic_store_{nullptr};

    // ─────────────────────────────────────────────────────────────────────────
    // Internal helpers
    // ─────────────────────────────────────────────────────────────────────────
    
    static std::string escapeJson(const std::string& input);
    static std::string escapeJsonl(const std::string& input);
    
    // Build SFT conversation from episodic records
    static std::vector<SftSample> buildSftConversations(
        const std::vector<yuki::brain::memory::EpisodeRecord>& records,
        float minQuality);
    
    // Build DPO preference pairs from episodic records
    std::vector<DpoSample> buildDpoPairs(
        const std::vector<yuki::brain::memory::EpisodeRecord>& records,
        float minChosen,
        float maxRejected) const;
    
    // Write SFT samples to JSONL
    static bool writeSftJsonl(const std::string& path, const std::vector<SftSample>& samples);
    
    // Write DPO samples to JSONL
    static bool writeDpoJsonl(const std::string& path, const std::vector<DpoSample>& samples);
    
    // Detect tool calls in episode text
    static std::vector<std::string> parseToolTrace(const std::string& text);
    
    // Build multi-turn conversation from sequential episodes
    static std::vector<SftSample> buildMultiTurnConversations(
        const std::vector<yuki::brain::memory::EpisodeRecord>& records,
        float minQuality);
};

} // namespace sleep
} // namespace yuki

