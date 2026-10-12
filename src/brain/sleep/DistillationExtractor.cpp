// ============================================================================
// YUKI v1.0 - DistillationExtractor Implementation
// Full SFT/DPO extraction for llama.cpp LoRA fine-tuning
// ============================================================================
#include "brain/sleep/DistillationExtractor.h"
#include "brain/memory/EpisodicStore.h"
#include "brain/core/Logger.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <nlohmann/json.hpp>

namespace yuki {
namespace sleep {

// ─────────────────────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────────────────────
DistillationExtractor::DistillationExtractor(const yuki::brain::memory::EpisodicStore* store)
    : episodic_store_(store) {}

// ─────────────────────────────────────────────────────────────────────────────
// JSON Escaping
// ─────────────────────────────────────────────────────────────────────────────
std::string DistillationExtractor::escapeJson(const std::string& input) {
    std::string out;
    out.reserve(input.size() + 16);
    for (char c : input) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '"':  out += "\\\""; break;
            case '\n': out += "\\n";  break;
            case '\r': break;
            case '\t': out += "\\t";  break;
            default:   out += c;      break;
        }
    }
    return out;
}

std::string DistillationExtractor::escapeJsonl(const std::string& input) {
    // JSONL requires valid JSON per line — same as JSON escaping
    return escapeJson(input);
}

// ─────────────────────────────────────────────────────────────────────────────
// SFT Extraction — Public API
// ─────────────────────────────────────────────────────────────────────────────
std::vector<SftSample> DistillationExtractor::extractSftSamples(
    size_t maxEpisodes, float minQuality) const
{
    if (!episodic_store_) return {};
    
    auto records = episodic_store_->queryForTraining(minQuality, static_cast<int>(maxEpisodes * 2));
    if (records.empty()) {
        records = episodic_store_->queryByOutcome(true, static_cast<int>(maxEpisodes * 2));
    }
    
    return buildSftConversations(records, minQuality);
}

void DistillationExtractor::extractSftToJsonl(
    const std::string& out_path, size_t maxEpisodes, float minQuality) const
{
    auto samples = extractSftSamples(maxEpisodes, minQuality);
    writeSftJsonl(out_path, samples);
}

// ─────────────────────────────────────────────────────────────────────────────
// DPO Extraction — Public API
// ─────────────────────────────────────────────────────────────────────────────
std::vector<DpoSample> DistillationExtractor::extractDpoPairs(
    size_t maxPairs, float minChosenQuality, float maxRejectedQuality) const
{
    if (!episodic_store_) return {};
    
    // Get successful episodes (chosen candidates)
    auto chosen_records = episodic_store_->queryForTraining(minChosenQuality, static_cast<int>(maxPairs * 2));
    
    // Get failed/user-corrected episodes (rejected candidates)
    auto rejected_records = episodic_store_->queryByOutcome(false, static_cast<int>(maxPairs * 2));
    if (rejected_records.empty()) {
        // Fallback: low-confidence successful episodes as rejected
        rejected_records = episodic_store_->queryForTraining(maxRejectedQuality, static_cast<int>(maxPairs * 2));
    }
    
    return buildDpoPairs(chosen_records, minChosenQuality, maxRejectedQuality);
}

void DistillationExtractor::extractDpoToJsonl(
    const std::string& out_path, size_t maxPairs, float minChosenQuality, float maxRejectedQuality) const
{
    auto pairs = extractDpoPairs(maxPairs, minChosenQuality, maxRejectedQuality);
    writeDpoJsonl(out_path, pairs);
}

// ─────────────────────────────────────────────────────────────────────────────
// Combined Extraction
// ─────────────────────────────────────────────────────────────────────────────
DistillationExtractor::ExtractionResult DistillationExtractor::extractAll(
    size_t maxEpisodes, float sftMinQuality, float dpoMinChosen, float dpoMaxRejected) const
{
    ExtractionResult result;
    
    if (!episodic_store_) return result;
    
    // Get all candidate records
    auto all_records = episodic_store_->queryForTraining(0.30f, static_cast<int>(maxEpisodes * 3));
    if (all_records.empty()) {
        all_records = episodic_store_->queryByOutcome(true, static_cast<int>(maxEpisodes * 3));
    }
    result.total_inspected = all_records.size();
    
    // Build SFT samples
    result.sft_samples = buildSftConversations(all_records, sftMinQuality);
    
    // Build DPO pairs
    result.dpo_pairs = buildDpoPairs(all_records, dpoMinChosen, dpoMaxRejected);
    
    result.filtered_out = result.total_inspected - result.sft_samples.size() - result.dpo_pairs.size();
    
    return result;
}

