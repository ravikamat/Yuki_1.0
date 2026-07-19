#pragma once
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <cstdint>
#include <memory>

namespace hnswlib {
    template<typename dist_t> class HierarchicalNSW;
    class InnerProductSpace;
}

struct VectorSearchResult {
    uint64_t id;
    std::string metadata;
    float distance; // Lower is better
};

class VectorStore {
public:
    VectorStore();
    ~VectorStore();

    bool init(int dim, int maxElements = 100000);
    
    void addDocument(uint64_t id, const std::vector<float>& embedding, const std::string& metadata);
    std::vector<VectorSearchResult> search(const std::vector<float>& query, int k = 5);
    
    void addDocumentsBatch(const std::vector<std::pair<uint64_t, std::vector<float>>>& docs);
    bool save(const std::string& path);
    bool load(const std::string& path, int dim, int maxElements = 100000);

private:
    std::unique_ptr<hnswlib::InnerProductSpace> space_;
    std::unique_ptr<hnswlib::HierarchicalNSW<float>> index_;
    int dim_ = 0;
    int maxElements_ = 0;
    std::map<uint64_t, std::string> metadataMap_;
    mutable std::shared_mutex rwMutex_;
    bool loaded_ = false;
};
