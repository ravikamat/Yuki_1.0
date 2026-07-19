#include "brain/retrieval/VectorStore.h"
#include <hnswlib/hnswlib.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cmath>
#include <algorithm>

VectorStore::VectorStore() {}
VectorStore::~VectorStore() {}

bool VectorStore::init(int dim, int maxElements) {
    std::unique_lock<std::shared_mutex> lock(rwMutex_);
    if (loaded_ || index_) return true;
    
    auto newSpace = std::make_unique<hnswlib::InnerProductSpace>(dim);
    auto newIndex = std::make_unique<hnswlib::HierarchicalNSW<float>>(newSpace.get(), maxElements, 16, 200);
    
    space_ = std::move(newSpace);
    index_ = std::move(newIndex);
    dim_ = dim;
    maxElements_ = maxElements;
    loaded_ = true;
    return true;
}

void VectorStore::addDocument(uint64_t id, const std::vector<float>& embedding, const std::string& metadata) {
    std::unique_lock<std::shared_mutex> lock(rwMutex_);
    if (!loaded_ || !index_ || embedding.size() != dim_) return;
    
    std::vector<float> normalized = embedding;
    float norm = 0.0f;
    for (float v : normalized) norm += v * v;
    if (norm > 0) {
        norm = std::sqrt(norm);
        for (float& v : normalized) v /= norm;
    }
    
    index_->addPoint(normalized.data(), id);
    metadataMap_[id] = metadata;
}

std::vector<VectorSearchResult> VectorStore::search(const std::vector<float>& query, int k) {
    std::shared_lock<std::shared_mutex> lock(rwMutex_);
    std::vector<VectorSearchResult> results;
    if (!loaded_ || !index_ || query.size() != dim_ || index_->cur_element_count == 0) return results;
    
    std::vector<float> normalized = query;
    float norm = 0.0f;
    for (float v : normalized) norm += v * v;
    if (norm > 0) {
        norm = std::sqrt(norm);
        for (float& v : normalized) v /= norm;
    }
    
    auto result_queue = index_->searchKnn(normalized.data(), k);
    while (!result_queue.empty()) {
        auto top = result_queue.top();
        result_queue.pop();
        
        VectorSearchResult res;
        res.id = top.second;
        res.distance = top.first;
        if (metadataMap_.find(res.id) != metadataMap_.end()) {
            res.metadata = metadataMap_[res.id];
        }
        results.push_back(res);
    }
    std::reverse(results.begin(), results.end());
    return results;
}

bool VectorStore::save(const std::string& path) {
    std::shared_lock<std::shared_mutex> lock(rwMutex_);
    if (!loaded_ || !index_) return false;
    try {
        index_->saveIndex(path + ".index");
        std::ofstream metaOut(path + ".meta");
        for (const auto& pair : metadataMap_) {
            metaOut << pair.first << "\t" << pair.second << "\n";
        }
        return true;
    } catch (...) {
        return false;
    }
}

bool VectorStore::load(const std::string& path, int dim, int maxElements) {
    std::unique_lock<std::shared_mutex> lock(rwMutex_);
    
    try {
        auto newSpace = std::make_unique<hnswlib::InnerProductSpace>(dim);
        auto newIndex = std::make_unique<hnswlib::HierarchicalNSW<float>>(newSpace.get(), path + ".index");
        
        std::map<uint64_t, std::string> newMetadataMap;
        std::ifstream metaIn(path + ".meta");
        if (!metaIn.is_open()) {
            std::cerr << "Failed to open metadata file: " << path << ".meta\n";
        } else {
            std::string line;
            while (std::getline(metaIn, line)) {
                size_t tabPos = line.find('\t');
                if (tabPos != std::string::npos) {
                    uint64_t id = std::stoull(line.substr(0, tabPos));
                    std::string metadata = line.substr(tabPos + 1);
                    newMetadataMap[id] = metadata;
                }
            }
        }
        
        // Commit
        space_ = std::move(newSpace);
        index_ = std::move(newIndex);
        metadataMap_ = std::move(newMetadataMap);
        dim_ = dim;
        maxElements_ = maxElements;
        loaded_ = true;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Exception loading VectorStore: " << e.what() << "\n";
        return false;
    } catch (...) {
        std::cerr << "Unknown exception loading VectorStore\n";
        return false;
    }
}

void VectorStore::addDocumentsBatch(const std::vector<std::pair<uint64_t, std::vector<float>>>& docs) {
    if (!loaded_ || !index_) return;
    
    std::vector<std::vector<float>> normalized;
    normalized.reserve(docs.size());
    
    for (const auto& [id, embedding] : docs) {
        if (embedding.size() != static_cast<size_t>(dim_)) continue;
        
        std::vector<float> norm = embedding;
        float len = 0.0f;
        for (float v : norm) len += v * v;
        len = std::sqrt(len);
        if (len > 0.0f) {
            for (float& v : norm) v /= len;
        }
        normalized.push_back(std::move(norm));
    }
    
    size_t ni = 0;
    for (size_t i = 0; i < docs.size(); ++i) {
        if (docs[i].second.size() != static_cast<size_t>(dim_)) continue;
        index_->addPoint(normalized[ni].data(), docs[i].first);
        metadataMap_[docs[i].first] = "";
        ++ni;
    }
}
