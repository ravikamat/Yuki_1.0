#include "CuriosityEngine.h"
#include <random>
#include <vector>

void CuriosityEngine::tick(const EmotionState& emotion, const KnowledgeStore& store) {
    ++turnCount_;
    if (turnCount_ < 5) return;
    // Curiosity threshold check would go here when EmotionState is fully wired
    // if (emotion.curiosity < 0.7f) return;
    
    // Placeholder: generate a curiosity-driven question every 10 turns
    if (turnCount_ % 10 != 0) return;
    
    InternalQuestion question;
    question.text = "I've been wondering about something. Can you tell me more?";
    question.confidence = 1.0f;
    
    std::lock_guard<std::mutex> lock(mutex_);
    pendingQuestions_.push(question);
}

bool CuriosityEngine::hasPendingQuestion() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return !pendingQuestions_.empty();
}

InternalQuestion CuriosityEngine::generateQuestion() {
    std::lock_guard<std::mutex> lock(mutex_);
    auto q = pendingQuestions_.front();
    pendingQuestions_.pop();
    return q;
}
