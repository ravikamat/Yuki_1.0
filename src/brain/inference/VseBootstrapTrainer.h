#pragma once
// VseBootstrapTrainer.h
// Bootstraps the VSE GenerativeModel with synthetic training data.
// 160 examples across 8 intent classes, 4 modalities (text focus).
// Runs ONCE at startup; the model then continues learning from real turns.

#include "brain/inference/VariationalStateEstimator.h"
#include "brain/predictive/predictive_turn_engine.h"  // IntentClass
#include <string>
#include <vector>

namespace yuki::inference {

class VseBootstrapTrainer {
public:
    explicit VseBootstrapTrainer(VariationalStateEstimator* vse);

    // Inject all synthetic training examples into the generative model.
    // Returns number of examples successfully injected.
    int injectAll();

private:
    struct TrainingExample {
        IntentClass intent;
        yuki::perception::Modality modality;
        std::vector<float> features;   // 12-dim text feature vector
    };

    VariationalStateEstimator* vse_;

    std::vector<TrainingExample> buildExamples_() const;

    // Feature helpers — map linguistic signals to 12-dim float vector
    // Dims: [0]=length_norm  [1]=word_count_norm  [2]=question  [3]=command
    //       [4]=emotional    [5]=technical        [6]=greeting  [7]=urgency
    //       [8]=yuki_name    [9]=action_cue       [10]=polarity [11]=confidence
    static std::vector<float> queryFeatures_(bool is_question, bool has_topic,
                                             bool is_technical);
    static std::vector<float> commandFeatures_(bool is_imperative, bool has_target,
                                               bool is_short);
    static std::vector<float> tutorialFeatures_();
    static std::vector<float> emotionalFeatures_(bool positive);
    static std::vector<float> clarificationFeatures_();
    static std::vector<float> metaFeatures_();
    static std::vector<float> abortFeatures_();
    static std::vector<float> generalFeatures_();
};

} // namespace yuki::inference
