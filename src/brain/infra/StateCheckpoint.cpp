#include "brain/infra/StateCheckpoint.h"
#include "brain/core/Logger.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <random>
#include <sstream>
#include <nlohmann/json.hpp>
#include "brain/core/RandomEngine.h"

namespace yuki {
namespace brain {
namespace infra {

namespace {
// Component serialize() output is not guaranteed to be JSON (e.g. VSE uses a
// length-prefixed text encoding). Store non-JSON payloads as JSON strings so a
// legacy/foreign payload can never abort saveCheckpoint with an unhandled
// nlohmann::json::parse exception on a background thread.
nlohmann::json toJsonPayload(const std::string& data) {
    try {
        return nlohmann::json::parse(data);
    } catch (...) {
        return data;  // JSON string fallback; unwrap on load
    }
}
} // namespace


StateCheckpoint::StateCheckpoint(const CheckpointConfig& cfg)
    : config_(cfg) {
    std::filesystem::create_directories(config_.checkpoint_directory);
    
    yuki::core::Logger::instance().log(yuki::core::LogLevel::INFO, "StateCheckpoint", 
        "Initialized state checkpoint system: " + config_.checkpoint_directory);
}

StateCheckpoint::~StateCheckpoint() {
    stopPeriodicCheckpoint();
}

CheckpointResult StateCheckpoint::saveCheckpoint(const std::string& reason) {
    std::lock_guard<std::mutex> lock(checkpoint_mutex_);
    
    auto start = std::chrono::high_resolution_clock::now();
    std::string checkpoint_id = generateCheckpointId();
    
    yuki::core::Logger::instance().log(yuki::core::LogLevel::INFO, "StateCheckpoint",
        "Saving checkpoint: " + checkpoint_id + " (" + reason + ")");
    
    CheckpointMetadata metadata;
    metadata.checkpoint_id = checkpoint_id;
    metadata.timestamp = std::chrono::system_clock::now();
    metadata.session_id = "session_" + std::to_string(std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
    metadata.version = "1.0.0";
    metadata.git_commit = "unknown";
    
    nlohmann::json full_state;
    metadata.components_included.clear();
    metadata.component_sizes.clear();
    size_t total_size = 0;
    
    // Serialize VSE
    if (config_.checkpoint_vse && vse_) {
        metadata.components_included["vse"] = true;
        std::string data = vse_->serialize();
        metadata.component_sizes["vse"] = data.size();
        total_size += data.size();
        full_state["vse"] = toJsonPayload(data);
    }
    
    // Serialize HPC
    if (config_.checkpoint_hpc && hpc_) {
        metadata.components_included["hpc"] = true;
        std::string data = "{}"; // HPC doesn't have serialize yet
        metadata.component_sizes["hpc"] = data.size();
        total_size += data.size();
        full_state["hpc"] = toJsonPayload(data);
    }
    
    // Serialize drives
    if (config_.checkpoint_drives && drives_) {
        metadata.components_included["drives"] = true;
        std::string data = drives_->serialize();
        metadata.component_sizes["drives"] = data.size();
        total_size += data.size();
        full_state["drives"] = toJsonPayload(data);
    }
    
    // Serialize identity core
    if (config_.checkpoint_identity && identity_core_) {
        metadata.components_included["identity"] = true;
        std::string data = identity_core_->serialize();
        metadata.component_sizes["identity"] = data.size();
        total_size += data.size();
        full_state["identity"] = toJsonPayload(data);
    }
    
    // Serialize goal generator (deferred)
    // if (config_.checkpoint_autonomy && goal_generator_) {
    //     metadata.components_included["goal_generator"] = true;
    //     std::string data = goal_generator_->serialize();
    //     metadata.component_sizes["goal_generator"] = data.size();
    //     total_size += data.size();
    //     full_state["goal_generator"] = nlohmann::json::parse(data);
    // }
    
    // // Serialize curiosity engine
    // if (config_.checkpoint_autonomy && curiosity_engine_) {
    //     metadata.components_included["curiosity_engine"] = true;
    //     std::string data = "{}"; // CuriosityEngine doesn't have serialize yet
    //     metadata.component_sizes["curiosity_engine"] = data.size();
    //     total_size += data.size();
    //     full_state["curiosity_engine"] = nlohmann::json::parse(data);
    // }
    // 
    // // Serialize policy engine
    // if (config_.checkpoint_policy && policy_engine_) {
    //     metadata.components_included["policy_engine"] = true;
    //     std::string data = "{}"; // PolicyEngine doesn't have serialize yet
    //     metadata.component_sizes["policy_engine"] = data.size();
    //     total_size += data.size();
    //     full_state["policy_engine"] = nlohmann::json::parse(data);
    // }
    
    // Write checkpoint file
    std::string checkpoint_path = getCheckpointPath(checkpoint_id);
    std::string metadata_path = getMetadataPath(checkpoint_id);
    
    std::string full_json = full_state.dump(2);
    if (!writeCheckpointFile(checkpoint_path, full_json)) {
        return {false, checkpoint_id, "Failed to write checkpoint file", metadata};
    }
    
    std::string metadata_json = nlohmann::json{
        {"checkpoint_id", metadata.checkpoint_id},
        {"timestamp", std::chrono::duration_cast<std::chrono::seconds>(metadata.timestamp.time_since_epoch()).count()},
        {"session_id", metadata.session_id},
        {"version", metadata.version},
        {"git_commit", metadata.git_commit},
        {"components_included", metadata.components_included},
        {"component_sizes", metadata.component_sizes},
        {"total_size_bytes", total_size}
    }.dump(2);
    
    if (!writeCheckpointFile(metadata_path, metadata_json)) {
        return {false, checkpoint_id, "Failed to write metadata file", metadata};
    }
    
    metadata.total_size_bytes = total_size;
    metadata.save_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now() - start);
    
    // Cleanup old checkpoints
    auto checkpoints = listCheckpoints();
    if (checkpoints.size() > config_.max_checkpoints) {
        std::sort(checkpoints.begin(), checkpoints.end(), 
            [](const CheckpointMetadata& a, const CheckpointMetadata& b) {
                return a.timestamp < b.timestamp;
            });
        for (size_t i = 0; i < checkpoints.size() - config_.max_checkpoints; ++i) {
            deleteCheckpoint(checkpoints[i].checkpoint_id);
        }
    }
    
    return {true, checkpoint_id, "", metadata};
}

RestoreResult StateCheckpoint::loadCheckpoint(const std::string& checkpoint_id) {
    std::lock_guard<std::mutex> lock(checkpoint_mutex_);
    
    auto start = std::chrono::high_resolution_clock::now();
    
    yuki::core::Logger::instance().log(yuki::core::LogLevel::INFO, "StateCheckpoint",
        "Loading checkpoint: " + checkpoint_id);
    
    std::string checkpoint_path = getCheckpointPath(checkpoint_id);
    std::string data;
    
    if (!readCheckpointFile(checkpoint_path, data)) {
        return {false, "Checkpoint file not found: " + checkpoint_id, {}, 
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::high_resolution_clock::now() - start)};
    }
    