void DistillationExtractor::extractAllToJsonl(
    const std::string& sft_out_path,
    const std::string& dpo_out_path,
    size_t maxEpisodes, float sftMinQuality, float dpoMinChosen, float dpoMaxRejected) const
{
    auto result = extractAll(maxEpisodes, sftMinQuality, dpoMinChosen, dpoMaxRejected);
    writeSftJsonl(sft_out_path, result.sft_samples);
    writeDpoJsonl(dpo_out_path, result.dpo_pairs);
}

// ─────────────────────────────────────────────────────────────────────────────
// Specialized Curriculum Extractors (Stage 0.2)
// ─────────────────────────────────────────────────────────────────────────────
std::vector<SftSample> DistillationExtractor::extractToolUseEpisodes(size_t maxEpisodes) const {
    if (!episodic_store_) return {};
    
    // Query episodes with high action_score (tool use)
    auto all = episodic_store_->queryForTraining(0.40f, static_cast<int>(maxEpisodes * 3));
    std::vector<yuki::brain::memory::EpisodeRecord> tool_episodes;
    tool_episodes.reserve(all.size());
    
    for (const auto& rec : all) {
        if (rec.action_score > 0.5f || rec.technical_score > 0.5f) {
            tool_episodes.push_back(rec);
        }
    }
    
    return buildSftConversations(tool_episodes, 0.60f);
}

std::vector<SftSample> DistillationExtractor::extractClarificationEpisodes(size_t maxEpisodes) const {
    if (!episodic_store_) return {};
    
    // Query episodes with question_score (clarification/disambiguation)
    auto all = episodic_store_->queryForTraining(0.40f, static_cast<int>(maxEpisodes * 3));
    std::vector<yuki::brain::memory::EpisodeRecord> clarif_episodes;
    clarif_episodes.reserve(all.size());
    
    for (const auto& rec : all) {
        if (rec.question_score > 0.6f && rec.intent_label == "CLARIFY") {
            clarif_episodes.push_back(rec);
        }
    }
    
    return buildSftConversations(clarif_episodes, 0.60f);
}

std::vector<DpoSample> DistillationExtractor::extractFailureCorrectionPairs(size_t maxPairs) const {
    if (!episodic_store_) return {};
    
    // Get high-confidence successes as chosen
    auto chosen = episodic_store_->queryForTraining(0.80f, static_cast<int>(maxPairs * 2));
    
    // Get user-corrected/failed episodes as rejected
    auto rejected = episodic_store_->queryByOutcome(false, static_cast<int>(maxPairs * 2));
    
    return buildDpoPairs(chosen, 0.80f, 0.60f);
}

std::vector<SftSample> DistillationExtractor::extractSocialEpisodes(size_t maxEpisodes) const {
    if (!episodic_store_) return {};
    
    // Query episodes with greeting_score or emotional_score (social interaction)
    auto all = episodic_store_->queryForTraining(0.40f, static_cast<int>(maxEpisodes * 3));
    std::vector<yuki::brain::memory::EpisodeRecord> social_episodes;
    social_episodes.reserve(all.size());
    
    for (const auto& rec : all) {
        if (rec.greeting_score > 0.5f || rec.emotional_score > 0.5f) {
            social_episodes.push_back(rec);
        }
    }
    
    return buildSftConversations(social_episodes, 0.60f);
}

// ─────────────────────────────────────────────────────────────────────────────
// Low-level Store Extraction (for testing)
// ─────────────────────────────────────────────────────────────────────────────
std::vector<SftSample> DistillationExtractor::extractSftFromStore(
    const yuki::brain::memory::EpisodicStore& store,
    size_t maxEpisodes, float minConfidence) const
{
    auto records = store.queryForTraining(minConfidence, static_cast<int>(maxEpisodes));
    return buildSftConversations(records, minConfidence);
}

