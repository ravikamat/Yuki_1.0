#pragma once
#include "MeaningTypes.h"
#include <string>
#include <vector>

class EntitySpanDetector {
public:
    EntitySpanDetector();
    
    // Finds potential entity spans in a sentence (e.g. capitalized phrases, quotes)
    std::vector<std::string> detectSpans(const std::string& query);
};

class UserMemory;

class EntityLinker {
public:
    EntityLinker();
    
    // Links detected spans to a semantic EntityType using context and memory
    std::vector<LinkedEntity> linkEntities(const std::vector<std::string>& spans, const std::string& context, const UserMemory* memory);

private:
    EntityType heuristicClassify(const std::string& span, const std::string& context, const UserMemory* memory, std::string& linkSourceOut);
};