    try {
        nlohmann::json full_state = nlohmann::json::parse(data);
        
        if (config_.checkpoint_vse && vse_ && full_state.contains("vse")) {
            const auto& payload = full_state["vse"];
            vse_->deserialize(payload.is_string() ? payload.get<std::string>() : payload.dump());
        }
        if (config_.checkpoint_drives && drives_ && full_state.contains("drives")) {
            const auto& payload = full_state["drives"];
            drives_->deserialize(payload.is_string() ? payload.get<std::string>() : payload.dump());
        }
        if (config_.checkpoint_identity && identity_core_ && full_state.contains("identity")) {
            const auto& payload = full_state["identity"];
            identity_core_->deserialize(payload.is_string() ? payload.get<std::string>() : payload.dump());
        }
        // if (config_.checkpoint_autonomy && goal_generator_ && full_state.contains("goal_generator")) {
        //     goal_generator_->deserialize(full_state["goal_generator"].dump());
        // }
        // if (config_.checkpoint_policy && policy_engine_ && full_state.contains("policy_engine")) {
        //     // policy_engine_->deserialize(full_state["policy_engine"].dump());
        // }
        
        CheckpointMetadata metadata;
        metadata.checkpoint_id = checkpoint_id;
        metadata.timestamp = std::chrono::system_clock::now();
        
        return {true, "", metadata, 
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::high_resolution_clock::now() - start)};
    } catch (const std::exception& e) {
        return {false, "Failed to parse checkpoint: " + std::string(e.what()), {}, 
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::high_resolution_clock::now() - start)};
    }
}