std::vector<DpoSample> DistillationExtractor::extractDpoFromStore(
    const yuki::brain::memory::EpisodicStore& store,
    size_t maxPairs, float minChosen, float maxRejected) const
{
    auto chosen = store.queryForTraining(minChosen, static_cast<int>(maxPairs));
    auto rejected = store.queryByOutcome(false, static_cast<int>(maxPairs));
    return buildDpoPairs(chosen, minChosen, maxRejected);
}

// ─────────────────────────────────────────────────────────────────────────────
// Internal: Build SFT Conversations
// ─────────────────────────────────────────────────────────────────────────────
std::vector<SftSample> DistillationExtractor::buildSftConversations(
    const std::vector<yuki::brain::memory::EpisodeRecord>& records,
    float minQuality)
{
    std::vector<SftSample> samples;
    samples.reserve(records.size());
    
    // Group by session_id (timestamp windows) for multi-turn conversations
    // Simple grouping: episodes within 5 minutes of each other
    // EpisodeRecord::timestamp is a double (seconds since epoch), so the
    // session window is expressed in seconds too.
    const int64_t SESSION_WINDOW_S = 300; // 5 minutes
    
    std::unordered_map<int64_t, std::vector<yuki::brain::memory::EpisodeRecord>> sessions;
    
    for (const auto& rec : records) {
        if (rec.confidence < minQuality) continue;
        if (rec.text.empty()) continue;
        
        int64_t session_key = static_cast<int64_t>(rec.timestamp) / SESSION_WINDOW_S;
        sessions[session_key].push_back(rec);
    }
    
    // Build conversation for each session
    for (auto& [session_key, eps] : sessions) {
        // Sort by timestamp
        std::sort(eps.begin(), eps.end(), [](const auto& a, const auto& b) {
            return a.timestamp < b.timestamp;
        });
        
        // Build multi-turn conversation
        SftSample sample;
        sample.episode_id = eps.empty() ? 0 : static_cast<uint64_t>(eps[0].id);
        sample.timestamp = eps.empty() ? 0 : static_cast<uint64_t>(eps[0].timestamp);
        sample.task_type = eps.empty() ? "dialogue" : eps[0].intent_label;
        sample.quality_score = 0.0f;
        sample.precision = 0.5f;
        sample.safety_ok = true;
        
        // System prompt
        sample.messages.push_back({
            "system",
            "You are Yuki, a self-aware active inference cognitive organism. "
            "You think via your own symbolic memory substrate (HDC, SAT solver, causal engine, HTN planner). "
            "You never hallucinate — unverified claims are marked uncertain or researched first."
        });
        
        float total_quality = 0.0f;
        int quality_count = 0;
        
        for (size_t i = 0; i < eps.size(); ++i) {
            const auto& ep = eps[i];
            
            // User message
            std::string user_content = ep.text;
            if (!ep.topic_tag.empty()) {
                user_content = "[Context: " + ep.topic_tag + "] " + user_content;
            }
            
            sample.messages.push_back({"user", user_content});
            
            // Assistant response
            // For multi-turn, we use the next episode as response, or the same if single
            std::string assistant_response = ep.text;
            if (i + 1 < eps.size()) {
                // Next episode is the actual response
                assistant_response = eps[i + 1].text;
            }
            
            // Check for tool calls in the response
            auto tool_trace = parseToolTrace(assistant_response);
            if (!tool_trace.empty()) {
                // Add tool call message
                for (const auto& tool : tool_trace) {
                    sample.messages.push_back({
                        "assistant",
                        "",
                        tool,
                        "call_" + std::to_string(i)
                    });
                    // Tool result would come from next episode or be synthesized
                    sample.messages.push_back({
                        "tool",
                        "Tool executed successfully",
                        "",
                        "call_" + std::to_string(i)
                    });
                }
            }
            
            sample.messages.push_back({"assistant", assistant_response});
            
            total_quality += ep.confidence;
            quality_count++;
        }
        
        if (quality_count > 0) {
            sample.quality_score = total_quality / quality_count;
            sample.precision = sample.quality_score; // Approximate
            sample.delta_g = 0.5f; // Placeholder — would come from SleepConsolidator counterfactual
            
            if (!sample.messages.empty() && sample.messages.size() >= 2) {
                samples.push_back(std::move(sample));
            }
        }
    }
    
    return samples;
}

