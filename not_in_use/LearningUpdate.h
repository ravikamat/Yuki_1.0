#pragma once
#include "MeaningTypes.h"
#include "UncertaintyDetector.h"

class LearningUpdate {
public:
    LearningUpdate(UncertaintyDetector& ud);
    
    // Updates internal dictionaries and weights after a successful resolution
    void finalizeAndLearn(const MeaningState& state, bool success);

    // Autonomously researches an unknown word on the web and learns it if possible
    bool autoResearch(const std::string& unknownWord);

private:
    std::string extractDefinition(const std::string& word, const std::string& text);
    
    UncertaintyDetector& uncertaintyDetectorRef;
};