RestoreResult StateCheckpoint::loadLatestCheckpoint() {
    auto checkpoints = listCheckpoints();
    if (checkpoints.empty()) {
        return {false, "No checkpoints found", {}, std::chrono::milliseconds(0)};
    }
    
    std::sort(checkpoints.begin(), checkpoints.end(),
        [](const CheckpointMetadata& a, const CheckpointMetadata& b) {
            return a.timestamp > b.timestamp;
        });
    
    return loadCheckpoint(checkpoints[0].checkpoint_id);
}

std::vector<CheckpointMetadata> StateCheckpoint::listCheckpoints() const {
    std::vector<CheckpointMetadata> result;
    
    try {
        for (const auto& entry : std::filesystem::directory_iterator(config_.checkpoint_directory)) {
            if (entry.path().extension() == ".meta.json") {
                std::string data;
                if (readCheckpointFile(entry.path().string(), data)) {
                    try {
                        auto j = nlohmann::json::parse(data);
                        CheckpointMetadata metadata;
                        metadata.checkpoint_id = j.value("checkpoint_id", "");
                        metadata.session_id = j.value("session_id", "");
                        metadata.version = j.value("version", "");
                        metadata.git_commit = j.value("git_commit", "");
                        metadata.total_size_bytes = j.value("total_size_bytes", 0);
                        
                        auto ts = j.value("timestamp", 0L);
                        metadata.timestamp = std::chrono::system_clock::time_point(
                            std::chrono::seconds(ts));
                        
                        result.push_back(metadata);
                    } catch (...) {
                        // Skip invalid metadata files
                    }
                }
            }
        }
    } catch (...) {
        // Directory doesn't exist or other error
    }
    
    return result;
}

bool StateCheckpoint::deleteCheckpoint(const std::string& checkpoint_id) {
    std::lock_guard<std::mutex> lock(checkpoint_mutex_);
    
    std::string checkpoint_path = getCheckpointPath(checkpoint_id);
    std::string metadata_path = getMetadataPath(checkpoint_id);
    
    bool success = true;
    if (std::filesystem::exists(checkpoint_path)) {
        success &= std::filesystem::remove(checkpoint_path);
    }
    if (std::filesystem::exists(metadata_path)) {
        success &= std::filesystem::remove(metadata_path);
    }
    
    return success;
}

bool StateCheckpoint::validateCheckpoint(const std::string& checkpoint_id) const {
    std::string checkpoint_path = getCheckpointPath(checkpoint_id);
    std::string metadata_path = getMetadataPath(checkpoint_id);
    
    if (!std::filesystem::exists(checkpoint_path) || !std::filesystem::exists(metadata_path)) {
        return false;
    }
    
    std::string data;
    if (!readCheckpointFile(checkpoint_path, data)) return false;
    
    try {
        nlohmann::json::parse(data);
        return true;
    } catch (...) {
        return false;
    }
}

