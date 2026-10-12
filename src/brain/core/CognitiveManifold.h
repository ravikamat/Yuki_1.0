#pragma once
// CognitiveManifold — Multi-scale shared latent space
// 128D (fast), 512D (standard), 1024D (deep) with bidirectional projectors
// Sources: VSE(24D), HDC(10K sparse), YNC(128D), Embeddings(384D), Symbolic(64D)

#include <array>
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <mutex>
#include "brain/inference/VariationalStateEstimator.h"
#include "brain/memory/HdcSemanticGraph.h"
#include "brain/ync/NeuromorphicSimulator.h"

namespace yuki {
namespace brain {
namespace core {

enum class ManifoldScale {
    FAST = 0,      // 128D - rapid processing, reflexive
    STANDARD = 1,  // 512D - balanced, main workspace
    DEEP = 2       // 1024D - deep reasoning, planning
};

struct ManifoldConfig {
    // Scale dimensions
    size_t fast_dim = 128;
    size_t standard_dim = 512;
    size_t deep_dim = 1024;
    
    // Source dimensions
    size_t vse_dim = 24;
    size_t hdc_dim = 10000;
    size_t ync_dim = 128;
    size_t embed_dim = 384;
    size_t symbolic_dim = 64;
    
    // Projector config
    float projector_lr = 0.001f;
    bool use_orthogonal = true;
    float reconstruction_weight = 1.0f;
    float alignment_weight = 0.5f;
    float scale_coupling_weight = 0.3f;  // Cross-scale coupling
    
    // Alignment targets
    bool align_vse_hdc = true;
    bool align_vse_ync = true;
    bool align_vse_embed = true;
    bool align_hdc_ync = true;
    
    // Sparsity for HDC projectors
    float hdc_sparsity = 0.005f;
    
    // Scale-specific learning rates
    float fast_lr = 0.01f;
    float standard_lr = 0.001f;
    float deep_lr = 0.0001f;
};

struct ProjectionResult {
    std::array<std::vector<float>, 3> manifold_vecs;  // [FAST, STANDARD, DEEP]
    float reconstruction_error = 0.0f;
    std::unordered_map<std::string, float> source_errors;
    std::array<float, 3> scale_errors = {0.0f, 0.0f, 0.0f};
};

struct ScaleState {
    std::vector<float> manifold;
    std::chrono::steady_clock::time_point last_update;
    float activity_level = 0.0f;
};

class CognitiveManifold {
public:
    explicit CognitiveManifold(const ManifoldConfig& cfg = ManifoldConfig{});
    ~CognitiveManifold() = default;
    
    void setVSE(inference::VariationalStateEstimator* vse) { vse_ = vse; }
    void setHDC(yuki::memory::HdcSemanticGraph* hdc) { hdc_ = hdc; }
    void setYNC(ync::NeuromorphicSimulator* ync) { ync_ = ync; }
    
    // ─────────────────────────────────────────────────────────────────────────
    // Projection: Source → Manifold (all scales)
    // ─────────────────────────────────────────────────────────────────────────
    
    // VSE belief → all scales
    std::array<std::vector<float>, 3> projectVSE(const std::array<float, 8>& q_intent,
                                                  const std::array<float, 3>& q_engagement,
                                                  const std::array<float, 2>& q_urgency,
                                                  float safety_mass, float surprise_budget, float entropy);
    
    // HDC concept → all scales
    std::array<std::vector<float>, 3> projectHDC(const std::vector<float>& hdc_vector);
    
    // YNC motor state → all scales
    std::array<std::vector<float>, 3> projectYNC(const std::vector<float>& motor_output);
    
    // Text embedding → all scales
    std::array<std::vector<float>, 3> projectEmbedding(const std::vector<float>& embedding);
    
    // Symbolic/fact → all scales
    std::array<std::vector<float>, 3> projectSymbolic(const std::vector<std::pair<std::string, float>>& facts);
    
    // ─────────────────────────────────────────────────────────────────────────
    // Reverse projection: Manifold → Source
    // ─────────────────────────────────────────────────────────────────────────
    
    // Manifold (any scale) → VSE belief update
    void projectToVSE(const std::vector<float>& manifold, ManifoldScale scale,
                      std::array<float, 8>& q_intent,
                      std::array<float, 3>& q_engagement,
                      std::array<float, 2>& q_urgency,
                      float& safety_mass, float& surprise_budget, float& entropy) const;
    
    // Manifold → HDC concept activation
    std::vector<std::pair<std::string, float>> projectToHDC(const std::vector<float>& manifold, 
                                                            ManifoldScale scale,
                                                            int top_k = 10) const;
    
    // Manifold → YNC motor command
    std::vector<float> projectToYNC(const std::vector<float>& manifold, ManifoldScale scale) const;
    
    // Manifold → Text embedding
    std::vector<float> projectToEmbedding(const std::vector<float>& manifold, ManifoldScale scale) const;
    
