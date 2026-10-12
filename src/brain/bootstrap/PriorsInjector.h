#pragma once
// PriorsInjector — Injects GGUF-extracted manifolds as priors
// Targets: HDC Semantic Graph, VSE Generative Model, YNC Network

#include <string>
#include <vector>
#include <unordered_map>
#include "brain/bootstrap/GgufManifoldExtractor.h"

namespace yuki {
namespace memory { class HdcSemanticGraph; }  // real: yuki::memory
namespace brain::inference { class VariationalStateEstimator; }
namespace ync { class NeuromorphicSimulator; }
namespace bootstrap {

struct PriorsInjectorConfig {
    // HDC injection
    bool inject_hdc_concepts = true;
    bool inject_hdc_edges = true;
    float hdc_edge_threshold = 0.3f;  // Cosine similarity threshold for edges
    int hdc_max_edges_per_concept = 10;
    
    // VSE injection
    bool inject_vse_priors = true;
    int vse_num_prototypes_per_intent = 5;
    float vse_prototype_weight = 0.1f;  // Weight for EMA updates
    
    // YNC injection
    bool inject_ync_connectivity = true;
    float ync_connectivity_density = 0.05f;
    int ync_sensory_neurons = 128;
    int ync_motor_neurons = 16;
    
    // General
    int max_concepts = 50000;
    float min_manifold_entropy = 0.1f;
};

struct InjectionReport {
    size_t hdc_concepts_injected = 0;
    size_t hdc_edges_injected = 0;
    size_t vse_prototypes_injected = 0;
    size_t ync_connections_injected = 0;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
};

class PriorsInjector {
public:
    PriorsInjector(
        memory::HdcSemanticGraph* hdc = nullptr,
        yuki::brain::inference::VariationalStateEstimator* vse = nullptr,
        ync::NeuromorphicSimulator* ync = nullptr);
    ~PriorsInjector() = default;
    
    void setHdcSemanticGraph(memory::HdcSemanticGraph* hdc) noexcept { hdc_ = hdc; }
    void setVSE(yuki::brain::inference::VariationalStateEstimator* vse) noexcept { vse_ = vse; }
    void setYNC(ync::NeuromorphicSimulator* ync) noexcept { ync_ = ync; }
    void setConfig(const PriorsInjectorConfig& cfg) { config_ = cfg; }
    
    // Main injection pipeline
    InjectionReport injectFromManifolds(const std::vector<SemanticManifold>& manifolds,
                                         const std::unordered_map<std::string, std::vector<std::string>>& domain_clusters);
    
    // Individual injections
    InjectionReport injectToHDC(const std::vector<SemanticManifold>& manifolds);
    InjectionReport injectToVSE(const std::vector<SemanticManifold>& manifolds);
    InjectionReport injectToYNC(const std::vector<SemanticManifold>& manifolds);
    
    // Load from saved bootstrap package
    InjectionReport injectFromPackage(const BootstrapPackage& package);
    InjectionReport injectFromFile(const std::string& package_path);

private:
    memory::HdcSemanticGraph* hdc_ = nullptr;
    yuki::brain::inference::VariationalStateEstimator* vse_ = nullptr;
    ync::NeuromorphicSimulator* ync_ = nullptr;
    PriorsInjectorConfig config_;
    
    // Helpers
    std::vector<float> manifoldToHdcVector(const SemanticManifold& manifold) const;
    std::vector<float> manifoldToVseObservation(const SemanticManifold& manifold, int intent_class) const;
    int mapManifoldToIntent(const SemanticManifold& manifold) const;
    int mapManifoldToEngagement(const SemanticManifold& manifold) const;
    int mapManifoldToUrgency(const SemanticManifold& manifold) const;
};

} // namespace bootstrap
} // namespace yuki