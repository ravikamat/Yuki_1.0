#include "src/brain/language/DistillationCorpusWriter.h"
#include <fstream>

namespace yuki::brain::language {

bool DistillationCorpusWriter::writeEpisode(
    const yuki::brain::learning::LearningEpisode& episode, const std::string& outputPath) {
    std::ofstream out(outputPath, std::ios::out | std::ios::app);
    if (!out.is_open()) return false;

    out << "{\"episode_id\":\"" << episode.episodeId
        << "\",\"input\":\"" << episode.userInput
        << "\",\"target\":\"" << episode.finalOutput << "\"}\n";
    return true;
}

} // namespace yuki::brain::language
