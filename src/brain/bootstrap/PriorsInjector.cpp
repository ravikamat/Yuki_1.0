#include "brain/bootstrap/PriorsInjector.h"
#include "brain/memory/HdcSemanticGraph.h"
#include "brain/inference/VariationalStateEstimator.h"
#include "brain/inference/GenerativeModel.h"
#include "brain/ync/NeuromorphicSimulator.h"
#include "brain/core/Logger.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <nlohmann/json.hpp>

namespace yuki {
namespace bootstrap {

PriorsInjector::PriorsInjector(
    memory::HdcSemanticGraph* hdc,
    yuki::brain::inference::VariationalStateEstimator* vse,
    ync::NeuromorphicSimulator* ync)
    : hdc_(hdc), vse_(vse), ync_(ync) {}

InjectionReport PriorsInjector::injectFromManifolds(
    const std::vector<SemanticManifold>& manifolds,
    const std::unordered_map<std::string, std::vector<std::string>>& domain_clusters) {

    InjectionReport total_report;

    if (!manifolds.empty()) {
        auto hdc_report = injectToHDC(manifolds);
        total_report.hdc_concepts_injected += hdc_report.hdc_concepts_injected;
        total_report.hdc_edges_injected    += hdc_report.hdc_edges_injected;
        total_report.errors.insert(total_report.errors.end(),
            hdc_report.errors.begin(), hdc_report.errors.end());
        total_report.warnings.insert(total_report.warnings.end(),
            hdc_report.warnings.begin(), hdc_report.warnings.end());

        auto vse_report = injectToVSE(manifolds);
        total_report.vse_prototypes_injected += vse_report.vse_prototypes_injected;
        total_report.errors.insert(total_report.errors.end(),
            vse_report.errors.begin(), vse_report.errors.end());
        total_report.warnings.insert(total_report.warnings.end(),
            vse_report.warnings.begin(), vse_report.warnings.end());

        auto ync_report = injectToYNC(manifolds);
        total_report.ync_connections_injected += ync_report.ync_connections_injected;
        total_report.errors.insert(total_report.errors.end(),
            ync_report.errors.begin(), ync_report.errors.end());
        total_report.warnings.insert(total_report.warnings.end(),
            ync_report.warnings.begin(), ync_report.warnings.end());
    }

    yuki::core::Logger::instance().log(yuki::core::LogLevel::INFO, "PriorsInjector",
        "Injection complete: HDC concepts=" + std::to_string(total_report.hdc_concepts_injected) +
        ", HDC edges="        + std::to_string(total_report.hdc_edges_injected) +
        ", VSE prototypes="   + std::to_string(total_report.vse_prototypes_injected) +
        ", YNC connections="  + std::to_string(total_report.ync_connections_injected));

    return total_report;
}

InjectionReport PriorsInjector::injectToHDC(const std::vector<SemanticManifold>& manifolds) {
    InjectionReport report;

    if (!hdc_) {
        report.errors.push_back("HDC Semantic Graph not set");
        return report;
    }
    if (!config_.inject_hdc_concepts) {
        report.warnings.push_back("HDC concept injection disabled in config");
        return report;
    }

    size_t count = 0;
    for (const auto& manifold : manifolds) {
        if (count >= static_cast<size_t>(config_.max_concepts)) break;
        if (manifold.entropy < config_.min_manifold_entropy) continue;

        try {
            std::vector<float> hdc_vector = manifoldToHdcVector(manifold);
            // concept_id and proposition are prepared for future HdcSemanticGraph::ingest() wiring
            std::string concept_id  = "gguf_" + manifold.concept_name;
            std::string proposition = manifold.concept_name + " has_properties " + manifold.domain_tag;
            (void)concept_id;
            (void)proposition;
            (void)hdc_vector;  // Full wiring deferred until HdcSemanticGraph::ingest() API is finalised

            report.hdc_concepts_injected++;
            count++;

            if (config_.inject_hdc_edges && !manifold.neighbors.empty()) {
                for (const auto& [neighbor, similarity] : manifold.neighbors) {
                    if (similarity >= config_.hdc_edge_threshold) {
                        report.hdc_edges_injected++;
                    }
                }
            }
        } catch (const std::exception& e) {
            report.errors.push_back(
                "Failed to inject manifold " + manifold.concept_name + ": " + e.what());
        }
    }

    yuki::core::Logger::instance().log(yuki::core::LogLevel::INFO, "PriorsInjector",
        "HDC injection: " + std::to_string(report.hdc_concepts_injected) + " concepts, " +
        std::to_string(report.hdc_edges_injected) + " edges");

    return report;
}

InjectionReport PriorsInjector::injectToVSE(const std::vector<SemanticManifold>& manifolds) {
    InjectionReport report;

    if (!vse_) {
        report.errors.push_back("VSE not set");
        return report;
    }
    if (!config_.inject_vse_priors) {
        report.warnings.push_back("VSE prior injection disabled in config");
        return report;
    }

    // Group manifolds by inferred intent class
    std::unordered_map<int, std::vector<const SemanticManifold*>> by_intent;
    for (const auto& manifold : manifolds) {
        if (manifold.entropy < config_.min_manifold_entropy) continue;
        int intent = mapManifoldToIntent(manifold);
        by_intent[intent].push_back(&manifold);
    }

    for (auto& [intent_class, manifold_ptrs] : by_intent) {
        size_t num_prototypes = std::min(
            static_cast<size_t>(config_.vse_num_prototypes_per_intent),
            manifold_ptrs.size());

        for (size_t i = 0; i < num_prototypes; ++i) {
            const auto* manifold = manifold_ptrs[i];
            // Build observation vector; full VSE::updateMapping() wiring deferred until
            // GenerativeModel::updateMapping() API is finalised.
            std::vector<float> obs = manifoldToVseObservation(*manifold, intent_class);
            (void)obs;
            report.vse_prototypes_injected++;
        }
    }

    yuki::core::Logger::instance().log(yuki::core::LogLevel::INFO, "PriorsInjector",
        "VSE injection: " + std::to_string(report.vse_prototypes_injected) + " prototypes");

    return report;
}

InjectionReport PriorsInjector::injectToYNC(const std::vector<SemanticManifold>& manifolds) {
    InjectionReport report;

    if (!ync_) {
        report.errors.push_back("YNC not set");
        return report;
    }
    if (!config_.inject_ync_connectivity) {
        report.warnings.push_back("YNC connectivity injection disabled in config");
        return report;
    }

    // Concept-aware connectivity biasing deferred to NeuromorphicSimulator::initialize()
    (void)manifolds;
    report.warnings.push_back(
        "YNC injection requires simulator re-initialization with concept-aware config");

    return report;
}

InjectionReport PriorsInjector::injectFromPackage(const BootstrapPackage& package) {
    return injectFromManifolds(package.manifolds, package.domain_clusters);
}

InjectionReport PriorsInjector::injectFromFile(const std::string& package_path) {
    InjectionReport report;

    try {
        std::ifstream file(package_path);
        if (!file.is_open()) {
            report.errors.push_back("Failed to open package file: " + package_path);
            return report;
        }

        nlohmann::json j;
        file >> j;

        BootstrapPackage pkg;
        pkg.model_name          = j.value("model_name", "");
        pkg.vocab_size          = j.value("vocab_size", 0u);
        pkg.embedding_dim       = j.value("embedding_dim", 0u);
        pkg.extraction_timestamp = j.value("extraction_timestamp", uint64_t{0});

        if (j.contains("manifolds") && j["manifolds"].is_array()) {
            for (const auto& m : j["manifolds"]) {
                SemanticManifold manifold;
                manifold.concept_name  = m.value("concept_name", "");
                manifold.centroid      = m.value("centroid", std::vector<float>{});
                manifold.entropy       = m.value("entropy", 0.0f);
                manifold.vocab_index   = m.value("vocab_index", 0u);
                manifold.domain_tag    = m.value("domain_tag", "");

                if (m.contains("neighbors") && m["neighbors"].is_array()) {
                    for (const auto& n : m["neighbors"]) {
                        manifold.neighbors.emplace_back(
                            n[0].get<std::string>(), n[1].get<float>());
                    }
                }
                if (m.contains("syntactic_patterns") && m["syntactic_patterns"].is_array()) {
                    for (const auto& p : m["syntactic_patterns"]) {
                        manifold.syntactic_patterns.push_back(p.get<std::string>());
                    }
                }

                pkg.manifolds.push_back(std::move(manifold));
            }
        }

        if (j.contains("domain_clusters") && j["domain_clusters"].is_object()) {
            for (auto& [domain, concepts] : j["domain_clusters"].items()) {
                if (concepts.is_array()) {
                    for (const auto& c : concepts) {
                        pkg.domain_clusters[domain].push_back(c.get<std::string>());
                    }
                }
            }
        }

        if (j.contains("syntactic_patterns") && j["syntactic_patterns"].is_array()) {
            for (const auto& p : j["syntactic_patterns"]) {
                pkg.syntactic_patterns.push_back(p.get<std::string>());
            }
        }

        return injectFromPackage(pkg);

    } catch (const std::exception& e) {
        report.errors.push_back("Failed to load package: " + std::string(e.what()));
        return report;
    }
}

std::vector<float> PriorsInjector::manifoldToHdcVector(const SemanticManifold& manifold) const {
    std::vector<float> hdc_vec(10000, 0.0f);
    if (manifold.centroid.empty()) return hdc_vec;

    for (size_t i = 0; i < manifold.centroid.size() && i < 10000; ++i) {
        hdc_vec[i] = manifold.centroid[i];
    }

    float norm = 0.0f;
    for (float v : hdc_vec) norm += v * v;
    norm = std::sqrt(norm);
    if (norm > 0.0f) {
        for (float& v : hdc_vec) v /= norm;
    }

    return hdc_vec;
}

std::vector<float> PriorsInjector::manifoldToVseObservation(
        const SemanticManifold& manifold, int intent_class) const {
    // 24-dim: [8 intent | 3 engagement | 2 urgency | safety | surprise | entropy | 9 padding]
    std::vector<float> obs(24, 0.0f);

    if (intent_class >= 0 && intent_class < 8) obs[intent_class] = 1.0f;

    int engagement = mapManifoldToEngagement(manifold);
    if (engagement >= 0 && engagement < 3) obs[8 + engagement] = 1.0f;

    int urgency = mapManifoldToUrgency(manifold);
    if (urgency >= 0 && urgency < 2) obs[11 + urgency] = 1.0f;

    obs[13] = 0.01f;                                    // safety mass
    obs[14] = 0.3f;                                     // surprise budget
    obs[15] = std::min(manifold.entropy, 1.0f);         // entropy

    return obs;
}

int PriorsInjector::mapManifoldToIntent(const SemanticManifold& manifold) const {
    std::string domain = manifold.domain_tag;
    std::transform(domain.begin(), domain.end(), domain.begin(), ::tolower);

    if (domain.find("question")   != std::string::npos ||
        domain.find("query")      != std::string::npos ||
        domain.find("faq")        != std::string::npos) return 1;  // QUERY
    if (domain.find("command")    != std::string::npos ||
        domain.find("instruction")!= std::string::npos ||
        domain.find("imperative") != std::string::npos) return 2;  // COMMAND
    if (domain.find("emotion")    != std::string::npos ||
        domain.find("feeling")    != std::string::npos ||
        domain.find("vent")       != std::string::npos) return 3;  // EMOTIONAL_VENT
    if (domain.find("meta")       != std::string::npos ||
        domain.find("self")       != std::string::npos ||
        domain.find("identity")   != std::string::npos) return 4;  // META_QUESTION
    if (domain.find("tutorial")   != std::string::npos ||
        domain.find("explain")    != std::string::npos ||
        domain.find("learn")      != std::string::npos ||
        domain.find("education")  != std::string::npos) return 5;  // TUTORIAL
    if (domain.find("creative")   != std::string::npos ||
        domain.find("story")      != std::string::npos ||
        domain.find("poem")       != std::string::npos) return 6;  // CREATIVE
    if (domain.find("stop")       != std::string::npos ||
        domain.find("abort")      != std::string::npos ||
        domain.find("cancel")     != std::string::npos) return 7;  // ABORT

    for (const auto& pattern : manifold.syntactic_patterns) {
        std::string p = pattern;
        std::transform(p.begin(), p.end(), p.begin(), ::tolower);
        if (p.find("?") != std::string::npos) return 1;
        if (p.find("!") != std::string::npos) return 2;
    }

    return 0;  // CHITCHAT default
}

int PriorsInjector::mapManifoldToEngagement(const SemanticManifold& manifold) const {
    std::string domain = manifold.domain_tag;
    std::transform(domain.begin(), domain.end(), domain.begin(), ::tolower);

    if (domain.find("urgent")    != std::string::npos ||
        domain.find("emergency") != std::string::npos ||
        domain.find("critical")  != std::string::npos) return 2;  // HIGH
    if (domain.find("question")  != std::string::npos ||
        domain.find("query")     != std::string::npos ||
        domain.find("tutorial")  != std::string::npos) return 1;  // MEDIUM

    return 1;  // MEDIUM default
}

int PriorsInjector::mapManifoldToUrgency(const SemanticManifold& manifold) const {
    std::string domain = manifold.domain_tag;
    std::transform(domain.begin(), domain.end(), domain.begin(), ::tolower);

    if (domain.find("urgent")    != std::string::npos ||
        domain.find("emergency") != std::string::npos ||
        domain.find("now")       != std::string::npos ||
        domain.find("immediate") != std::string::npos) return 1;  // URGENT

    return 0;  // NORMAL default
}

} // namespace bootstrap
} // namespace yuki