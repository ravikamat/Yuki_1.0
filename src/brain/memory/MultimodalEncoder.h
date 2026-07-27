#pragma once
#include <cstdint>
#include <vector>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <string>
#include "brain/memory/HdcSemanticGraph.h"
#include "brain/learning/neural/Matrix.h"

namespace yuki::memory {

enum class ModalityType : uint8_t { AUDIO = 0, VISUAL = 1, TEXT = 2, COUNT = 3 };

struct ModalitySample {
    ModalityType type;
    std::vector<float> features;
    Hypervector hdc;  // cached projection
};

class BindingMatrix {
public:
    BindingMatrix();
    explicit BindingMatrix(const std::string& config_path);

    // Projects continuous features into HDC bipolar {-1,+1}, then binarizes to {0,1}
    Hypervector project(const std::vector<float>& features, ModalityType type);

    // InfoNCE contrastive training step.
    void trainStep(const ModalitySample& anchor,
                   const ModalitySample& positive,
                   const std::vector<ModalitySample>& negatives,
                   float learning_rate);

    // Binary persistence: magic "YBM0" + version + weight matrices + CRC32
    void save(const std::string& path) const;
    bool load(const std::string& path);

    // Cosine similarity in bipolar space: dot(h1, h2) / (|h1|*|h2|)
    static float similarity(const Hypervector& a, const Hypervector& b);

private:
    std::unordered_map<ModalityType, std::unique_ptr<yuki::learning::neural::Matrix>> W_; // [hdc_dim x input_dim]
    std::unordered_map<ModalityType, std::vector<float>> b_;                       // [hdc_dim]
    float temperature_ = 0.07f;
    mutable std::mutex mtx_;

    size_t hdcDim() const { return 10000; }
    void initializeWeights(ModalityType type, size_t in_dim);
    float infoNCELoss(const Hypervector& anchor,
                      const Hypervector& positive,
                      const std::vector<Hypervector>& negatives) const;
};

class MultimodalEncoder {
public:
    explicit MultimodalEncoder(BindingMatrix* binding);

    // Fuses three raw modality vectors into a single bound HDC hypervector.
    // Binding: hdc_audio XOR hdc_visual XOR PERM(hdc_text, 1)
    Hypervector fuse(const std::vector<float>& audio_mfcc,
                     const std::vector<float>& visual_hog,
                     const std::vector<float>& text_w2v);

    // Cross-modal query: given a text concept, find top-K visual concept IDs
    std::vector<int64_t> queryVisualByText(const std::string& text_query,
                                           const HdcSemanticGraph& graph,
                                           size_t top_k);

    // Training entry: samples a positive pair from graph and hard negatives.
    void onlineTrainStep(const HdcSemanticGraph& graph, float lr);

    static Hypervector permute(const Hypervector& hv, size_t shift);

private:
    BindingMatrix* binding_;
    std::mutex mtx_;
};

} // namespace yuki::memory
