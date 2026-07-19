#pragma once
#include <string>
#include <vector>

struct KnowledgeRecord {
    std::string id;
    std::string domain;
    std::string key;
    std::string value;
    std::string source;
    float confidence = 1.0f;
    int64_t timestamp = 0;
};
