#pragma once
#include "BeliefState.h"
#include "input/encoding/SensoryObservation.h"
#include "brain/memory/ProceduralStore.h"
#include <vector>
#include <algorithm>
#include <cmath>

namespace yuki::inference {
class GenerativeModel {
public:
    // Dimension constants
    static constexpr size_t AUDIO_DIM  = 8;
    static constexpr size_t VISUAL_DIM = 10;
    static constexpr size_t TEXT_DIM   = 12;
    static constexpr size_t NUM_INTENTS = 8;

    GenerativeModel();

    // Inject ProceduralStore for persistence
    void setProceduralStore(yuki::memory::ProceduralStore* store) { proc_store_ = store; }

    void bootstrapStructuralPriors();  // one-time: seed known linguistic patterns

    // Map intent to mean feature vector (μ) based on domain priors
    yuki::perception::FeatureVector likelihood(
        const BeliefState::MAPState& state,
        yuki::perception::Modality modality) const;

    // Compute residual error between predicted and observed features
    std::vector<float> predictionError(
        const yuki::perception::SensoryObservation& obs,
        const BeliefState& belief) const;

    // Expected sensory precision (1/σ²) per dimension for this modality
    // Now data-driven from learned variance instead of hardcoded uniform
    yuki::perception::PrecisionMatrix expectedPrecision(
        yuki::perception::Modality modality) const;

    // Compute negative log-likelihood: -log N(obs; μ_intent, diag(σ²_intent))
    // Used by FreeEnergyCalculator for real FEP math
    float negLogLikelihood(const std::vector<float>& obs,
                           yuki::IntentClass intent,
                           yuki::perception::Modality modality) const;

    // Get learned variance (σ²) for a specific intent+modality
    const std::vector<float>& getVariance(yuki::IntentClass intent,
                                           yuki::perception::Modality modality) const;

    // Update generative model from observations: online EM on both μ and σ²
    void updateMapping(yuki::IntentClass intent,
                       yuki::perception::Modality modality,
                       const std::vector<float>& observed_features,
                       float learning_rate = 0.05f);

    // Decay old mappings toward neutral to prevent overfitting
    void decayMappings_(float decay_factor = 0.999f);

    // Persistence: save/load learned mappings (μ + σ²) to/from ProceduralStore
    bool saveMappings(const std::string& blob_id = "generative_model_v1");
    bool loadMappings(const std::string& blob_id = "generative_model_v1");

    // Get current mean mapping for inspection/debugging
    std::vector<float> getMapping(yuki::IntentClass intent,
                                   yuki::perception::Modality modality) const;

private:
    // Mean vectors (μ) — one per intent per modality
    std::vector<float> intent_to_audio_features_[NUM_INTENTS];
    std::vector<float> intent_to_visual_features_[NUM_INTENTS];
    std::vector<float> intent_to_text_features_[NUM_INTENTS];

    // Variance vectors (σ²) — one per intent per modality, initialized to 1.0 (max uncertainty)
    std::vector<float> intent_to_audio_variance_[NUM_INTENTS];
    std::vector<float> intent_to_visual_variance_[NUM_INTENTS];
    std::vector<float> intent_to_text_variance_[NUM_INTENTS];

    // Fallback variance for intents/modalities not yet learned
    mutable std::vector<float> fallback_variance_;

    void initializeMappings_();

    // Helper: get mutable mean array pointer
    std::vector<float>* getMeanArrayMutable_(yuki::IntentClass intent,
                                              yuki::perception::Modality modality);
    // Helper: get mutable variance array pointer
    std::vector<float>* getVarianceArrayMutable_(yuki::IntentClass intent,
                                                  yuki::perception::Modality modality);

    yuki::memory::ProceduralStore* proc_store_ = nullptr;
};
}
