#pragma once
#include <string>
#include <queue>
#include <mutex>

// Forward declarations to avoid circular includes
struct EmotionState;
class KnowledgeStore;

struct InternalQuestion {
    std::string text;
    float confidence = 1.0f;
};

class CuriosityEngine {
public:
    void tick(const EmotionState& emotion, const KnowledgeStore& store);
    bool hasPendingQuestion() const;
    InternalQuestion generateQuestion();
    
private:
    std::queue<InternalQuestion> pendingQuestions_;
    mutable std::mutex mutex_;
    int turnCount_ = 0;
};
