#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <memory>
#include <mutex>
#include "brain/inference/VariationalStateEstimator.h"
// #include "brain/autonomy/CompetenceTracker.h"  // Not yet implemented

namespace yuki {
namespace brain {
namespace autonomy {

struct CurriculumBuilderConfig {
    int max_resources_per_module = 5;
    int max_search_results = 20;
    float min_resource_quality = 0.4f;
    float max_hours_per_day = 2.0f;
    float practice_ratio = 0.6f;
    float mastery_threshold = 0.8f;
    int verification_attempts = 3;
    bool auto_generate_project = true;
    int project_complexity_levels = 3;
};

struct CurriculumSeed {
    std::string skill;
    std::string description;
    float priority = 1.0f;
    std::chrono::steady_clock::time_point planted;
};

class CurriculumBuilder {
public:
    explicit CurriculumBuilder(const CurriculumBuilderConfig& cfg = CurriculumBuilderConfig{});
    ~CurriculumBuilder() = default;
    
    void setConfig(const CurriculumBuilderConfig& cfg) { config_ = cfg; }
    
    void addSeed(const std::string& skill, const std::string& description);
    std::vector<CurriculumSeed> getSeeds() const;
    
    std::string serialize() const;
    bool deserialize(const std::string& data);

private:
    CurriculumBuilderConfig config_;
    std::vector<CurriculumSeed> seeds_;
    mutable std::mutex mutex_;
};

} // namespace autonomy
} // namespace brain
} // namespace yuki