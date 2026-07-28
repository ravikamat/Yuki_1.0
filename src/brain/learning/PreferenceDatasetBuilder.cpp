#include "src/brain/learning/PreferenceDatasetBuilder.h"
#include <fstream>

namespace yuki::brain::learning {

PreferencePair PreferenceDatasetBuilder::buildPair(
    const LearningEpisode& localEpisode, const LearningEpisode& externalEpisode) {
    PreferencePair pair;
    pair.prompt = localEpisode.userInput;
    if (localEpisode.selfEvalScore >= externalEpisode.selfEvalScore) {
        pair.chosen = localEpisode.finalOutput;
        pair.rejected = externalEpisode.finalOutput;
        pair.margin = localEpisode.selfEvalScore - externalEpisode.selfEvalScore;
    } else {
        pair.chosen = externalEpisode.finalOutput;
        pair.rejected = localEpisode.finalOutput;
        pair.margin = externalEpisode.selfEvalScore - localEpisode.selfEvalScore;
    }
    return pair;
}

std::size_t PreferenceDatasetBuilder::exportDpoJsonl(
    const std::vector<PreferencePair>& pairs, const std::string& outputPath) {
    std::ofstream out(outputPath, std::ios::out | std::ios::trunc);
    if (!out.is_open()) return 0;

    std::size_t count = 0;
    for (const auto& pair : pairs) {
        out << "{\"prompt\":\"" << pair.prompt
            << "\",\"chosen\":\"" << pair.chosen
            << "\",\"rejected\":\"" << pair.rejected << "\"}\n";
        ++count;
    }
    return count;
}

} // namespace yuki::brain::learning
