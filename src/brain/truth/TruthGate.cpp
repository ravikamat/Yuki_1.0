// TruthGate.cpp — Claim extraction, verification, and uncertainty marking

#include "TruthGate.h"
#include "brain/memory/CognitiveMemoryFabric.h"
#include "brain/memory/EpisodicStore.h"
#include "brain/memory/HdcSemanticGraph.h"
#include "brain/core/Logger.h"

#include <algorithm>
#include <regex>
#include <sstream>

namespace yuki {
namespace truth {

TruthGate::TruthGate(const TruthGateConfig& cfg)
    : config_(cfg) {
}

bool TruthGate::initialize(yuki::brain::memory::CognitiveMemoryFabric* cmf) {
    if (!cmf) {
        yuki::core::Logger::instance().log(yuki::core::LogLevel::ERROR, "TruthGate",
            "Null CognitiveMemoryFabric provided");
        return false;
    }
    cmf_ = cmf;
    yuki::core::Logger::instance().log(yuki::core::LogLevel::INFO, "TruthGate",
        "Initialized with min_confidence=" + std::to_string(config_.min_verification_confidence));
    return true;
}

std::string TruthGate::process_response(const std::string& raw_response) {
    if (raw_response.empty()) return raw_response;

    // Extract claims
    auto claims = extract_claims(raw_response);
    if (claims.empty()) return raw_response;

    // Limit claims processed
    if (claims.size() > config_.max_claims_per_response) {
        claims.resize(config_.max_claims_per_response);
    }

    // Verify each claim
    std::vector<VerificationResult> results;
    results.reserve(claims.size());

    for (const auto& claim : claims) {
        auto result = verify_claim(claim);
        results.push_back(result);

        // Update stats
        stats_.total_claims++;
        switch (result.status) {
            case VerificationStatus::VERIFIED: stats_.verified++; break;
            case VerificationStatus::PARTIAL: stats_.partial++; break;
            case VerificationStatus::CONTRADICTED: stats_.contradicted++; break;
            case VerificationStatus::UNVERIFIED: stats_.unverified++; break;
            case VerificationStatus::UNVERIFIABLE: stats_.unverifiable++; break;
        }
    }

    // Store results for audit
    {
        std::lock_guard<std::mutex> lock(results_mutex_);
        last_results_ = results;
    }

    // Insert uncertainty markers
    std::string processed = insert_markers(raw_response, results);

    return processed;
}

std::vector<yuki::truth::VerificationResult> TruthGate::get_last_results() const {
    std::lock_guard<std::mutex> lock(results_mutex_);
    return last_results_;
}

TruthGate::Stats TruthGate::get_stats() const {
    return stats_;
}

std::vector<yuki::truth::Claim> TruthGate::extract_claims(const std::string& text) const {
    std::vector<Claim> claims;

    // Simple sentence splitter (replace with proper NLP later)
    std::regex sentence_regex(R"(([^.!?]+[.!?]))");
    std::sregex_iterator it(text.begin(), text.end(), sentence_regex);
    std::sregex_iterator end;

    for (; it != end; ++it) {
        std::string sentence = it->str();
        sentence = std::regex_replace(sentence, std::regex(R"(^\s+|\s+$)"), "");

        if (sentence.length() < 10) continue; // Skip very short fragments

        // Skip questions, commands, greetings
        if (sentence.find('?') != std::string::npos) continue;
        if (sentence.rfind("please", 0) == 0) continue;
        if (sentence.rfind("can you", 0) == 0) continue;
        if (sentence.rfind("tell me", 0) == 0) continue;

        Claim claim;
        claim.text = sentence;
        claim.type = classify_claim(sentence);
        claim.confidence = 0.8f; // Base extractor confidence
        claim.source_span = sentence;

        // Simple subject-predicate-object extraction
        // Pattern: "X is Y" or "X has Y" or "X was Y"
        std::regex pattern(R"((\w+(?:\s+\w+)?)\s+(is|are|was|were|has|have|had)\s+(.+))");
        std::smatch match;
        if (std::regex_search(sentence, match, pattern)) {
            claim.subject = match[1].str();
            claim.predicate = match[2].str();
            claim.object = match[3].str();
        }

        claims.push_back(std::move(claim));
    }

    return claims;
}

yuki::truth::ClaimType TruthGate::classify_claim(const std::string& claim_text) const {
    std::string lower = claim_text;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    // Quantitative: numbers, measurements
    if (std::regex_search(lower, std::regex(R"(\d+(\.\d+)?\s*(%|percent|million|billion|km|miles|kg|bytes|gb|tb))"))) {
        return yuki::truth::ClaimType::QUANTITATIVE;
    }

    // Temporal: dates, years, time references
    if (std::regex_search(lower, std::regex(R"(\b(19|20)\d{2}\b|\b(january|february|march|april|may|june|july|august|september|october|november|december)\b|yesterday|today|tomorrow|last week|next month)"))) {
        return yuki::truth::ClaimType::TEMPORAL;
    }

    // Causal: "causes", "leads to", "results in"
    if (std::regex_search(lower, std::regex(R"(\b(causes?|leads? to|results? in|because of|due to)\b)"))) {
        return yuki::truth::ClaimType::CAUSAL;
    }

    // Procedural: "to X, do Y", "run command", "execute"
    if (std::regex_search(lower, std::regex(R"(\b(to\s+\w+,\s*\w+|run|execute|install|configure|set up)\b)"))) {
        return yuki::truth::ClaimType::PROCEDURAL;
    }

    // Definitional: "X is a Y", "X means Y", "defined as"
    if (std::regex_search(lower, std::regex(R"(\b(is a|is an|means|defined as|refers to)\b)"))) {
        return yuki::truth::ClaimType::DEFINITIONAL;
    }

    // Opinion: "I think", "best", "worst", "should", "recommend"
    if (std::regex_search(lower, std::regex(R"(\b(i think|i believe|best|worst|should|recommend|prefer|in my opinion)\b)"))) {
        return yuki::truth::ClaimType::OPINION;
    }

    return yuki::truth::ClaimType::FACTUAL;
}

yuki::truth::VerificationResult TruthGate::verify_claim(const yuki::truth::Claim& claim) const {
    yuki::truth::VerificationResult result;
    result.claim = claim;

    // Opinion claims are inherently unverifiable
    if (claim.type == yuki::truth::ClaimType::OPINION) {
        result.status = yuki::truth::VerificationStatus::UNVERIFIABLE;
        result.uncertainty_marker = make_uncertainty_marker(result.status, claim);
        result.should_emit = config_.emit_uncertain;
        return result;
    }

    // Query memory tiers
    float best_confidence = 0.0f;
    std::vector<std::string> evidence_ids;

    // Try T1 episodic memory
    if (auto ep_conf = query_episodic(claim)) {
        if (*ep_conf > best_confidence) best_confidence = *ep_conf;
        evidence_ids.push_back("T1:episodic");
    }

    // Try T2 semantic graph
    if (auto sem_conf = query_semantic(claim)) {
        if (*sem_conf > best_confidence) best_confidence = *sem_conf;
        evidence_ids.push_back("T2:semantic");
    }

    result.evidence_confidence = best_confidence;
    result.evidence_ids = evidence_ids;

    // Determine status
    if (best_confidence >= config_.min_verification_confidence) {
        result.status = yuki::truth::VerificationStatus::VERIFIED;
    } else if (best_confidence >= config_.partial_threshold) {
        result.status = yuki::truth::VerificationStatus::PARTIAL;
    } else if (best_confidence > 0.0f) {
        // Low confidence but some evidence - might be contradicted
        result.status = yuki::truth::VerificationStatus::UNVERIFIED;
    } else {
        // No evidence found
        result.status = yuki::truth::VerificationStatus::UNVERIFIED;
    }

    // Generate uncertainty marker
    result.uncertainty_marker = make_uncertainty_marker(result.status, claim);

    // Determine emission
    switch (result.status) {
        case yuki::truth::VerificationStatus::VERIFIED:
        case yuki::truth::VerificationStatus::PARTIAL:
            result.should_emit = true;
            break;
        case yuki::truth::VerificationStatus::CONTRADICTED:
            result.should_emit = true; // Emit with contradiction marker
            break;
        case yuki::truth::VerificationStatus::UNVERIFIED:
            result.should_emit = config_.emit_uncertain;
            if (config_.suppress_unverified) result.should_emit = false;
            break;
        case yuki::truth::VerificationStatus::UNVERIFIABLE:
            result.should_emit = config_.emit_uncertain;
            if (config_.suppress_unverifiable) result.should_emit = false;
            break;
    }

    if (!result.should_emit) {
        // Note: stats_ is mutable in header
        const_cast<Stats&>(stats_).suppressed++;
    }

    return result;
}

std::optional<float> TruthGate::query_episodic(const yuki::truth::Claim& claim) const {
    if (!cmf_ || !cmf_->episodicStore()) return std::nullopt;

    // Build query from claim
    std::string query = claim.subject + " " + claim.predicate + " " + claim.object;
    if (query.empty()) query = claim.text;

    // Get text encoder for embedding
    auto* encoder = cmf_->encoder();
    if (!encoder) return std::nullopt;

    // This is a simplified version - in reality would use the encoder
    // For now, return nullopt to indicate no T1 match
    return std::nullopt;
}

std::optional<float> TruthGate::query_semantic(const yuki::truth::Claim& claim) const {
    if (!cmf_ || !cmf_->hdcSemanticGraph()) return std::nullopt;

    // Query HDC semantic graph for subject-predicate-object
    // This would use the HDC proposition query interface
    // For now, return nullopt
    return std::nullopt;
}

std::string TruthGate::make_uncertainty_marker(yuki::truth::VerificationStatus status, const yuki::truth::Claim& claim) const {
    switch (status) {
        case yuki::truth::VerificationStatus::VERIFIED:
            return ""; // No marker needed
        case yuki::truth::VerificationStatus::PARTIAL:
            return " [partially verified]";
        case yuki::truth::VerificationStatus::CONTRADICTED:
            return " [contradicts my knowledge]";
        case yuki::truth::VerificationStatus::UNVERIFIED:
            return " [I'm not certain about this]";
        case yuki::truth::VerificationStatus::UNVERIFIABLE:
            return " [this is my opinion]";
    }
    return "";
}

std::string TruthGate::insert_markers(const std::string& response,
                                      const std::vector<yuki::truth::VerificationResult>& results) const {
    if (results.empty()) return response;

    std::string modified = response;

    // Sort results by source_span position (descending) to insert from end
    std::vector<yuki::truth::VerificationResult> sorted = results;
    std::sort(sorted.begin(), sorted.end(),
        [&response](const yuki::truth::VerificationResult& a, const yuki::truth::VerificationResult& b) {
            // Find position in original response
            size_t pos_a = response.find(a.claim.source_span);
            size_t pos_b = response.find(b.claim.source_span);
            return pos_a > pos_b;
        });

    for (const auto& result : sorted) {
        if (result.uncertainty_marker.empty()) continue;
        if (!result.should_emit) continue; // Suppressed

        // Find the claim text in modified response and append marker
        size_t pos = modified.find(result.claim.source_span);
        if (pos != std::string::npos) {
            size_t end_pos = pos + result.claim.source_span.length();
            // Insert before punctuation if at end, otherwise after the span
            modified.insert(end_pos, result.uncertainty_marker);
        }
    }

    return modified;
}

} // namespace truth
} // namespace yuki