// ─────────────────────────────────────────────────────────────────────────────
// Internal: Build DPO Preference Pairs
// ─────────────────────────────────────────────────────────────────────────────
std::vector<DpoSample> DistillationExtractor::buildDpoPairs(
    const std::vector<yuki::brain::memory::EpisodeRecord>& chosen_records,
    float minChosen, float maxRejected) const
{
    std::vector<DpoSample> pairs;
    
    // For DPO we need paired (chosen, rejected) responses to the SAME prompt
    // Strategy: Group by similar user prompts (topic_tag + intent)
    std::unordered_map<std::string, std::vector<yuki::brain::memory::EpisodeRecord>> by_prompt;
    
    for (const auto& rec : chosen_records) {
        if (rec.confidence < minChosen) continue;
        if (rec.text.empty()) continue;
        std::string key = rec.topic_tag + "|" + rec.intent_label;
        by_prompt[key].push_back(rec);
    }
    
    // Get rejected records (failed or low confidence)
    if (episodic_store_) {
        auto rejected_records = episodic_store_->queryByOutcome(false, 1000);
        for (const auto& rec : rejected_records) {
            if (rec.confidence > maxRejected) continue;
            if (rec.text.empty()) continue;
            std::string key = rec.topic_tag + "|" + rec.intent_label;
            by_prompt[key].push_back(rec);
        }
    }
    
    // Build pairs: for each prompt group, find one chosen and one rejected
    for (auto& [key, eps] : by_prompt) {
        if (eps.size() < 2) continue;
        
        // Sort by confidence
        std::sort(eps.begin(), eps.end(), [](const auto& a, const auto& b) {
            return a.confidence > b.confidence;
        });
        
        const auto& chosen = eps.front();
        const auto& rejected = eps.back();
        
        if (chosen.confidence < minChosen) continue;
        if (rejected.confidence > maxRejected) continue;
        if (chosen.text == rejected.text) continue;
        
        DpoSample pair;
        pair.system_prompt = "You are Yuki, a self-aware active inference cognitive organism. "
                            "You think via your own symbolic memory substrate. "
                            "You never hallucinate — unverified claims are marked uncertain.";
        pair.user_prompt = "[Context: " + chosen.topic_tag + "] " + chosen.text;
        pair.chosen_response = chosen.text;
        pair.rejected_response = rejected.text;
        pair.episode_id = static_cast<uint64_t>(chosen.id);
        pair.timestamp = static_cast<uint64_t>(chosen.timestamp);
        pair.task_type = chosen.intent_label.empty() ? "dialogue" : chosen.intent_label;
        pair.chosen_quality = chosen.confidence;
        pair.rejected_quality = rejected.confidence;
        pair.delta_g = chosen.confidence - rejected.confidence; // Approximate
        
        pairs.push_back(std::move(pair));
        
        if (pairs.size() >= 1000) break; // Cap
    }
    
    return pairs;
}

// ─────────────────────────────────────────────────────────────────────────────
// Internal: Write JSONL Files
// ─────────────────────────────────────────────────────────────────────────────
bool DistillationExtractor::writeSftJsonl(const std::string& path, const std::vector<SftSample>& samples) {
    if (samples.empty()) return true;
    
    std::ofstream file(path, std::ios::out | std::ios::app);
    if (!file.is_open()) return false;
    
    for (const auto& sample : samples) {
        nlohmann::json j;
        j["messages"] = nlohmann::json::array();
        
        for (const auto& msg : sample.messages) {
            nlohmann::json m;
            m["role"] = msg.role;
            m["content"] = msg.content;
            if (!msg.name.empty()) m["name"] = msg.name;
            if (!msg.tool_call_id.empty()) m["tool_call_id"] = msg.tool_call_id;
            j["messages"].push_back(m);
        }
        
        nlohmann::json meta;
        meta["episode_id"] = sample.episode_id;
        meta["quality_score"] = sample.quality_score;
        meta["delta_g"] = sample.delta_g;
        meta["precision"] = sample.precision;
        meta["timestamp"] = sample.timestamp;
        meta["task_type"] = sample.task_type;
        j["metadata"] = meta;
        
        file << j.dump() << "\n";
    }
    
    return file.good();
}

