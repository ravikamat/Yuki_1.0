#pragma once
// ResponseEngine.h
// Yuki_1.0 — NeuralSpine Layer
//
// Contextual, personality-driven response generator.
// Takes IntentResult + WorldSnapshot + ConversationMemory and
// generates a natural, aware response string — no LLM needed.
//
// Personality traits: curious, warm, precise, occasionally wry.

#include "IntentScorer.h"
#include "brain/memory/ContextMemory.h"
#include <string>

class ResponseEngine {
public:
    // Generate a response string. All inputs are read-only.
    std::string generate(const IntentResult&       intent,
                         const WorldSnapshot&      world,
                         const ConversationMemory& memory,
                         const std::string&        rawInput) const;

private:
    // Per-intent response generators
    std::string respondGreeting(const WorldSnapshot& world,
                                const ConversationMemory& mem)  const;
    std::string respondFarewell(const WorldSnapshot& world)      const;
    std::string respondQuestionFactual(const std::string& raw,
                                       const IntentResult& intent,
                                       const WorldSnapshot& world) const;
    std::string respondSelfQuery(const WorldSnapshot& world,
                                 const ConversationMemory& mem)  const;
    std::string respondStatusQuery(const WorldSnapshot& world)   const;
    std::string respondAck(const ConversationMemory& mem)        const;
    std::string respondPositiveEmotion()                          const;
    std::string respondNegativeEmotion(const std::string& raw)   const;
    std::string respondScreenRef(const WorldSnapshot& world)     const;
    std::string respondBodyRef(const WorldSnapshot& world)       const;
    std::string respondGeneric(const std::string& raw,
                               const WorldSnapshot& world,
                               const ConversationMemory& mem)    const;

    // Personality helpers
    std::string pickFrom(std::initializer_list<const char*> options) const;
    std::string formatSystemHealth(const WorldSnapshot& world)         const;
    std::string formatAudioContext(const WorldSnapshot& world)         const;
    std::string embedScreenContext(const WorldSnapshot& world)         const;
};
