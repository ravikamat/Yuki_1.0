// ============================================================================
// YUKI v1.0 - DistillationExtractor
// Extracts high-quality episodic memory pairs into JSONL distillation corpora.
// ============================================================================
#pragma once

#include <string>
#include <vector>
#include <memory>
#include <fstream>

namespace yuki {
namespace sleep {

struct DistilledEpisode {
    std::string task_type;
    std::string prompt;
    std::string context;
    std::vector<std::string> tool_trace;
    std::string target;
    float quality = 0.0f;
    std::string source;
    float reward = 0.0f;
    bool safety_ok = true;
};

class DistillationExtractor {
public:
    DistillationExtractor() = default;
    ~DistillationExtractor() = default;

    std::vector<DistilledEpisode> extractFromMemory(size_t maxEpisodes = 500) const {
        std::vector<DistilledEpisode> episodes;
        episodes.reserve(maxEpisodes);

        // Standard extraction mock/stub placeholder for sleep cycles
        DistilledEpisode ep;
        ep.task_type = "causal_query";
        ep.prompt = "Why does ice float on water?";
        ep.context = "Ice density is 917 kg/m3; liquid water is 1000 kg/m3.";
        ep.tool_trace = {"PhysicsKnowledgeBase", "CausalGraph"};
        ep.target = "Ice floats because solid ice has a lower density than liquid water due to hydrogen bonding expansion.";
        ep.quality = 0.95f;
        ep.source = "self-verified";
        ep.reward = 1.0f;
        ep.safety_ok = true;

        episodes.push_back(std::move(ep));
        return episodes;
    }

    bool saveToJsonl(const std::string& path, const std::vector<DistilledEpisode>& episodes) const {
        std::ofstream file(path, std::ios::app);
        if (!file.is_open()) return false;

        for (const auto& ep : episodes) {
            file << "{\"task_type\":\"" << ep.task_type
                 << "\",\"prompt\":\"" << ep.prompt
                 << "\",\"target\":\"" << ep.target
                 << "\",\"quality\":" << ep.quality
                 << "}\n";
        }
        return file.good();
    }
};

} // namespace sleep
} // namespace yuki
