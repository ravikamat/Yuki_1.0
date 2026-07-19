#pragma once
#include "KnowledgeRecord.h"
#include <string>
#include <vector>

struct sqlite3;

class LocalKnowledgeBase {
public:
    LocalKnowledgeBase(const std::string& dbPath);
    ~LocalKnowledgeBase();

    bool initialize();
    bool storeFact(const KnowledgeRecord& record);
    std::vector<KnowledgeRecord> queryDomain(const std::string& domain) const;
    std::vector<KnowledgeRecord> queryKey(const std::string& key) const;

private:
    std::string dbPath_;
    sqlite3* db_ = nullptr;
};