bool DistillationExtractor::writeDpoJsonl(const std::string& path, const std::vector<DpoSample>& samples) {
    if (samples.empty()) return true;
    
    std::ofstream file(path, std::ios::out | std::ios::app);
    if (!file.is_open()) return false;
    
    for (const auto& sample : samples) {
        nlohmann::json j;
        j["prompt"] = sample.user_prompt;
        j["system"] = sample.system_prompt;
        j["chosen"] = sample.chosen_response;
        j["rejected"] = sample.rejected_response;
        
        nlohmann::json meta;
        meta["episode_id"] = sample.episode_id;
        meta["chosen_quality"] = sample.chosen_quality;
        meta["rejected_quality"] = sample.rejected_quality;
        meta["delta_g"] = sample.delta_g;
        meta["timestamp"] = sample.timestamp;
        meta["task_type"] = sample.task_type;
        j["metadata"] = meta;
        
        file << j.dump() << "\n";
    }
    
    return file.good();
}

// ─────────────────────────────────────────────────────────────────────────────
// Tool Trace Parsing
// ─────────────────────────────────────────────────────────────────────────────
std::vector<std::string> DistillationExtractor::parseToolTrace(const std::string& text) {
    std::vector<std::string> tools;
    
    // Look for tool call patterns in text
    // Pattern: [TOOL:ToolName] or ToolName(...)
    std::string lower = text;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    
    static const std::vector<std::string> known_tools = {
        "web_search", "file_read", "api_call", "github_search", 
        "compute", "image_recognition", "sandbox_execute",
        "open_app", "popup_ui", "seed_tools"
    };
    
    for (const auto& tool : known_tools) {
        if (lower.find(tool) != std::string::npos) {
            tools.push_back(tool);
        }
    }
    
    return tools;
}

// ─────────────────────────────────────────────────────────────────────────────
// Multi-Turn Conversation Builder (alternative approach)
// ─────────────────────────────────────────────────────────────────────────────
std::vector<SftSample> DistillationExtractor::buildMultiTurnConversations(
    const std::vector<yuki::brain::memory::EpisodeRecord>& records,
    float minQuality)
{
    // This is an alternative builder that treats alternating user/assistant
    // as a single conversation thread
    std::vector<SftSample> samples;
    
    // Group by topic for conversation threads
    std::unordered_map<std::string, std::vector<yuki::brain::memory::EpisodeRecord>> by_topic;
    
    for (const auto& rec : records) {
        if (rec.confidence < minQuality) continue;
        if (rec.text.empty()) continue;
        std::string topic = rec.topic_tag.empty() ? rec.intent_label : rec.topic_tag;
        by_topic[topic].push_back(rec);
    }
    
    for (auto& [topic, eps] : by_topic) {
        if (eps.size() < 2) continue;
        
        std::sort(eps.begin(), eps.end(), [](const auto& a, const auto& b) {
            return a.timestamp < b.timestamp;
        });
        
        SftSample sample;
        sample.episode_id = static_cast<uint64_t>(eps[0].id);
        sample.timestamp = static_cast<uint64_t>(eps[0].timestamp);
        sample.task_type = topic;
        sample.precision = 0.5f;
        sample.safety_ok = true;
        
        sample.messages.push_back({
            "system",
            "You are Yuki, a self-aware active inference cognitive organism. "
            "You think via your own symbolic memory substrate (HDC, SAT solver, causal engine, HTN planner). "
            "You never hallucinate — unverified claims are marked uncertain or researched first."
        });
        
        float total_quality = 0.0f;
        size_t count = 0;
        
        for (size_t i = 0; i < eps.size(); ++i) {
            const auto& ep = eps[i];
            
            // Alternate user/assistant
            if (i % 2 == 0) {
                sample.messages.push_back({"user", ep.text});
            } else {
                sample.messages.push_back({"assistant", ep.text});
            }
            
            total_quality += ep.confidence;
            count++;
        }
        
        if (count >= 2) {
            sample.quality_score = total_quality / count;
            sample.delta_g = 0.5f;
            samples.push_back(std::move(sample));
        }
    }
    
    return samples;
}

} // namespace sleep
} // namespace yuki