void StateCheckpoint::startPeriodicCheckpoint() {
    if (periodic_running_.load()) return;
    
    periodic_running_.store(true);
    shutdown_requested_.store(false);
    
    periodic_thread_ = std::thread([this]() { periodicLoop(); });
}

void StateCheckpoint::stopPeriodicCheckpoint() {
    if (!periodic_running_.load()) return;
    
    shutdown_requested_.store(true);
    periodic_running_.store(false);
    
    if (periodic_thread_.joinable()) {
        periodic_thread_.join();
    }
}

void StateCheckpoint::triggerCheckpoint(const std::string& reason) {
    saveCheckpoint(reason);
}

std::string StateCheckpoint::serializeMetadata() const {
    nlohmann::json j;
    j["checkpoint_directory"] = config_.checkpoint_directory;
    j["max_checkpoints"] = config_.max_checkpoints;
    j["compress"] = config_.compress;
    j["encrypt"] = config_.encrypt;
    return j.dump(2);
}

bool StateCheckpoint::deserializeMetadata(const std::string& data) {
    try {
        auto j = nlohmann::json::parse(data);
        config_.checkpoint_directory = j.value("checkpoint_directory", "data/checkpoints");
        config_.max_checkpoints = j.value("max_checkpoints", 100);
        config_.compress = j.value("compress", true);
        config_.encrypt = j.value("encrypt", false);
        std::filesystem::create_directories(config_.checkpoint_directory);
        return true;
    } catch (...) {
        return false;
    }
}

// const CheckpointConfig& StateCheckpoint::getConfig() const { return config_; }
// bool StateCheckpoint::isPeriodicRunning() const { return periodic_running_.load(); }

std::string StateCheckpoint::generateCheckpointId() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::ostringstream oss;
    oss << "ckpt_" << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S") << "_" << ms.count();
    return oss.str();
}

std::string StateCheckpoint::getCheckpointPath(const std::string& checkpoint_id) const {
    return config_.checkpoint_directory + "/" + checkpoint_id + ".json";
}

std::string StateCheckpoint::getMetadataPath(const std::string& checkpoint_id) const {
    return config_.checkpoint_directory + "/" + checkpoint_id + ".meta.json";
}

template<typename T>
std::string StateCheckpoint::serializeComponent(const T* component) const {
    if (!component) return "{}";
    return component->serialize();
}

template<typename T>
bool StateCheckpoint::deserializeComponent(T* component, const std::string& data) const {
    if (!component) return false;
    return component->deserialize(data);
}

bool StateCheckpoint::writeCheckpointFile(const std::string& path, const std::string& data) const {
    try {
        std::ofstream file(path);
        if (!file.is_open()) return false;
        file << data;
        return true;
    } catch (...) {
        return false;
    }
}

bool StateCheckpoint::readCheckpointFile(const std::string& path, std::string& data) const {
    try {
        std::ifstream file(path);
        if (!file.is_open()) return false;
        std::ostringstream oss;
        oss << file.rdbuf();
        data = oss.str();
        return true;
    } catch (...) {
        return false;
    }
}

bool StateCheckpoint::computeChecksum(const std::string& data, std::string& checksum) const {
    // Simple hash for now
    std::hash<std::string> hasher;
    checksum = std::to_string(hasher(data));
    return true;
}

void StateCheckpoint::periodicLoop() {
    while (!shutdown_requested_.load()) {
        std::this_thread::sleep_for(std::chrono::minutes(config_.periodic_interval.count()));
        if (!shutdown_requested_.load()) {
            saveCheckpoint("periodic");
        }
    }
}

bool StateCheckpoint::validateComponentSizes(const CheckpointMetadata& metadata) const {
    size_t total = 0;
    for (const auto& [name, size] : metadata.component_sizes) {
        total += size;
    }
    return total == metadata.total_size_bytes;
}

} // namespace infra
} // namespace brain
} // namespace yuki