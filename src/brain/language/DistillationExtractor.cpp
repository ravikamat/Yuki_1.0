#include "src/brain/language/DistillationExtractor.h"
#include "src/brain/memory/MemoryFabric.h"
#include <fstream>
#include <sstream>

namespace yuki::brain::language {

DistillationExtractor::DistillationExtractor(yuki::memory::MemoryFabric& memoryFabric)
    : memoryFabric_(memoryFabric) {}

bool DistillationExtractor::shouldDistill(const yuki::brain::learning::LearningEpisode& e) const {
    if (!e.safe) return false;
    if (e.finalOutput.empty()) return false;
    if (e.userInput.empty()) return false;
    if (e.acceptedByOwner) return true;
    if (e.critiqueScore >= 0.82f && e.selfEvalScore >= 0.72f) return true;
    return false;
}

std::string DistillationExtractor::toJsonLine(const yuki::brain::learning::LearningEpisode& e) const {
    auto esc = [](std::string value) {
        std::string out;
        out.reserve(value.size() + 8);
        for (char c : value) {
            switch (c) {
                case '\\': out += "\\\\"; break;
                case '"': out += "\\\""; break;
                case '\n': out += "\\n"; break;
                case '\r': break;
                case '\t': out += "\\t"; break;
                default: out += c; break;
            }
        }
        return out;
    };

    std::ostringstream ss;
    ss << "{";
    ss << "\"episode_id\":\"" << esc(e.episodeId) << "\",";
    ss << "\"session_id\":\"" << esc(e.sessionId) << "\",";
    ss << "\"input\":\"" << esc(e.userInput) << "\",";
    ss << "\"system_prompt\":\"" << esc(e.systemPrompt) << "\",";
    ss << "\"target\":\"" << esc(e.finalOutput) << "\",";
    ss << "\"local_candidate\":\"" << esc(e.localCandidate) << "\",";
    ss << "\"task_type\":\"" << esc(e.taskType) << "\",";
    ss << "\"backend\":\"" << esc(e.backendName) << "\",";
    ss << "\"critique_score\":" << e.critiqueScore << ",";
    ss << "\"self_eval_score\":" << e.selfEvalScore << ",";
    ss << "\"reward\":" << e.reward << ",";
    ss << "\"accepted_by_owner\":" << (e.acceptedByOwner ? "true" : "false") << ",";
    ss << "\"fallback_used\":" << (e.fallbackUsed ? "true" : "false");
    ss << "}";
    return ss.str();
}

std::size_t DistillationExtractor::exportEligibleEpisodes(const std::string& outputPath) {
    const auto episodes = memoryFabric_.loadLearningEpisodes();
    std::ofstream out(outputPath, std::ios::out | std::ios::trunc);
    if (!out.is_open()) {
        return 0;
    }

    std::size_t written = 0;
    for (const auto& e : episodes) {
        if (!shouldDistill(e)) continue;
        out << toJsonLine(e) << '\n';
        ++written;
    }
    return written;
}

} // namespace yuki::brain::language
