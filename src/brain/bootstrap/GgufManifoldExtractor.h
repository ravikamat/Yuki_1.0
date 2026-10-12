#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <optional>

namespace yuki { namespace bootstrap {

struct SemanticManifold {
    std::string concept_name;
    std::vector<float> centroid;
    std::vector<std::pair<std::string, float>> neighbors;
    std::vector<std::string> syntactic_patterns;
    std::string domain_tag;
    float entropy = 0.0f;
    uint32_t vocab_index = 0;
};

struct GgufTensorView {
    const void* data = nullptr;
    std::vector<uint64_t> shape;
    uint32_t type = 0;
    std::string name;
};

struct BootstrapPackage {
    std::vector<SemanticManifold> manifolds;
    std::unordered_map<std::string, std::vector<std::string>> domain_clusters;
    std::vector<std::string> syntactic_patterns;
    std::string model_name;
    uint64_t extraction_timestamp = 0;
    uint32_t vocab_size = 0;
    uint32_t embedding_dim = 0;
};

class GgufManifoldExtractor {
public:
    explicit GgufManifoldExtractor(const std::string& gguf_path);
    ~GgufManifoldExtractor();

    bool isValid() const { return valid_; }
    std::string getError() const { return error_; }

    std::optional<BootstrapPackage> extractAll(uint32_t max_manifolds = 50000, float entropy_threshold = 0.1f);

    std::optional<SemanticManifold> extractManifold(const std::string& concept_token);

    std::vector<std::string> extractSyntacticPatterns(int layer_idx);

    std::unordered_map<std::string, std::vector<std::string>> extractDomainClusters(uint32_t k = 32);

    bool saveToFile(const std::string& path, const BootstrapPackage& package);

private:
    class Impl;
    std::unique_ptr<Impl> pimpl_;
    bool valid_ = false;
    std::string error_;
};

}} // namespace yuki::bootstrap