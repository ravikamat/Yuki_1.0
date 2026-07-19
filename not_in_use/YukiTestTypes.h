#pragma once
#include <string>
#include <vector>

enum class YukiStage {
    CORE_FOUNDATION = 0,
    PRESENCE_SHELL = 1,
    REFINED_SHELL = 2,
    EXPANDED_DETAIL_VIEW = 3,
    AVATAR_BODY = 4,
    AVATAR_RENDERING = 5,
    VOICE_REACTIVE = 6,
    VISION_PRESENCE = 7,
    SUBSYSTEM_CONTROL = 8,
    PERCEPTION_UNIFICATION = 9
};

inline std::string yukiStageToString(YukiStage stage) {
    switch (stage) {
        case YukiStage::CORE_FOUNDATION: return "CORE_FOUNDATION";
        case YukiStage::PRESENCE_SHELL: return "PRESENCE_SHELL";
        case YukiStage::REFINED_SHELL: return "REFINED_SHELL";
        case YukiStage::EXPANDED_DETAIL_VIEW: return "EXPANDED_DETAIL_VIEW";
        case YukiStage::AVATAR_BODY: return "AVATAR_BODY";
        case YukiStage::AVATAR_RENDERING: return "AVATAR_RENDERING";
        case YukiStage::VOICE_REACTIVE: return "VOICE_REACTIVE";
        case YukiStage::VISION_PRESENCE: return "VISION_PRESENCE";
        case YukiStage::SUBSYSTEM_CONTROL: return "SUBSYSTEM_CONTROL";
        case YukiStage::PERCEPTION_UNIFICATION: return "PERCEPTION_UNIFICATION";
        default: return "UNKNOWN_STAGE";
    }
}

enum class TestCategory {
    POSITIVE,
    NEGATIVE,
    FAILURE_SAFETY,
    STATE_CONSISTENCY
};

inline std::string testCategoryToString(TestCategory cat) {
    switch (cat) {
        case TestCategory::POSITIVE: return "POSITIVE";
        case TestCategory::NEGATIVE: return "NEGATIVE";
        case TestCategory::FAILURE_SAFETY: return "FAILURE_SAFETY";
        case TestCategory::STATE_CONSISTENCY: return "STATE_CONSISTENCY";
        default: return "UNKNOWN_CATEGORY";
    }
}

enum class TestMode {
    MOCK_UNIT,
    INTEGRATION,
    FORCED_FAILURE
};

inline std::string testModeToString(TestMode mode) {
    switch (mode) {
        case TestMode::MOCK_UNIT: return "mock_unit";
        case TestMode::INTEGRATION: return "integration";
        case TestMode::FORCED_FAILURE: return "forced_failure";
        default: return "unknown_mode";
    }
}

struct TestResult {
    int id = 0;
    std::string name;
    YukiStage stage = YukiStage::CORE_FOUNDATION;
    TestCategory category = TestCategory::POSITIVE;
    TestMode testMode = TestMode::MOCK_UNIT;
    std::string expected;
    std::string actual;
    bool passed = false;       // Whether the assertions passed
    bool expected_pass = true; // Whether we expect the test to pass (true) or intentionally fail (false)
    std::string notes;
};
