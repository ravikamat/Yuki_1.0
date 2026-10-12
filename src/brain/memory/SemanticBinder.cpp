// SemanticBinder.cpp — Digest new concepts into T2 semantic graph

#include "SemanticBinder.h"
#include "HdcSemanticGraph.h"
#include "brain/core/Logger.h"

#include <algorithm>
#include <chrono>
#include <mutex>

namespace yuki {
namespace brain {
namespace memory {

using yuki::memory::HdcSemanticGraph;

SemanticBinder::SemanticBinder(HdcSemanticGraph* graph)
    : graph_(graph) {
}

void SemanticBinder::setSemanticGraph(HdcSemanticGraph* graph) {
    std::lock_guard<std::mutex> lock(mutex_);
    graph_ = graph;
}

void SemanticBinder::bindConcept(const std::string& name,
                                 const std::vector<std::string>& episodes,
                                 const std::string& valence,
                                 const std::vector<std::string>& affordances) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    ConceptBinding binding;
    binding.name = name;
    binding.episodes = episodes;
    binding.valence = valence;
    binding.affordances = affordances;
    binding.confidence = 0.8f; // Default confidence
    binding.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    // Add to HDC semantic graph if available
    if (graph_) {
        // Create concept in graph
        graph_->ingestProposition(name, "has_episodes", 
            "[" + std::to_string(episodes.size()) + "]", 0.9f);
        
        for (const auto& episode : episodes) {
            graph_->ingestProposition(name, "related_to", episode, 0.7f);
        }
        
        for (const auto& affordance : affordances) {
            graph_->ingestProposition(name, "affords", affordance, 0.8f);
        }
        
        graph_->ingestProposition(name, "valence", valence, 0.9f);
    }
    
    // Store binding
    bindings_.push_back(std::move(binding));
    
    yuki::core::Logger::instance().log(yuki::core::LogLevel::INFO, "SemanticBinder",
        "Bound concept: " + name + " (" + std::to_string(episodes.size()) + " episodes)");
}

const ConceptBinding* SemanticBinder::getBinding(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& b : bindings_) {
        if (b.name == name) return &b;
    }
    return nullptr;
}

std::vector<ConceptBinding> SemanticBinder::getBindingsByDomain(const std::string& domain) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<ConceptBinding> result;
    for (const auto& b : bindings_) {
        if (b.name.find(domain) != std::string::npos) {
            result.push_back(b);
        }
    }
    return result;
}

bool SemanticBinder::mergeConcepts(const std::string& name_a, const std::string& name_b) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it_a = std::find_if(bindings_.begin(), bindings_.end(),
        [&](const ConceptBinding& b) { return b.name == name_a; });
    auto it_b = std::find_if(bindings_.begin(), bindings_.end(),
        [&](const ConceptBinding& b) { return b.name == name_b; });
    
    if (it_a == bindings_.end() || it_b == bindings_.end()) return false;
    
    // Merge episodes and affordances
    ConceptBinding merged;
    merged.name = name_a + "+" + name_b;
    merged.episodes = it_a->episodes;
    merged.episodes.insert(merged.episodes.end(), it_b->episodes.begin(), it_b->episodes.end());
    merged.valence = it_a->valence; // Use first concept's valence
    merged.affordances = it_a->affordances;
    merged.affordances.insert(merged.affordances.end(), 
                             it_b->affordances.begin(), it_b->affordances.end());
    merged.confidence = std::min(it_a->confidence, it_b->confidence);
    merged.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    // Remove old bindings
    bindings_.erase(it_a);
    bindings_.erase(it_b);
    
    // Add merged
    bindings_.push_back(std::move(merged));
    
    return true;
}

const std::vector<ConceptBinding>& SemanticBinder::getAllBindings() const {
    return bindings_;
}

} // namespace memory
} // namespace brain
} // namespace yuki