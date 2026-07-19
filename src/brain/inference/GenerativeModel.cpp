#include "GenerativeModel.h"
#include "brain/predictive/predictive_turn_engine.h"
#include <cmath>
#include <algorithm>
#include <iostream>

namespace yuki::inference {
GenerativeModel::GenerativeModel() {
    initializeMappings_();
    bootstrapStructuralPriors();
}

yuki::perception::FeatureVector GenerativeModel::likelihood(
    const BeliefState::MAPState& state,
    yuki::perception::Modality modality) const
{
    yuki::perception::FeatureVector result;
    size_t intent_idx = static_cast<size_t>(state.intent);
    switch (modality) {
        case yuki::perception::Modality::AUDIO:
            result.values = intent_to_audio_features_[intent_idx];
            result.dimension_names = {"rms", "rms_ema", "spectral_proxy", "vad", "noise_floor", "dynamic_range", "has_pcm", "confidence"};
            break;
        case yuki::perception::Modality::VISUAL_CAMERA:
            result.values = intent_to_visual_features_[intent_idx];
            result.dimension_names = {"brightness", "motion", "motion_ema", "face_present", "face_density", "motion_binary", "lighting", "confidence", "bright_face", "motion_face"};
            break;
        case yuki::perception::Modality::TEXT:
            result.values = intent_to_text_features_[intent_idx];
            result.dimension_names = {"length_norm", "word_count_norm", "question", "command", "emotional", "technical", "greeting", "urgency", "yuki_name", "action_cue", "polarity", "confidence"};
            break;
        default:
            result.values = {0.5f};
            result.dimension_names = {"default"};
    }
    return result;
}

std::vector<float> GenerativeModel::predictionError(
    const yuki::perception::SensoryObservation& obs,
    const BeliefState& belief) const
{
    auto map_state = belief.getMAP();
    auto predicted = likelihood(map_state, obs.modality);
    std::vector<float> error;
    size_t n = std::min(obs.features.size(), predicted.size());
    for (size_t i = 0; i < n; ++i) {
        error.push_back(obs.features.values[i] - predicted.values[i]);
    }
    return error;
}

yuki::perception::PrecisionMatrix GenerativeModel::expectedPrecision(
    yuki::perception::Modality modality) const
{
    yuki::perception::PrecisionMatrix prec;
    switch (modality) {
        case yuki::perception::Modality::AUDIO:
            prec.setUniform(8, 1.5f); break;
        case yuki::perception::Modality::VISUAL_CAMERA:
            prec.setUniform(10, 1.0f); break;
        case yuki::perception::Modality::VISUAL_SCREEN:
            prec.setUniform(8, 0.8f); break;
        case yuki::perception::Modality::TEXT:
            prec.setUniform(12, 2.0f); break;
        default:
            prec.setUniform(1, 1.0f);
    }
    return prec;
}

void GenerativeModel::initializeMappings_() {
    intent_to_text_features_[static_cast<size_t>(yuki::IntentClass::QUERY)] = {0.3f, 0.2f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f};
    intent_to_text_features_[static_cast<size_t>(yuki::IntentClass::COMMAND)] = {0.3f, 0.2f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f};
    intent_to_text_features_[static_cast<size_t>(yuki::IntentClass::TUTORIAL)] = {0.5f, 0.4f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f};
    intent_to_text_features_[static_cast<size_t>(yuki::IntentClass::EMOTIONAL_VENT)] = {0.4f, 0.3f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.5f, 0.8f};
    for (size_t i = 0; i < 8; ++i) {
        if (intent_to_text_features_[i].empty()) {
            intent_to_text_features_[i] = std::vector<float>(12, 0.5f);
        }
    }
    for (size_t i = 0; i < 8; ++i) {
        intent_to_audio_features_[i] = std::vector<float>(8, 0.3f);
    }
    for (size_t i = 0; i < 8; ++i) {
        intent_to_visual_features_[i] = std::vector<float>(10, 0.4f);
    }
}

void GenerativeModel::bootstrapStructuralPriors() {
    // Fix: indices must match the 12-dim text_obs layout built in predictive_turn_engine.cpp:
    // [0]=length_norm [1]=word_count_norm [2]=question [3]=command [4]=emotional
    // [5]=technical   [6]=greeting        [7]=urgency  [8]=yuki_name [9]=action_cue
    // [10]=polarity   [11]=confidence
    //
    // Previous code had QUERY[0]=0.9 (wrong: length_norm) and COMMAND[1]=0.9 (wrong: word_count).
    // UNKNOWN(0)'s neutral [0.5]*12 was always closest because discriminative dims were wrong.
    intent_to_text_features_[static_cast<size_t>(yuki::IntentClass::QUERY)][2]  = 0.9f;  // question  ← was [0]
    intent_to_text_features_[static_cast<size_t>(yuki::IntentClass::QUERY)][11] = 0.9f;  // confidence ← was [8]
    intent_to_text_features_[static_cast<size_t>(yuki::IntentClass::COMMAND)][3]  = 0.9f; // command   ← was [1]
    intent_to_text_features_[static_cast<size_t>(yuki::IntentClass::COMMAND)][9]  = 0.5f; // action_cue (new)
    intent_to_text_features_[static_cast<size_t>(yuki::IntentClass::COMMAND)][11] = 0.9f; // confidence (new)
    intent_to_text_features_[static_cast<size_t>(yuki::IntentClass::META_QUESTION)][6]  = 0.9f; // greeting (new)
    intent_to_text_features_[static_cast<size_t>(yuki::IntentClass::META_QUESTION)][11] = 0.9f; // confidence
    intent_to_text_features_[static_cast<size_t>(yuki::IntentClass::EMOTIONAL_VENT)][4]  = 0.9f; // emotional
    intent_to_text_features_[static_cast<size_t>(yuki::IntentClass::EMOTIONAL_VENT)][11] = 0.9f; // confidence
    intent_to_text_features_[static_cast<size_t>(yuki::IntentClass::TUTORIAL)][5]  = 0.9f; // technical
    intent_to_text_features_[static_cast<size_t>(yuki::IntentClass::TUTORIAL)][11] = 0.9f; // confidence
    std::cout << "[GenerativeModel] structural priors bootstrapped (12-dim aligned)" << std::endl;
}
// ── Learning: Update generative model from observations ─────────────────────

void GenerativeModel::updateMapping(yuki::IntentClass intent,
                                     yuki::perception::Modality modality,
                                     const std::vector<float>& observed_features,
                                     float learning_rate)
{
    size_t intent_idx = static_cast<size_t>(intent);
    if (intent_idx >= 8) return;

    std::vector<float>* target = nullptr;
    size_t expected_dims = 0;
    switch (modality) {
        case yuki::perception::Modality::AUDIO:
            target = &intent_to_audio_features_[intent_idx];
            expected_dims = 8;
            break;
        case yuki::perception::Modality::VISUAL_CAMERA:
            target = &intent_to_visual_features_[intent_idx];
            expected_dims = 10;
            break;
        case yuki::perception::Modality::VISUAL_SCREEN:
            target = nullptr; // Screen features don't map to intent directly
            return;
        case yuki::perception::Modality::TEXT:
            target = &intent_to_text_features_[intent_idx];
            expected_dims = 12;
            break;
        default:
            return;
    }

    if (!target || target->empty()) {
        // Initialize if not already set
        if (target) target->assign(expected_dims, 0.5f);
        return;
    }

    // EMA update: mapping = (1 - lr) * mapping + lr * observed
    size_t n = std::min(target->size(), observed_features.size());
    for (size_t i = 0; i < n; ++i) {
        (*target)[i] = (1.0f - learning_rate) * (*target)[i] + learning_rate * observed_features[i];
    }

    // Clamp to [0, 1] to keep features in valid range
    for (auto& v : *target) {
        v = std::max(0.0f, std::min(1.0f, v));
    }
}

void GenerativeModel::decayMappings_(float decay_factor) {
    for (size_t i = 0; i < 8; ++i) {
        for (auto& v : intent_to_audio_features_[i]) {
            v = decay_factor * v + (1.0f - decay_factor) * 0.5f;
        }
        for (auto& v : intent_to_visual_features_[i]) {
            v = decay_factor * v + (1.0f - decay_factor) * 0.5f;
        }
        for (auto& v : intent_to_text_features_[i]) {
            v = decay_factor * v + (1.0f - decay_factor) * 0.5f;
        }
    }
}

std::vector<float> GenerativeModel::getMapping(yuki::IntentClass intent,
                                                yuki::perception::Modality modality) const
{
    size_t intent_idx = static_cast<size_t>(intent);
    if (intent_idx >= 8) return {};

    switch (modality) {
        case yuki::perception::Modality::AUDIO:
            return intent_to_audio_features_[intent_idx];
        case yuki::perception::Modality::VISUAL_CAMERA:
            return intent_to_visual_features_[intent_idx];
        case yuki::perception::Modality::TEXT:
            return intent_to_text_features_[intent_idx];
        default:
            return {};
    }
}

bool GenerativeModel::saveMappings(const std::string& blob_id) {
    if (!proc_store_) return false;

    std::vector<uint8_t> blob;
    // Magic header "YGM1"
    blob.push_back('Y'); blob.push_back('G'); blob.push_back('M'); blob.push_back('1');
    // Version = 1
    uint32_t version = 1;
    const uint8_t* v_ptr = reinterpret_cast<const uint8_t*>(&version);
    blob.insert(blob.end(), v_ptr, v_ptr + sizeof(uint32_t));

    // Serialize 8 intents * (8 audio + 10 visual + 12 text) floats
    for (size_t i = 0; i < 8; ++i) {
        auto append_floats = [&blob](const std::vector<float>& vec, size_t expected_size) {
            for (size_t j = 0; j < expected_size; ++j) {
                float val = (j < vec.size()) ? vec[j] : 0.5f;
                const uint8_t* f_ptr = reinterpret_cast<const uint8_t*>(&val);
                blob.insert(blob.end(), f_ptr, f_ptr + sizeof(float));
            }
        };
        append_floats(intent_to_audio_features_[i], 8);
        append_floats(intent_to_visual_features_[i], 10);
        append_floats(intent_to_text_features_[i], 12);
    }

    return proc_store_->store(blob_id, yuki::memory::ProceduralStore::BlobType::GENERATIVE_MODEL, blob);
}

bool GenerativeModel::loadMappings(const std::string& blob_id) {
    if (!proc_store_) return false;

    auto blob_opt = proc_store_->retrieve(blob_id);
    if (!blob_opt) return false;

    const auto& blob = *blob_opt;
    size_t min_size = 8 + 8 * (8 + 10 + 12) * sizeof(float);
    if (blob.size() < min_size) {
        std::cerr << "[GenerativeModel] Blob size " << blob.size() << " too small for " << blob_id << std::endl;
        return false;
    }

    if (blob[0] != 'Y' || blob[1] != 'G' || blob[2] != 'M' || blob[3] != '1') {
        std::cerr << "[GenerativeModel] Invalid magic in blob " << blob_id << std::endl;
        return false;
    }

    uint32_t version;
    std::memcpy(&version, &blob[4], sizeof(uint32_t));
    if (version != 1) {
        std::cerr << "[GenerativeModel] Unsupported version " << version << " in blob " << blob_id << std::endl;
        return false;
    }

    size_t offset = 8;
    for (size_t i = 0; i < 8; ++i) {
        auto read_floats = [&blob, &offset](std::vector<float>& vec, size_t count) {
            vec.resize(count);
            for (size_t j = 0; j < count; ++j) {
                std::memcpy(&vec[j], &blob[offset], sizeof(float));
                offset += sizeof(float);
            }
        };
        read_floats(intent_to_audio_features_[i], 8);
        read_floats(intent_to_visual_features_[i], 10);
        read_floats(intent_to_text_features_[i], 12);
    }

    std::cout << "[GenerativeModel] Loaded mappings from ProceduralStore (" << blob_id << ")\n";
    return true;
}

}
