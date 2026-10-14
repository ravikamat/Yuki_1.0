#include "brain/infra/CognitiveConfig.h"
#include "brain/core/Logger.h"
#include <flatbuffers/flatbuffers.h>
#include <fstream>
#include <algorithm>

namespace yuki {
namespace brain {
namespace infra {

// FlatBuffers serialization
flatbuffers::Offset<ConfigSchema::CognitiveConfig> CognitiveConfig::serialize(flatbuffers::FlatBufferBuilder& builder) const {
    // Serialize all sub-configs
    auto vse_offset = ConfigSchema::CreateVSEConfig(builder,
        vse.surprise_threshold,
        vse.entropy_threshold,
        vse.precision_floor,
        vse.precision_ceil,
        vse.recovery_rate,
        vse.baseline_intent,
        vse.baseline_entity,
        vse.baseline_tone,
        vse.baseline_source,
        vse.calibration_warmup_alpha,
        vse.calibration_steady_alpha
    );
    
    auto hpc_offset = ConfigSchema::CreateHPCConfig(builder,
        hpc.l1_interval.count(),
        hpc.l2_interval.count(),
        hpc.l3_interval.count(),
        hpc.l1_dim,
        hpc.l2_dim,
        hpc.l3_dim,
        hpc.l1_precision,
        hpc.l2_precision,
        hpc.l3_precision,
        hpc.top_down_weight,
        hpc.bottom_up_weight,
        hpc.surprise_threshold
    );
    
    auto ync_offset = ConfigSchema::CreateYNCConfig(builder,
        ync.sensory_neurons,
        ync.motor_neurons,
        ync.interneurons,
        ync.connectivity_density,
        ync.learning_rate,
        ync.homeostatic_target,
        ync.stdp_a_plus,
        ync.stdp_a_minus,
        ync.stdp_tau_plus,
        ync.stdp_tau_minus
    );
    
    // Create arrays for DriveConfig
    auto drives_decay_rate = builder.CreateVector(drives.decay_rate);
    auto drives_setpoint = builder.CreateVector(drives.setpoint);
    auto drives_min_val = builder.CreateVector(drives.min_val);
    auto drives_max_val = builder.CreateVector(drives.max_val);
    
    auto drives_offset = ConfigSchema::CreateDriveConfig(builder,
        drives.curiosity_prediction_error_gain,
        drives.curiosity_competence_progress_gain,
        drives.social_prediction_error_gain,
        drives.competence_success_gain,
        drives.competence_failure_gain,
        drives.energy_compute_cost_gain,
        drives.safety_surprise_gain,
        drives.autonomy_control_gain,
        drives_decay_rate,
        drives_setpoint,
        drives_min_val,
        drives_max_val
    );
    
    // ... (other configs would be serialized similarly)
    // For brevity, creating minimal flatbuffer
    
    auto config_offset = ConfigSchema::CreateCognitiveConfig(builder,
        schema_version,
        config_version,
        vse_offset,
        hpc_offset,
        ync_offset,
        drives_offset,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    );
    
    builder.Finish(config_offset);
    return config_offset;
}

CognitiveConfig CognitiveConfig::deserialize(const flatbuffers::Table* table) {
    CognitiveConfig config;
    
    if (!table) return config;
    
    auto config_table = flatbuffers::GetRoot<ConfigSchema::CognitiveConfig>(table);
    
    config.schema_version = config_table->schema_version();
    config.config_version = config_table->config_version();
    
    if (auto vse = config_table->vse()) {
        config.vse.surprise_threshold = vse->surprise_threshold();
        config.vse.entropy_threshold = vse->entropy_threshold();
        config.vse.precision_floor = vse->precision_floor();
        config.vse.precision_ceil = vse->precision_ceil();
        config.vse.recovery_rate = vse->recovery_rate();
        config.vse.baseline_intent = vse->baseline_intent();
        config.vse.baseline_entity = vse->baseline_entity();
        config.vse.baseline_tone = vse->baseline_tone();
        config.vse.baseline_source = vse->baseline_source();
        config.vse.calibration_warmup_alpha = vse->calibration_warmup_alpha();
        config.vse.calibration_steady_alpha = vse->calibration_steady_alpha();
    }
    
    if (auto hpc = config_table->hpc()) {
        config.hpc.l1_interval = std::chrono::milliseconds(hpc->l1_interval_ms());
        config.hpc.l2_interval = std::chrono::milliseconds(hpc->l2_interval_ms());
        config.hpc.l3_interval = std::chrono::milliseconds(hpc->l3_interval_ms());
        config.hpc.l1_dim = hpc->l1_dim();
        config.hpc.l2_dim = hpc->l2_dim();
        config.hpc.l3_dim = hpc->l3_dim();
        config.hpc.l1_precision = hpc->l1_precision();
        config.hpc.l2_precision = hpc->l2_precision();
        config.hpc.l3_precision = hpc->l3_precision();
        config.hpc.top_down_weight = hpc->top_down_weight();
        config.hpc.bottom_up_weight = hpc->bottom_up_weight();
        config.hpc.surprise_threshold = hpc->surprise_threshold();
    }
    
    if (auto ync = config_table->ync()) {
        config.ync.sensory_neurons = ync->sensory_neurons();
        config.ync.motor_neurons = ync->motor_neurons();
        config.ync.interneurons = ync->interneurons();
        config.ync.connectivity_density = ync->connectivity_density();
        config.ync.learning_rate = ync->learning_rate();
        config.ync.homeostatic_target = ync->homeostatic_target();
        config.ync.stdp_a_plus = ync->stdp_a_plus();
        config.ync.stdp_a_minus = ync->stdp_a_minus();
        config.ync.stdp_tau_plus = ync->stdp_tau_plus();
        config.ync.stdp_tau_minus = ync->stdp_tau_minus();
    }
    
    if (auto drives = config_table->drives()) {
        config.drives.curiosity_prediction_error_gain = drives->curiosity_prediction_error_gain();
        config.drives.curiosity_competence_progress_gain = drives->curiosity_competence_progress_gain();
        config.drives.social_prediction_error_gain = drives->social_prediction_error_gain();
        config.drives.competence_success_gain = drives->competence_success_gain();
        config.drives.competence_failure_gain = drives->competence_failure_gain();
        config.drives.energy_compute_cost_gain = drives->energy_compute_cost_gain();
        config.drives.safety_surprise_gain = drives->safety_surprise_gain();
        config.drives.autonomy_control_gain = drives->autonomy_control_gain();
        
        if (auto decay = drives->decay_rate()) {
            for (size_t i = 0; i < 6 && i < decay->size(); ++i) {
                config.drives.decay_rate[i] = decay->Get(i);
            }
        }
        if (auto setpoint = drives->setpoint()) {
            for (size_t i = 0; i < 6 && i < setpoint->size(); ++i) {
                config.drives.setpoint[i] = setpoint->Get(i);
            }
        }
        if (auto min_v = drives->min_val()) {
            for (size_t i = 0; i < 6 && i < min_v->size(); ++i) {
                config.drives.min_val[i] = min_v->Get(i);
            }
        }
        if (auto max_v = drives->max_val()) {
            for (size_t i = 0; i < 6 && i < max_v->size(); ++i) {
                config.drives.max_val[i] = max_v->Get(i);
            }
        }
    }
    
    return config;
}

bool CognitiveConfig::serializeToFile(const std::string& filepath) const {
    flatbuffers::FlatBufferBuilder builder(1024 * 1024);
    auto offset = serialize(builder);
    builder.Finish(offset);
    
    std::ofstream file(filepath, std::ios::binary);
    if (!file) return false;
    
    file.write(reinterpret_cast<const char*>(builder.GetBufferPointer()), builder.GetSize());
    return file.good();
}

CognitiveConfig CognitiveConfig::deserializeFromFile(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file) return CognitiveConfig{};
    
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::vector<uint8_t> buffer(size);
    file.read(reinterpret_cast<char*>(buffer.data()), size);
    
