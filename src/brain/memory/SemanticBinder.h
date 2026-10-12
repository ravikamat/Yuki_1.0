// SemanticBinder.h — Digest new concepts into T2 semantic graph
#pragma once
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include "HdcSemanticGraph.h"

namespace yuki {
namespace brain {
namespace memory {

struct ConceptBinding {
    std::string name;
    std::vector<std::string> episodes;
    std::string valence;
    std::vector<std::string> affordances;
    float confidence = 0.0f;
    uint64_t timestamp = 0;
};

class SemanticBinder {
public:
    explicit SemanticBinder(yuki::memory::HdcSemanticGraph* graph = nullptr);
    ~SemanticBinder() = default;

    void setSemanticGraph(yuki::memory::HdcSemanticGraph* graph);

    void bindConcept(const std::string& name,
                     const std::vector<std::string>& episodes,
                     const std::string& valence,
                     const std::vector<std::string>& affordances);

    const ConceptBinding* getBinding(const std::string& name) const;

    std::vector<ConceptBinding> getBindingsByDomain(const std::string& domain) const;

    bool mergeConcepts(const std::string& name_a, const std::string& name_b);

    const std::vector<ConceptBinding>& getAllBindings() const;

private:
    yuki::memory::HdcSemanticGraph* graph_ = nullptr;
    std::vector<ConceptBinding> bindings_;
    mutable std::mutex mutex_;
};

} // namespace memory
} // namespace brain
} // namespace yuki