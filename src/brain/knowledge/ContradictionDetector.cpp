#include "brain/knowledge/ContradictionDetector.h"
#include "brain/core/Logger.h"
#include "brain/memory/HdcSemanticGraph.h"
#include "brain/predictive/LatentWorldModel.h"
#include "brain/causality/CausalWorldModel.h"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <queue>
#include <set>

namespace yuki {
namespace brain {
namespace knowledge {

using yuki::memory::HdcConcept;

ContradictionDetector::ContradictionDetector(const ContradictionConfig& cfg)
    : config_(cfg) {
    yuki::core::Logger::instance().log(yuki::core::LogLevel::INFO, "ContradictionDetector",
        "Initialized Contradiction Detector (vector + logic + latent)");
}

std::vector<Contradiction> ContradictionDetector::detectForFact(
    const std::string& subject,
    const std::string& relation,
    const std::string& object,
    float confidence) {
    std::vector<Contradiction> all_contradictions;
    auto vec_contradictions = detectVectorContradictions(subject, relation, object);
    all_contradictions.insert(all_contradictions.end(), vec_contradictions.begin(), vec_contradictions.end());
    if (config_.enable_logic_check) {
        auto logic_contradictions = detectLogicContradictions(subject, relation, object);
        all_contradictions.insert(all_contradictions.end(), logic_contradictions.begin(), logic_contradictions.end());
    }
    std::vector<Contradiction> filtered;
    for (auto& c : all_contradictions) {
        c.confidence *= confidence;
        if (c.confidence >= 0.5f) {
            c.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            filtered.push_back(c);
            active_contradictions_.push_back(c);
            stats_.total_detected++;
            stats_.by_type[static_cast<int>(c.type)]++;
        }
    }
    return filtered;
}

std::vector<Contradiction> ContradictionDetector::scanAll(float min_severity) {
    if (!hdc_) return {};
    std::vector<Contradiction> all;
    auto concepts = hdc_->getAllConcepts(1000);
    size_t n_concepts = concepts.size();
    
    for (size_t i = 0; i < n_concepts; ++i) {
        const auto& cpt = concepts[i];
        std::string concept_name = cpt.name;
        auto triples = hdc_->getTriplesFor(concept_name, 20);
        size_t n_triples = triples.size();
        for (size_t j = 0; j < n_triples; ++j) {
            const auto& triple = triples[j];
            auto contradictions = detectForFact(triple.subject, triple.relation, triple.object, triple.weight);
            for (auto& c : contradictions) {
                if (c.severity >= min_severity) all.push_back(c);
            }
        }
    }
    return all;
}

std::vector<Contradiction> ContradictionDetector::detectLatentContradictions(
    const std::array<predictive::RSSMState, 5>& latent_states) {
    return detectLatentSpaceContradictions(latent_states);
}

std::vector<Contradiction> ContradictionDetector::detectCausalContradictions() {
    std::vector<Contradiction> contradictions;
    if (!causal_world_model_) return contradictions;
    return contradictions;
}

std::vector<Resolution> ContradictionDetector::resolve(const std::vector<Contradiction>& contradictions) {
    std::vector<Resolution> resolutions;
    for (const auto& c : contradictions) {
        Resolution r;
        r.contradiction = c;
        if (c.severity < 0.3f && c.type == Contradiction::Type::VECTOR_OPPOSITION) {
            r = resolveMergeContext(c);
        } else if (c.type == Contradiction::Type::LOGIC_INCONSISTENCY) {
            r = resolvePreferReliable(c);
        } else if (c.type == Contradiction::Type::TEMPORAL_INCONSISTENCY) {
            r = resolvePreferRecent(c);
        } else {
            r = resolveMergeContext(c);
        }
        resolutions.push_back(r);
    }
    return resolutions;
}

bool ContradictionDetector::applyResolution(const Resolution& resolution) {
    if (!resolution.applied) return false;
    applyToHdc(resolution);
    applyToLatent(resolution);
    stats_.total_resolved++;
    return true;
}

std::vector<Contradiction> ContradictionDetector::detectVectorContradictions(
    const std::string& subject, const std::string& relation, const std::string& object) {
    std::vector<Contradiction> contradictions;
    if (!hdc_) return contradictions;
    std::string opposite_relation = getOppositeRelation(relation);
    if (!opposite_relation.empty()) {
        auto triples = hdc_->getTriplesFor(subject, 20);
        float opposite_conf = 0.0f;
        float original_conf = 0.0f;
        for (const auto& triple : triples) {
            if (triple.relation == relation && triple.object == object) {
                original_conf = triple.weight;
            }
            if (triple.relation == opposite_relation && triple.object == object) {
                opposite_conf = triple.weight;
            }
        }
        if (original_conf > 0.6f && opposite_conf > 0.6f) {
            Contradiction c;
            c.type = Contradiction::Type::VECTOR_OPPOSITION;
            c.entity_a = subject;
            c.relation = relation;
            c.entity_b = object;
            c.statement_a = subject + " " + relation + " " + object;
            c.statement_b = subject + " " + opposite_relation + " " + object;
            c.confidence = std::min(original_conf, opposite_conf);
            c.severity = (original_conf + opposite_conf) / 2.0f;
            c.sources_a.push_back("HDC vector");
            c.sources_b.push_back("HDC vector");
            contradictions.push_back(c);
        }
    }
    float opposition = computeVectorOpposition(
        subject + " " + relation + " " + object,
        subject + " " + opposite_relation + " " + object);
    if (opposition < config_.opposition_threshold) {
        Contradiction c;
        c.type = Contradiction::Type::VECTOR_OPPOSITION;
        c.entity_a = subject;
        c.relation = relation;
        c.entity_b = object;
        c.confidence = -opposition;
        c.severity = -opposition;
        contradictions.push_back(c);
    }
    return contradictions;
}

std::vector<Contradiction> ContradictionDetector::detectLogicContradictions(
    const std::string& subject, const std::string& relation, const std::string& object) {
    std::vector<Contradiction> contradictions;
    if (relation == "has_property") {
        std::string negated = "not_" + object;
        auto triples = hdc_ ? hdc_->getTriplesFor(subject, 20) : std::vector<memory::HdcSemanticGraph::RetrievedTriple>{};
        float orig = 0.0f;
        float neg = 0.0f;
        for (const auto& triple : triples) {
            if (triple.relation == relation && triple.object == object) {
                orig = triple.weight;
            }
            if (triple.relation == relation && triple.object == negated) {
                neg = triple.weight;
            }
        }
        if (orig > 0.7f && neg > 0.7f) {
            Contradiction c;
            c.type = Contradiction::Type::LOGIC_INCONSISTENCY;
            c.entity_a = subject;
            c.relation = relation;
            c.entity_b = object;
            c.statement_a = subject + " has " + object;
            c.statement_b = subject + " has not " + object;
            c.confidence = std::min(orig, neg);
            c.severity = 0.9f;
            contradictions.push_back(c);
        }
    }
    return contradictions;
}

std::vector<Contradiction> ContradictionDetector::detectLatentSpaceContradictions(
    const std::array<predictive::RSSMState, 5>& latent_states) {
    std::vector<Contradiction> contradictions;
    if (!latent_world_model_) return contradictions;
    for (int l = 0; l < 5; ++l) {
        const auto& state = latent_states[l];
        if (!state.deterministic.empty() && !state.stochastic.empty()) {
            float dot = 0.0f;
            size_t min_size = std::min(state.deterministic.size(), state.stochastic.size());
            for (size_t i = 0; i < min_size; ++i) {
                dot += state.deterministic[i] * state.stochastic[i];
            }
            float norm_det = 0.0f, norm_sto = 0.0f;
            for (float v : state.deterministic) norm_det += v * v;
            for (float v : state.stochastic) norm_sto += v * v;
            float cos_sim = dot / (std::sqrt(norm_det) * std::sqrt(norm_sto) + 1e-8f);
            if (cos_sim < config_.latent_contradiction_threshold) {
                Contradiction c;
                c.type = Contradiction::Type::LATENT_OPPOSITION;
                c.entity_a = "level_" + std::to_string(l);
                c.relation = "deterministic_vs_stochastic";
                c.entity_b = "opposition";
                c.confidence = -cos_sim;
                c.severity = -cos_sim;
                contradictions.push_back(c);
            }
        }
    }
    return contradictions;
}

Resolution ContradictionDetector::resolvePreferRecent(const Contradiction& c) {
    Resolution r;
    r.contradiction = c;
    r.strategy = Resolution::Strategy::PREFER_RECENT;
    r.resolved_statement = c.statement_a;
    r.confidence = 0.7f;
    r.applied = true;
    return r;
}

Resolution ContradictionDetector::resolvePreferReliable(const Contradiction& c) {
    Resolution r;
    r.contradiction = c;
    r.strategy = Resolution::Strategy::PREFER_RELIABLE;
    r.resolved_statement = c.statement_a;
    r.confidence = 0.8f;
    r.applied = true;
    return r;
}

Resolution ContradictionDetector::resolveMergeContext(const Contradiction& c) {
    Resolution r;
    r.contradiction = c;
    r.strategy = Resolution::Strategy::MERGE_CONTEXT;
    r.resolved_statement = c.entity_a + " " + c.relation + " " + c.entity_b + " (context-dependent)";
    r.confidence = 0.7f;
    r.applied = true;
    return r;
}

void ContradictionDetector::applyToHdc(const Resolution& r) {
    if (!hdc_) return;
}

void ContradictionDetector::applyToLatent(const Resolution& r) {
    if (latent_world_model_) {
    }
}

std::string ContradictionDetector::getOppositeRelation(const std::string& relation) const {
    static const std::unordered_map<std::string, std::string> opposites = {
        {"is_a", "is_not_a"},
        {"has_property", "lacks_property"},
        {"causes", "prevents"},
        {"implies", "contradicts"},
        {"subclass_of", "not_subclass_of"},
        {"part_of", "not_part_of"},
        {"same_as", "different_from"},
        {"causes", "inhibits"},
        {"enables", "disables"}
    };
    auto it = opposites.find(relation);
    return it != opposites.end() ? it->second : "";
}

float ContradictionDetector::computeVectorOpposition(const std::string& a, const std::string& b) const {
    if (!hdc_) return 0.0f;
    auto vec_a = hdc_->getConceptEmbedding(a);
    auto vec_b = hdc_->getConceptEmbedding(b);
    if (vec_a.empty() || vec_b.empty()) return 0.0f;
    float dot = 0.0f, norm_a = 0.0f, norm_b = 0.0f;
    for (size_t i = 0; i < vec_a.size() && i < vec_b.size(); ++i) {
        dot += vec_a[i] * vec_b[i];
        norm_a += vec_a[i] * vec_a[i];
        norm_b += vec_b[i] * vec_b[i];
    }
    if (norm_a == 0.0f || norm_b == 0.0f) return 0.0f;
    return dot / (std::sqrt(norm_a) * std::sqrt(norm_b));
}

bool ContradictionDetector::checkLogicConsistency(const std::string& a, const std::string& b,
                                                   std::vector<std::string>& chain) const {
    return true;
}

} // namespace knowledge
} // namespace brain
} // namespace yuki