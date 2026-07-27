#pragma once
#include <cstddef>
#include <cstdint>

namespace yuki {
namespace config {

// ══════════════════════════════════════════════════════════════════════════════
// Action & Risk Thresholds
// ══════════════════════════════════════════════════════════════════════════════
inline constexpr float kActionRiskCutoff              = 0.50f;
inline constexpr float kResearchRiskCutoff            = 0.75f;
inline constexpr float kMinClassConfidence            = 0.30f;
inline constexpr float kHighConfidenceThreshold       = 0.80f;
inline constexpr float kMidConfidenceThreshold        = 0.50f;
inline constexpr float kLowConfidenceThreshold         = 0.30f;
inline constexpr float kSafetyRefusalThreshold        = 0.75f;
inline constexpr float kPythonExecutionRiskThreshold  = 0.70f;

// ══════════════════════════════════════════════════════════════════════════════
// Competence & Verification Thresholds
// ══════════════════════════════════════════════════════════════════════════════
inline constexpr float kExpertiseStrengthThreshold    = 0.70f;
inline constexpr float kExpertiseGapThreshold         = 0.40f;
inline constexpr float kLowExpertiseConfidence        = 0.30f;
inline constexpr float kNewUserInteractionLimit       = 5.0f;
inline constexpr float kVerificationConfidenceThreshold = 0.75f;
inline constexpr float kSatisfactionScoreThreshold    = 0.45f;
inline constexpr float kInputResolutionLowThreshold   = 0.40f;
inline constexpr float kInputResolutionMidThreshold   = 0.50f;
inline constexpr float kRetrievalConfidenceLow       = 0.35f;
inline constexpr float kRetrievalConfidenceHigh      = 0.65f;
inline constexpr float kAnswerConfidenceMin          = 0.65f;
inline constexpr float kTrustScoreThreshold           = 0.50f;
inline constexpr float kPlanNodeConfidenceLow        = 0.70f;
inline constexpr float kPlanNodeConfidenceHigh       = 0.35f;
inline constexpr float kComplexityMemoryOnly         = 0.40f;
inline constexpr float kComplexityNewSkill           = 0.50f;
inline constexpr float kNewSkillConfidenceMin        = 0.75f;
inline constexpr float kIntentFallbackThreshold      = 0.30f;

// ══════════════════════════════════════════════════════════════════════════════
// Capability Matcher Weights
// ══════════════════════════════════════════════════════════════════════════════
inline constexpr float kCapabilityWeightOutputOverlap = 0.50f;
inline constexpr float kCapabilityWeightPlatform      = 0.25f;
inline constexpr float kCapabilityWeightCompetence    = 0.25f;

// ══════════════════════════════════════════════════════════════════════════════
// Resource Optimizer Weights & Fallbacks
// ══════════════════════════════════════════════════════════════════════════════
inline constexpr float kResourceWeightDefault         = 0.50f;
inline constexpr float kResourceWeightCpuHigh         = 0.45f;
inline constexpr float kResourceWeightMonetary        = 0.30f;
inline constexpr float kResourceWeightRisk            = 0.25f;
inline constexpr float kResourceWeightTime            = 0.25f;

inline constexpr float kFallbackRamMb                 = 2048.0f;
inline constexpr float kFallbackCpuPercent            = 85.0f;
inline constexpr float kCpuHighThreshold              = 70.0f;

// ══════════════════════════════════════════════════════════════════════════════
// Observation Encoder & Tool Reliability Defaults
// ══════════════════════════════════════════════════════════════════════════════
inline constexpr float kObservationHighUniform        = 1.5f;
inline constexpr float kObservationLowUniform         = 0.8f;
inline constexpr float kObservationUniformThreshold   = 0.5f;

inline constexpr float kCompileToolReliability        = 0.90f;
inline constexpr float kCompileToolConfidence         = 0.90f;
inline constexpr float kFileCreateToolReliability     = 0.95f;
inline constexpr float kFileCreateToolConfidence      = 0.95f;
inline constexpr float kPopupUIToolReliability        = 0.99f;
inline constexpr float kIntegrationHealthWeight       = 0.75f;

// ══════════════════════════════════════════════════════════════════════════════
// System & Daemon Configuration
// ══════════════════════════════════════════════════════════════════════════════
inline constexpr int   kDaemonTickMs                  = 50;
inline constexpr int   kDormantPruneTicks             = 40;

inline constexpr float kMemoryExciteMinConfidence     = 0.30f;
inline constexpr float kMemoryExciteBase              = 0.50f;
inline constexpr float kPerceptExciteStrength         = 0.70f;

inline constexpr size_t kMaxActionGoals               = 50;
inline constexpr float kMinCompetenceThreshold        = 0.50f;

inline constexpr float kPathWeightTime                = 0.25f;
inline constexpr float kPathWeightResource            = 0.25f;
inline constexpr float kPathWeightRisk                = 0.25f;
inline constexpr float kPathMaxTotalRisk              = 0.75f;
inline constexpr float kPathMaxActionRisk             = 0.50f;
inline constexpr float kPathMaxRamMb                  = 2048.0f;
inline constexpr float kPathMaxCpuPercent             = 85.0f;

inline constexpr size_t kIoUringQueueDepth            = 4096;
inline constexpr size_t kEpollMaxEvents               = 64;
inline constexpr size_t kKqueueMaxEvents              = 64;

inline constexpr int   kScreenCaptureDelayMs          = 500;
inline constexpr int   kAutoSensorBackoffBaseMs       = 500;
inline constexpr int   kSensorInitDelayMs             = 120;
inline constexpr int   kSensorRetryDelayMs            = 500;

inline constexpr size_t kCacheLineSize                = 64;

} // namespace config
} // namespace yuki