    auto verifier = flatbuffers::Verifier(buffer.data(), buffer.size());
    if (!ConfigSchema::VerifyCognitiveConfigBuffer(verifier)) {
        return CognitiveConfig{};
    }
    
    auto table = flatbuffers::GetRoot<ConfigSchema::CognitiveConfig>(buffer.data());
    return deserialize(table);
}

bool CognitiveConfig::hotReload(const std::string& filepath) {
    try {
        auto new_config = deserializeFromFile(filepath);
        if (new_config.validate()) {
            *this = std::move(new_config);
            yuki::core::Logger::instance().log(yuki::core::LogLevel::INFO, "CognitiveConfig", 
                "Hot reload successful: " + filepath);
            
            if (hot_reload_callback_) {
                hot_reload_callback_(*this);
            }
            return true;
        }
    } catch (...) {
        yuki::core::Logger::instance().log(yuki::core::LogLevel::ERROR, "CognitiveConfig",
            "Hot reload failed: " + filepath);
    }
    return false;
}

bool CognitiveConfig::validate() const {
    auto errors = getValidationErrors();
    return errors.empty();
}

std::vector<std::string> CognitiveConfig::getValidationErrors() const {
    std::vector<std::string> errors;
    
    // VSE validation
    if (vse.surprise_threshold <= 0 || vse.surprise_threshold > 1)
        errors.push_back("VSE surprise_threshold must be in (0, 1]");
    if (vse.precision_floor >= vse.precision_ceil)
        errors.push_back("VSE precision_floor must be < precision_ceil");
    
    // HPC validation
    if (hpc.l1_interval.count() <= 0) errors.push_back("HPC l1_interval must be positive");
    if (hpc.l1_dim == 0) errors.push_back("HPC l1_dim must be > 0");
    
    // YNC validation
    if (ync.sensory_neurons == 0) errors.push_back("YNC sensory_neurons must be > 0");
    
    // Drives validation
    for (int i = 0; i < 6; ++i) {
        if (drives.min_val[i] >= drives.max_val[i])
            errors.push_back("Drive " + std::to_string(i) + " min_val must be < max_val");
    }
    
    return errors;
}

std::vector<std::string> CognitiveConfig::diff(const CognitiveConfig& a, const CognitiveConfig& b) {
    std::vector<std::string> diffs;
    
    if (a.schema_version != b.schema_version)
        diffs.push_back("schema_version: " + std::to_string(a.schema_version) + " -> " + std::to_string(b.schema_version));
    if (a.config_version != b.config_version)
        diffs.push_back("config_version: " + std::to_string(a.config_version) + " -> " + std::to_string(b.config_version));
    
    // Compare key fields (simplified)
    #define CHECK_DIFF(field) \
        if (a.field != b.field) \
            diffs.push_back(#field ": " + std::to_string(a.field) + " -> " + std::to_string(b.field));
    
    CHECK_DIFF(vse.surprise_threshold);
    CHECK_DIFF(vse.precision_floor);
    CHECK_DIFF(hpc.l1_interval.count());
    CHECK_DIFF(ync.sensory_neurons);
    CHECK_DIFF(drives.curiosity_prediction_error_gain);
    
    return diffs;
}

void CognitiveConfig::registerHotReloadCallback(std::function<void(const CognitiveConfig&)> callback) {
    hot_reload_callback_ = std::move(callback);
}

} // namespace infra
} // namespace brain
} // namespace yuki