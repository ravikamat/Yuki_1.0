#pragma once

#include "src/brain/autonomy/AutonomyTypes.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace yuki::autonomy {

class RequirementGraph {
public:
    void clear();
    void addNode(const RequirementNode& node);
    void addEdge(const std::string& from, const std::string& to);
    std::vector<RequirementNode> topologicalOrder() const;
    bool hasCycles() const;
    const std::unordered_map<std::string, RequirementNode>& nodes() const noexcept;

private:
    std::unordered_map<std::string, RequirementNode> nodes_;
    std::unordered_map<std::string, std::vector<std::string>> edges_;
};

} // namespace yuki::autonomy