    // ─────────────────────────────────────────────────────────────────────────
    // Cross-scale operations
    // ─────────────────────────────────────────────────────────────────────────
    
    // Upscale: FAST → STANDARD → DEEP
    std::vector<float> upscale(const std::vector<float>& src, ManifoldScale from_scale) const;
    
    // Downscale: DEEP → STANDARD → FAST
    std::vector<float> downscale(const std::vector<float>& src, ManifoldScale from_scale) const;
    
    // Fuse multi-scale representations
    std::array<std::vector<float>, 3> fuseScales(const std::array<std::vector<float>, 3>& scales) const;
    
    // ─────────────────────────────────────────────────────────────────────────
    // Training / Alignment
    // ─────────────────────────────────────────────────────────────────────────
    
    // Align paired samples across scales
    void align(const std::vector<float>& source_a, const std::vector<float>& source_b,
               const std::string& source_a_name, const std::string& source_b_name);
    
    // Align across scales
    void alignScales(ManifoldScale from_scale, ManifoldScale to_scale);
    
    // Reconstruct all sources from manifold (for training)
    ProjectionResult reconstructAll(const std::array<std::vector<float>, 3>& manifolds) const;
    
    // Step: online learning with scale-specific rates
    void step();
    
    // Get manifold states
    const std::array<ScaleState, 3>& getScales() const { return scales_; }
    const ScaleState& getScale(ManifoldScale scale) const { return scales_[static_cast<size_t>(scale)]; }
    ScaleState& getScale(ManifoldScale scale) { return scales_[static_cast<size_t>(scale)]; }
    
    void setManifold(ManifoldScale scale, const std::vector<float>& m);
    const std::vector<float>& getManifold(ManifoldScale scale) const { return scales_[static_cast<size_t>(scale)].manifold; }

private:
    ManifoldConfig config_;
    std::array<ScaleState, 3> scales_;
    
    inference::VariationalStateEstimator* vse_ = nullptr;
    yuki::memory::HdcSemanticGraph* hdc_ = nullptr;
    ync::NeuromorphicSimulator* ync_ = nullptr;
    
    // Projector matrices per scale
    // Forward: source → scale
    struct ScaleProjectors {
        // Dense projectors (source_dim × scale_dim)
        std::vector<std::vector<float>> W_vse_to_m;
        std::vector<std::vector<float>> W_hdc_to_m;     // sparse
        std::vector<std::vector<float>> W_ync_to_m;
        std::vector<std::vector<float>> W_embed_to_m;
        std::vector<std::vector<float>> W_symbolic_to_m;
        
        // Reverse projectors (scale_dim × source_dim)
        std::vector<std::vector<float>> W_m_to_vse;
        std::vector<std::vector<float>> W_m_to_hdc;     // sparse
        std::vector<std::vector<float>> W_m_to_ync;
        std::vector<std::vector<float>> W_m_to_embed;
        std::vector<std::vector<float>> W_m_to_symbolic;
        
        // Cross-scale projectors
        std::vector<std::vector<float>> W_upscale;   // smaller → larger
        std::vector<std::vector<float>> W_downscale; // larger → smaller
    };
    std::array<ScaleProjectors, 3> projectors_;
    
    // Optimizer state per scale
    struct AdamState {
        std::vector<float> m, v;
        int t = 0;
    };
    std::array<std::vector<AdamState>, 3> optimizer_states_;
    
    void initializeProjectors();
    void initializeAdamStates();
    
    // Orthogonalization
    void orthogonalize(std::vector<std::vector<float>>& W) const;
    
    // Initialize random projectors for a scale
    void initializeScaleProjectors(ScaleProjectors& proj, size_t scale_dim, float lr);
    
    // Matrix-vector multiply
    std::vector<float> matvec(const std::vector<std::vector<float>>& W, 
                              const std::vector<float>& x) const;
    
    // Sparse HDC projection
    std::vector<float> sparseProject(const std::vector<std::vector<float>>& W,
                                      const std::vector<float>& x) const;
    
    // VSE vector construction
    std::vector<float> buildVSEVector(const std::array<float, 8>& q_intent,
                                       const std::array<float, 3>& q_engagement,
                                       const std::array<float, 2>& q_urgency,
                                       float safety_mass, float surprise_budget, float entropy) const;
    
    // Matrix operations
    std::vector<float> matmul(const std::vector<float>& A, const std::vector<float>& B,
                              size_t rows_a, size_t cols_a, size_t cols_b) const;
    
    // Activation functions
    std::vector<float> relu(const std::vector<float>& x) const;
    std::vector<float> tanh(const std::vector<float>& x) const;
    
    // Adam update
    void adamUpdate(std::vector<float>& param, std::vector<float>& grad, AdamState& state, float lr) const;
};

} // namespace core
} // namespace brain
} // namespace yuki