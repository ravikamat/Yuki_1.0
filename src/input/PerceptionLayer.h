#pragma once
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <functional>
#include <memory>

// 1. PerceptionSourceType: Legacy representational source
enum class PerceptionSourceType {
    TEXT,
    VOICE,
    CAMERA,
    SCREEN,
    INTERNAL
};

// 2. InputSourceKind: Authoritative new typed sources
enum class InputSourceKind {
    TYPED,            // terminal keyboard input
    VOICE_FINAL,      // finalized STT transcript
    VOICE_PARTIAL,    // partial/in-progress STT
    CAMERA,           // camera frame input
    SCREEN,           // screen capture input
    SYSTEM_INTERNAL,  // internal system event
    // Brain Specification additions (§ 3.1)
    TERMINAL_TEXT,    // explicit terminal distinction
    UI_TEXT,          // shell UI text input
    FILE_INPUT,       // file content input
    IMAGE_INPUT       // image/visual input
};

// 3. PerceptionEventCategory: Semantic category of event
enum class PerceptionEventCategory {
    COMMAND,
    USER_CHAT,
    SENSORY_OBSERVATION,
    SYSTEM_EVENT,
    STATUS_UPDATE,
    // Precise new categories
    USER_TEXT,
    USER_VOICE,
    BODY_STATE,
    SCREEN_CONTEXT,
    CAMERA_CONTEXT,
    EAR_STATUS,
    MOUTH_STATUS,
    COMMAND_RESULT
};

// String conversion helpers
std::string toString(PerceptionSourceType type);
std::string toString(InputSourceKind sourceKind);
std::string toString(PerceptionEventCategory category);
bool parseSourceType(const std::string& str, PerceptionSourceType& typeOut);
bool parseEventCategory(const std::string& str, PerceptionEventCategory& catOut);

// 4. PerceptionEvent: Autoritative perception packet representing normalized signals
struct PerceptionEvent {
    std::string id;
    uint64_t timestamp = 0;            // New camelCase timestamp
    unsigned long long created_at = 0; // Legacy created_at timestamp
    
    InputSourceKind sourceKind = InputSourceKind::TYPED;
    PerceptionSourceType source = PerceptionSourceType::TEXT; // Legacy compatibility
    
    PerceptionEventCategory category = PerceptionEventCategory::USER_CHAT;
    std::string subtype;
    
    std::string rawContent;
    std::string raw_content;           // Legacy compatibility
    
    std::string normalizedContent;
    std::string normalized_content;    // Legacy compatibility
    
    double confidence = 1.0;
    std::vector<std::string> tags;
    std::vector<std::string> tokens;
    
    std::string subsystemSnapshotSummary;
    std::string subsystem_snapshot;    // Legacy compatibility
    
    std::string turnId;
    std::map<std::string, std::string> metadata;
    
    // Semantic queries
    bool isCommand() const { 
        return category == PerceptionEventCategory::COMMAND || 
               category == PerceptionEventCategory::COMMAND_RESULT; 
    }
    bool isConversational() const { 
        return category == PerceptionEventCategory::USER_CHAT || 
               category == PerceptionEventCategory::USER_TEXT || 
               category == PerceptionEventCategory::USER_VOICE; 
    }
    bool isVisual() const { 
        return sourceKind == InputSourceKind::CAMERA || 
               sourceKind == InputSourceKind::SCREEN || 
               source == PerceptionSourceType::CAMERA || 
               source == PerceptionSourceType::SCREEN; 
    }
    bool isInternal() const { 
        return sourceKind == InputSourceKind::SYSTEM_INTERNAL || 
               source == PerceptionSourceType::INTERNAL; 
    }
    bool isVoiceText() const { 
        return (sourceKind == InputSourceKind::VOICE_FINAL || source == PerceptionSourceType::VOICE) && 
               (category == PerceptionEventCategory::COMMAND || 
                category == PerceptionEventCategory::USER_VOICE || 
                category == PerceptionEventCategory::USER_CHAT); 
    }
    bool isObservation() const { 
        return category == PerceptionEventCategory::SENSORY_OBSERVATION || 
               category == PerceptionEventCategory::SCREEN_CONTEXT || 
               category == PerceptionEventCategory::CAMERA_CONTEXT; 
    }
    bool isSystemEvent() const { 
        return category == PerceptionEventCategory::SYSTEM_EVENT; 
    }
};

// 5. UnifiedPerceptionLayer: Authoritative event bus for signal ingestion & dispatch
class UnifiedPerceptionLayer {
public:
    static UnifiedPerceptionLayer& instance();

    // Ingest event and dispatch to active listeners
    void submitEvent(const PerceptionEvent& event);

    // Bounded history retrieval
    std::vector<PerceptionEvent> getEventHistory();
    void clearHistory();
    size_t getHistorySize() const;
    void setHistoryLimit(size_t limit);
    size_t getHistoryLimit() const;

    // Listener subscription interface
    using EventListener = std::function<void(const PerceptionEvent&)>;
    void registerListener(const std::string& name, EventListener listener);
    void unregisterListener(const std::string& name);
    bool hasListener(const std::string& name) const;
    size_t listenerCount() const;

    // Factory Adapters
    static PerceptionEvent fromText(const std::string& text, bool isCommand, const std::string& snapshot);
    static PerceptionEvent fromVoiceText(const std::string& transcript, bool isCommand, double confidence, const std::string& snapshot);
    static PerceptionEvent fromVoiceObservation(const std::string& audioSummary, double confidence, const std::string& snapshot);
    static PerceptionEvent fromVoice(const std::string& audioSummary, double confidence, const std::string& snapshot); // Legacy compatibility alias
    static PerceptionEvent fromCamera(const std::string& summary, const std::map<std::string, std::string>& meta, const std::string& snapshot);
    static PerceptionEvent fromScreen(const std::string& summary, const std::map<std::string, std::string>& meta, const std::string& snapshot);
    static PerceptionEvent fromInternal(const std::string& eventName, const std::string& details, const std::string& snapshot);

private:
    UnifiedPerceptionLayer() = default;
    ~UnifiedPerceptionLayer() = default;
    UnifiedPerceptionLayer(const UnifiedPerceptionLayer&) = delete;
    UnifiedPerceptionLayer& operator=(const UnifiedPerceptionLayer&) = delete;

    // Dry event builder helpers
    static PerceptionEvent makeBaseEvent(InputSourceKind sourceKind, PerceptionSourceType source, PerceptionEventCategory category, double confidence, const std::string& snapshot);
    static std::vector<std::string> tokenizeWhitespace(const std::string& text);
    static double clampConfidence(double confidence);
    static void appendTagIfMissing(std::vector<std::string>& tags, const std::string& tag);
    std::string generateUniqueId();

    mutable std::mutex mutex_;
    std::vector<PerceptionEvent> history_;
    size_t historyLimit_ = 1000;
    std::map<std::string, EventListener> listeners_;
    unsigned long long eventCounter_ = 0;
};
