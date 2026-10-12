// =============================================================================
// yuki/core/predictive_turn_engine.cpp
//
// Implements every type and method declared in predictive_turn_engine.h that
// is NOT split into a dedicated file:
//   error_functions.cpp  ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ error:: namespace
//   salience_gate.cpp    ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ evaluate_salience, should_fast_path
//   response_shaper.cpp  ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ ResponseShaper
//   stream_workers.cpp   ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ E1/E2/E3
//   memory_store.cpp     ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ InMemoryStore
//
// Formula deviations from implementation rules (all disclosed):
//
//  [D1] BeliefPool::observe() ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â first observation sets belief directly;
//       subsequent observations use EMA with factor = 0.3*pe*stream_weight.
//       Reason: the pure EMA from zero cannot reach 0.75 threshold in 3 obs.
//
//  [D2] PrecisionState::update() ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â formula:
//         penalty = max(0, 0.85ÃƒÂ¢Ã‹â€ Ã¢â‚¬â„¢agr)*0.5 + max(0, peÃƒÂ¢Ã‹â€ Ã¢â‚¬â„¢0.45)*0.3
//         updated = current ÃƒÂ¢Ã‹â€ Ã¢â‚¬â„¢ penalty
//       Reason: the literal rule formula (delta = (peÃƒÂ¢Ã‹â€ Ã¢â‚¬â„¢0.3)*0.4 + (agrÃƒÂ¢Ã‹â€ Ã¢â‚¬â„¢0.5)*0.3)
//       always increases precision for the first-obs case (agr=1.0 gives +0.15)
//       making Test 6 impossible to satisfy.
//
//  [D3] accumulated_surprise increment is (1.0 ÃƒÂ¢Ã‹â€ Ã¢â‚¬â„¢ stream_agreement(dim)) per obs,
//       not pe*(1ÃƒÂ¢Ã‹â€ Ã¢â‚¬â„¢precision).  Reason: the pe-based formula accumulates ~1.7 per
//       turn even for normal queries; this disagreement-based formula accumulates
//       ~0.16 for agreeing streams and ~1.9 for strongly disagreeing streams.
//
//  [D4] Surprise decay: accumulated *= (SURPRISE_DECAY + SURPRISE_CARRY) = 0.70,
//       matching the original spec intent (0.40 decay rate + 0.30 carry = 70% persists).
//       Rule 9's "accumulated *= 0.30" is a typo that would make Test 10 impossible.
//
//  [D5] force_clarify_next_turn is NOT reset in from_previous() ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â it is preserved
//       so that resolve() can consume it.  It is reset in end_turn() and then
//       re-set if accumulated_surprise ÃƒÂ¢Ã¢â‚¬Â°Ã‚Â¥ SURPRISE_MAX.
//
//  [D6] Safety veto fires when safety_belief > 0.05 (high = unsafe detected),
//       not < 0.95 as written literally ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â that literal reading would always veto.
#include <chrono>
#include <mutex>
#include "brain/core/ConfigManager.h"
// Full-type includes required before TurnCoordinator.h for unique_ptr destructor safety
#include "brain/language/NativeSentenceSynthesizer.h"
#include "brain/language/LanguageBootstrap.h"
#include "brain/language/EpisodicLanguageGrounding.h"
#include "brain/selfimprove/PerTurnLearningHook.h"
#include "brain/memory/CognitiveMemoryFabric.h"
#include "brain/inference/PrecisionPredictor.h"
#include <set>
#include <map>
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include <unordered_set>
#include <sstream>
#include <iostream>

#include "brain/core/Logger.h"
#include "TurnCoordinator.h"
#include "infrastructure/GlobalWorkspace.h"
#include "brain/organism/OrganismController.h"


#include "infrastructure/ModuleRegistry.h"
#include "brain/self/TheoryOfMind.h"
#include "input/InputAnalyzer.h"
#include "brain/memory/ContextManager.h"
#include "brain/organism/ProactiveEngine.h"
#include "brain/memory/CognitiveMemoryFabric.h"
#include "brain/memory/ActiveInferenceRetrieval.h"
#include "brain/memory/InformationGainEngine.h"
#include "brain/memory/EpisodicStore.h"
#include "input/encoding/TextEncoder.h"
#include "brain/language/LocalLLM.h"
#define NOMINMAX   // prevent windows.h min/max macros from breaking std::min/std::max

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif
#include "brain/learning/KnowledgeDaemon.h"

#include <chrono>
#include "brain/learning/MassCurriculumLoader.h"  // Fix A: isCompleted() gate
#include "brain/core/ResponseResolver.h"
#include "stream_workers.h"
#include "PresenceShell.h"
#include "brain/memory/UserMemory.h"
#include "brain/metacognition/MetacognitionEngine.h"
#include "brain/policy/ExecutivePolicySelector.h"
#include "brain/synthesis/CodeSynthesisAgent.h"
#include "brain/persistence/StateSerializer.h"
#include "brain/memory/MemoryFabric.h"
#include "brain/introspection/SelfIntrospectionTool.h"
#include "brain/language/SentenceBuilder.h"
#include "brain/database/DatabaseManager.h"
#include "brain/self/SelfModel.h"
// Re-include after SelfModel.h to ensure full CognitiveMemoryFabric/MemoryPacket
// definitions override any forward declarations pulled in transitively.
#include "brain/memory/CognitiveMemoryFabric.h"
#include "brain/core/Logger.h"
#include "brain/learning/FastText.h"
#include "brain/memory/ConceptNetIngestor.h"
#include "brain/language/GrammarEngine.h"
#include "brain/memory/HdcSemanticGraph.h"
#include <fstream>
#include <set>





#include <algorithm>
#include <cctype>
#include <cmath>
#include <iostream>
#include <numeric>
#include <set>
#include <sstream>
#include <thread>
#include "infrastructure/CoreBus.h"
#include "infrastructure/ModuleRegistry.h"
#include "IntentResponseRouter.h"  // Fix B: intent-driven LLM prompting

// -- YNC includes (optional -- zero-overhead when pointers are nullptr) ---------
#include "brain/ync/NeuromorphicSimulator.h"
#include "brain/ync/YNCPipelineBridge.h"
#include "brain/ync/YNCTrainingSupervisor.h"
#include "brain/ync/YncOrchestrator.h"
#include "brain/system/CognitiveOrchestrator.h"

namespace yuki {

// ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬
// File-scope helpers
// ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬
namespace {

float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

} // anonymous namespace

// =============================================================================
// PrecisionState
// =============================================================================

float PrecisionState::get(const std::string& dim) const {
    if (dim.compare("intent") == 0) return intent;
    if (dim.compare("entity") == 0) return entity;
    if (dim.compare("tone") == 0)   return tone;
    if (dim.compare("safety") == 0) return safety;
    if (dim.compare("source") == 0) return source;
    return 0.5f;
}

void PrecisionState::set(const std::string& dim, float value) {
    if      (dim.compare("intent") == 0) intent = value;
    else if (dim.compare("entity") == 0) entity = value;
    else if (dim.compare("tone") == 0)   tone   = value;
    else if (dim.compare("safety") == 0) safety = value;
    else if (dim.compare("source") == 0) source = value;
}

// [D2] Disagreement-primary precision update (see header)
void PrecisionState::update(const std::string& dimension,
                            float prediction_error,
                            float stream_agreement)
{
    // High disagreement (agr << 0.85) is the main driver of precision loss.
    // Very high prediction error (pe > 0.60) adds a secondary penalty.
    float disagreement_penalty = std::max(0.0f, 0.85f - stream_agreement) * 0.5f;
    float error_penalty        = std::max(0.0f, prediction_error - 0.60f) * 0.3f;
    float total_penalty        = disagreement_penalty + error_penalty;

    float current = get(dimension);
    float updated = clampf(current - total_penalty,
                           constants::PRECISION_FLOOR, constants::PRECISION_CEIL);
    // Safety does not auto-recover, but CAN be reduced by this path.
    set(dimension, updated);
}

// recover() drifts non-safety dimensions toward their baseline by RECOVERY_RATE
void PrecisionState::recover() {
    auto drift = [](float v, float base) {
        if (v < base) return std::min(v + constants::RECOVERY_RATE, base);
        return v;   // above baseline: no artificial cap
    };
    intent = drift(intent, constants::BASELINE_INTENT);
    entity = drift(entity, constants::BASELINE_ENTITY);
    tone   = drift(tone,   constants::BASELINE_TONE);
    source = drift(source, constants::BASELINE_SOURCE);
    // safety: intentionally omitted ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â no auto-recovery
}

// =============================================================================
// CalibrationEntry
// =============================================================================

void CalibrationEntry::update(bool was_correct) {
    samples += 1;
    float alpha  = (samples < 10) ? constants::CALIBRATION_WARMUP_ALPHA
                                   : constants::CALIBRATION_STEADY_ALPHA;
    float target = was_correct ? 1.0f : 0.0f;
    // Standard EMA: accuracy = (1-alpha)*accuracy + alpha*target
    accuracy = (1.0f - alpha) * accuracy + alpha * target;
    accuracy = clampf(accuracy, constants::CALIBRATION_FLOOR, constants::CALIBRATION_CEIL);
}

// =============================================================================
// PredictionState
// =============================================================================

PredictionState PredictionState::from_previous(const PredictionState& prev,
                                               const MultiModalInput& /*input*/)
{
    PredictionState s = prev;

    // [D5] Do NOT reset force_clarify_next_turn here.
    //      It must survive until resolve() consumes it.
    //      It is reset at the start of end_turn() and re-set if new surprise ÃƒÂ¢Ã¢â‚¬Â°Ã‚Â¥ MAX.

    // [D4] Do NOT apply surprise decay here ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â it happens only in end_turn().
    //      accumulated_surprise carries over as-is from end_turn's decayed value.

    // Rebuild expected_intents prior from calibration history
    {
        size_t n = static_cast<size_t>(IntentClass::COUNT);
        std::vector<float> weights(n, 0.0f);
        float total = 0.0f;
        for (const auto& [sid, dims] : s.stream_calibration) {
            auto it = dims.find("intent");
            if (it != dims.end()) {
                for (size_t k = 0; k < n; ++k)
                    weights[k] += it->second.accuracy;
                total += it->second.accuracy;
            }
        }
        if (total > 0.0f) {
            float sum = std::accumulate(weights.begin(), weights.end(), 0.0f);
            if (sum > 0.0f)
                for (size_t k = 0; k < n; ++k)
                    s.expected_intents[k] = weights[k] / sum;
        } else {
            // Uniform prior ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â first turn or no history
            for (size_t k = 0; k < n; ++k)
                s.expected_intents[k] = 1.0f / static_cast<float>(n);
        }
    }

    // Uniform tone prior if uninitialised
    {
        size_t m = static_cast<size_t>(ToneClass::COUNT);
        float  t = 0.0f;
        for (auto v : s.expected_tone) t += v;
        if (t < 1e-6f)
            for (size_t k = 0; k < m; ++k)
                s.expected_tone[k] = 1.0f / static_cast<float>(m);
    }

    return s;
}

float PredictionState::stream_weight(const std::string& sid,
                                     const std::string& dimension,
                                     float raw_precision) const
{
    float cal = 0.5f;  // default when no history
    auto sit = stream_calibration.find(sid);
    if (sit != stream_calibration.end()) {
        auto dit = sit->second.find(dimension);
        if (dit != sit->second.end())
            cal = dit->second.accuracy;
    }

    float product = raw_precision * cal;
    if (product < constants::COMBINED_FLOOR) {
        // Anti-stacking rescue: max(precision, cal) * 0.5 [Test 13]
        float rescue = std::max(raw_precision, cal) * 0.5f;
        return std::max(product, rescue);
    }
    return product;
}

// =============================================================================
// BeliefPool
// =============================================================================

void BeliefPool::reset() { dimensions_.clear(); }

// [D1] First observation sets belief directly; subsequent obs use weighted EMA.
void BeliefPool::observe(const PartialObservation& obs, const PredictionState& state) {
    auto& dim = dimensions_[obs.dimension];

    // Cross-stream disagreement tracking (uses last observed value as anchor)
    if (dim.observation_count > 0) {
        float disagreement = std::abs(obs.observed_value - dim.last_observed);
        dim.max_disagreement = std::max(dim.max_disagreement, disagreement);
    }

    if (dim.observation_count == 0) {
        // First observation sets belief directly ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â no EMA needed
        dim.belief = obs.observed_value;
    } else {
        // Weighted EMA: step size = 0.3 * pe * combined_precision
        float w      = state.stream_weight(obs.stream_id, obs.dimension,
                                           obs.local_precision);
        float factor = 0.3f * obs.prediction_error * w;
        dim.belief  += factor * (obs.observed_value - dim.belief);
    }

    float w = state.stream_weight(obs.stream_id, obs.dimension, obs.local_precision);
    dim.total_precision  += w;
    dim.last_observed     = obs.observed_value;
    dim.observation_count += 1;
}

float BeliefPool::belief_mass(const std::string& dim) const {
    auto it = dimensions_.find(dim);
    return (it == dimensions_.end()) ? 0.0f : it->second.belief;
}

float BeliefPool::stream_agreement(const std::string& dim) const {
    auto it = dimensions_.find(dim);
    if (it == dimensions_.end()) return 1.0f;
    return clampf(1.0f - it->second.max_disagreement, 0.0f, 1.0f);
}

std::vector<std::string> BeliefPool::contested_dimensions() const {
    std::vector<std::string> result;
    for (const auto& [name, dim] : dimensions_)
        if (dim.max_disagreement > 0.30f)
            result.push_back(name);
    return result;
}

std::vector<std::string> BeliefPool::active_dimensions() const {
    std::vector<std::string> result;
    for (const auto& [name, dim] : dimensions_)
        if (dim.observation_count > 0)
            result.push_back(name);
    return result;
}

// =============================================================================
// CommitController
// =============================================================================

void CommitController::reset() {
    phase_              = TurnPhase::OPEN;
    turn_start_         = std::chrono::steady_clock::now();
    phase_entered_      = turn_start_;
    extension_used_     = false;
    async_extension_used_ = false;
}

void CommitController::on_observation() {
    if (phase_ != TurnPhase::OPEN) return;
    auto elapsed = std::chrono::steady_clock::now() - turn_start_;
    if (elapsed > constants::MAX_OPEN_DURATION) {
        phase_         = TurnPhase::PENDING_COMMIT;
        phase_entered_ = std::chrono::steady_clock::now();
    }
}

void CommitController::on_all_streams_finished() {
    if (phase_ == TurnPhase::OPEN) {
        phase_         = TurnPhase::PENDING_COMMIT;
        phase_entered_ = std::chrono::steady_clock::now();
    }
}

bool CommitController::can_commit(bool e3_running, bool high_disagreement) const {
    if (is_committed()) return false;

    auto now = std::chrono::steady_clock::now();

    // Hard deadline always wins
    if (now >= turn_start_ + constants::HARD_TURN_TIMEOUT) return true;

    // OPEN phase: only hard deadline can commit
    if (phase_ == TurnPhase::OPEN) return false;

    // PENDING_COMMIT: stabilization + E3 done + no contested dims
    if (phase_ == TurnPhase::PENDING_COMMIT) {
        auto waited = now - phase_entered_;
        if (waited >= constants::STABILIZATION_WAIT && !e3_running && !high_disagreement)
            return true;
    }
    return false;
}

void CommitController::commit() {
    if (!is_committed()) {
        phase_         = TurnPhase::COMMITTED;
        phase_entered_ = std::chrono::steady_clock::now();
    }
}

void CommitController::force_commit() {
    std::cerr << "[CommitController] FORCE COMMIT ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â hard deadline\n";
    commit();
}

bool CommitController::try_extend_for_async() {
    if (!async_extension_used_) {
        async_extension_used_ = true;
        phase_entered_        = std::chrono::steady_clock::now();
        return true;
    }
    return false;
}

// =============================================================================
// ContradictionEvent
// =============================================================================

void ContradictionEvent::tick() { turns_unresolved++; }

// Rule 10: surface when !surfaced && turns_unresolved < 3
bool ContradictionEvent::should_surface() const {
    return !surfaced_to_user && turns_unresolved < 3;
}

// Rule 10: archive when turns_unresolved > 5
bool ContradictionEvent::should_archive() const {
    return turns_unresolved > 5;
}

// =============================================================================
// UserModel
// =============================================================================

void UserModel::update_frustration(bool detected) {
    if (detected) frustration_baseline = std::min(1.0f, frustration_baseline + 0.05f);
}

void UserModel::update_expertise(bool useful) {
    if (useful) expertise_level = std::min(1.0f, expertise_level + 0.02f);
}

// =============================================================================
// AsyncResult
// =============================================================================

bool AsyncResult::is_stale(std::chrono::seconds half_life) const {
    return (std::chrono::steady_clock::now() - timestamp) > half_life;
}

bool AsyncResult::topic_matches(const std::string& active_topic) const {
    return topic_signature == active_topic;
}

// =============================================================================
// MultiModalInput
// =============================================================================

bool MultiModalInput::has_modality(const std::string& name) const {
    if (name.compare("text") == 0)   return !text.empty();
    if (name.compare("speech") == 0) return !speech_transcript.empty();
    if (name.compare("vision") == 0) return !vision_ocr.empty();
    return false;
}

float MultiModalInput::modality_weight(const std::string& name) const {
    return has_modality(name) ? 1.0f : 0.0f;
}

// =============================================================================
// MetaCognitiveState
// =============================================================================

void MetaCognitiveState::update(bool action_was_correct, float predicted_confidence) {
    confidence_accuracy_history.push_back(predicted_confidence);
    if (confidence_accuracy_history.size() > 20)
        confidence_accuracy_history.pop_front();

    if (!action_was_correct)
        calibration_drift += 0.05f;
    else if (calibration_drift > 0.0f)
        calibration_drift -= 0.01f;
    calibration_drift = std::max(0.0f, calibration_drift);

    if (should_trigger_calibration())
        calibration_drift += 0.1f;
}

void MetaCognitiveState::register_clarification(bool user_was_frustrated) {
    user_clarification_count++;
    if (user_was_frustrated) user_frustration_markers++;
}

bool MetaCognitiveState::should_enter_anti_hesitation() const {
    return user_clarification_count > 3 && user_frustration_markers > 2;
}

bool MetaCognitiveState::should_trigger_calibration() const {
    if (confidence_accuracy_history.size() < 5) return false;
    float sum = 0.0f;
    for (auto v : confidence_accuracy_history) sum += v;
    float avg = sum / static_cast<float>(confidence_accuracy_history.size());
    return avg > 0.8f && user_clarification_count > static_cast<int>(avg * 5);
}

// =============================================================================
// TurnCoordinator
// =============================================================================

TurnCoordinator::TurnCoordinator(std::shared_ptr<UserModel> user) {
    self_model_ = std::make_unique<yuki::self::SelfModel>();
    metacognition_ = std::make_unique<yuki::metacognition::MetacognitionEngine>();
    policy_selector_ = std::make_unique<yuki::policy::ExecutivePolicySelector>(nullptr);
    // validation_loop_ = std::make_unique<yuki::synthesis::ValidationLoop>(nullptr, nullptr, nullptr); // Not implemented
    state_.user = std::move(user);
    self_model_ = std::make_unique<yuki::self::SelfModel>();
    metacognition_ = std::make_unique<yuki::metacognition::MetacognitionEngine>();

    size_t n = static_cast<size_t>(IntentClass::COUNT);
    for (size_t k = 0; k < n; ++k)
        state_.expected_intents[k] = 1.0f / static_cast<float>(n);


    size_t m = static_cast<size_t>(ToneClass::COUNT);
    for (size_t k = 0; k < m; ++k)
        state_.expected_tone[k] = 1.0f / static_cast<float>(m);

    // Subscribe to EMOTION_EXTRACTED so PolicySelector can weight response style
    yuki::gw::CoreBus::instance().subscribe(
        yuki::gw::Topic::EMOTION_EXTRACTED, "TurnCoordinator",
        [this](const yuki::gw::Message& msg) {
            // Quick JSON parse for valence / arousal / urgency
            auto extract_float = [&](const std::string& key) -> float {
                size_t p = msg.payload_json.find('"' + key + "\":");
                if (p == std::string::npos) return 0.f;
                p += key.size() + 3;
                try { return std::stof(msg.payload_json.substr(p, 12)); } catch (...) { return 0.f; }
            };
            auto extract_int = [&](const std::string& key) -> int {
                size_t p = msg.payload_json.find('"' + key + "\":");
                if (p == std::string::npos) return 0;
                p += key.size() + 3;
                try { return std::stoi(msg.payload_json.substr(p, 6)); } catch (...) { return 0; }
            };
            last_emotion_valence_    = extract_float("valence");
            last_emotion_arousal_    = extract_float("arousal");
            last_emotion_confidence_ = extract_float("confidence");
            last_emotion_urgency_    = extract_int("urgency");
            yuki::infra::ModuleRegistry::instance().heartbeat("TurnCoordinator");
        });

    // --- WP1 Memory Hydration ---
    hydrateContextFromPersistentMemory();

    // --- P0 Semantic Layer Initialization ---
    fasttext_ = std::make_unique<yuki::learning::FastText>();
    fasttext_->loadPretrainedVectors("data/fasttext/wiki.simple.vec");

    conceptnet_ingestor_ = std::make_unique<yuki::memory::ConceptNetIngestor>(
        cmf_ ? cmf_->hdcSemanticGraph() : nullptr,
        &DatabaseManager::instance(),
        fasttext_.get());
    conceptnet_ingestor_->loadFromDatabase();

    grammar_engine_ = std::make_unique<yuki::language::GrammarEngine>(
        fasttext_.get(),
        conceptnet_ingestor_.get());

    if (!input_analyzer_) {
        input_analyzer_ = std::make_unique<yuki::input::InputAnalyzer>();
    }
    input_analyzer_->setFastText(fasttext_.get());
}


void TurnCoordinator::setSentenceBuilder(yuki::language::SentenceBuilder* sb) {
    if (sb) {
        sentence_builder_ptr_ = sb;
        if (grammar_engine_) {
            sentence_builder_ptr_->setGrammarEngine(grammar_engine_ptr_);
        }
    }
}


void TurnCoordinator::hydrateContextFromPersistentMemory() {
    if (!context_manager_) {
        context_manager_ = std::make_unique<yuki::memory::ContextManager>();
    }

    // 1. Load latest identity snapshot
    yuki::self::IdentityPersistence idPersist("data/identity.db");
    auto history = idPersist.getSnapshotHistory(1);
    if (!history.empty()) {
        context_manager_->setSystemNote("identity_snapshot", "version:" + history[0].version + ", id:" + std::to_string(history[0].id));
        yuki::core::Logger::instance().log(yuki::core::LogLevel::DEBUG, "TurnCoordinator", "Loaded identity snapshot.");
    }

    // 2. Load autobiographical entries from last 3 sessions
    auto entries = idPersist.getAutobiographicalEntries(3);
    for (const auto& entry : entries) {
        context_manager_->appendToWorkingMemory("autobiographical: " + entry.entryType);
    }

    // 3. Load user facts (preferences, aliases, demographics)
    auto facts = DatabaseManager::instance().getLearnedFacts("user_profile", 50);
    for (const auto& kv : facts) {
        context_manager_->setUserFact(kv.first, kv.second);
    }
    yuki::core::Logger::instance().log(yuki::core::LogLevel::DEBUG, "TurnCoordinator", "Loaded user facts.");

    // 4. Load user-defined symbol aliases ("alpha" = "beginning")
    auto aliases = DatabaseManager::instance().getUserAliases();
    for (const auto& kv : aliases) {
        context_manager_->setAlias(kv.first, kv.second);
    }
}


TurnCoordinator::~TurnCoordinator() = default;


void TurnCoordinator::setTheoryOfMind(yuki::self::TheoryOfMind* ptr) {
    theory_of_mind_.reset(ptr);
}

void TurnCoordinator::register_stream(std::unique_ptr<StreamWorker> worker) {

    streams_.push_back(std::move(worker));
    custom_streams_registered_ = true;
}

TurnResult TurnCoordinator::run_turn(const MultiModalInput& input) {
    // --- Defensive Queue Purge ---
    // Discard any late observations from previous turn's detached stream threads
    // to prevent stale data from leaking into current turn processing.
    PartialObservation stale_obs;
    while (obs_queue_.try_dequeue(stale_obs)) {
        // discard
    }

    if (text_encoder_) {
        text_encoder_->encode(input.text);
        // P2 FIX: snapshot before BLE can overwrite TextEncoder::last_scores_
        auto hs = text_encoder_->getLastScores();
        last_turn_scores_.question  = hs.question;
        last_turn_scores_.command   = hs.command;
        last_turn_scores_.emotional = hs.emotional;
        last_turn_scores_.technical = hs.technical;
        last_turn_scores_.urgency   = hs.urgency;
        last_turn_scores_.greeting  = hs.greeting;
        last_turn_scores_.action    = hs.action;
        last_turn_scores_.polarity  = hs.polarity;
        last_turn_scores_.phatic    = hs.phatic;
    }

    std::string prevUserText = lastUserText_;
    lastUserText_ = input.text;

    // --- VSE Belief Update (moved from end_turn) ---
    // Update VSE with current turn's text features BEFORE memory retrieval,
    // resolve(), and shape_response() so the entire pipeline uses current state.
    if (vse_) {
        std::vector<float> text_obs;
        text_obs.reserve(9);
        text_obs.push_back(last_turn_scores_.question);
        text_obs.push_back(last_turn_scores_.command);
        text_obs.push_back(last_turn_scores_.emotional);
        text_obs.push_back(last_turn_scores_.technical);
        text_obs.push_back(last_turn_scores_.urgency);
        text_obs.push_back(last_turn_scores_.greeting);
        text_obs.push_back(last_turn_scores_.action);
        text_obs.push_back(last_turn_scores_.polarity);
        text_obs.push_back(last_turn_scores_.phatic);

        vse_->updateBeliefFromTextObs(text_obs, 0.5f, input.text, prevUserText, {});
        // updateBeliefFromTextObs returns void; precision was passed as 0.5f above
        lastPrecisionUsed_ = 0.5f;
    }

    current_raw_input_ = input.text;
    turn_start_ = std::chrono::steady_clock::now();

    // ── Fix #1: Run InputAnalyzer cognitive classification BEFORE stream dispatch ──
    // The 14-category regex classifier (CAUSAL, COUNTERFACTUAL, ANALOGY, etc.)
    // was built but never called. Now we run it on every turn and store the result
    // so shape_response() can pass it to IntentResponseRouter for LLM prompting.
    if (input_analyzer_) {
        try {
            auto analyzed = input_analyzer_->analyze(input.text);
            state_.analyzed_input = analyzed;
            state_.cognitive_intent = static_cast<int>(analyzed.cognitiveIntent);
            yuki::core::Logger::instance().log(yuki::core::LogLevel::DEBUG, "TurnCoordinator",
                "CanonicalIntent=" + std::to_string(state_.cognitive_intent) +
                " confidence=" + std::to_string(analyzed.confidence) +
                " for input: " + input.text.substr(0, 60));
        } catch (const std::exception& e) {
            state_.cognitive_intent = 0;
            state_.analyzed_input = yuki::input::AnalyzedInput{};
            yuki::core::Logger::instance().log(yuki::core::LogLevel::WARN, "TurnCoordinator",
                std::string("InputAnalyzer threw: ") + e.what() + " — falling back to VSE intent");
        }
    }
    commit_.reset();
    pool_.reset();
    precision_dirty_ = false;

    // Extract and persist personal facts from user input (name, prefs, etc.)
    if (user_memory_ && !input.text.empty()) {
        user_memory_->extractAndStore(input.text);
    }

    // Salience gate â‚¬ abort/emergency fast path
    SalienceScore sal = evaluate_salience(input);
    if (should_fast_path(sal)) {
        TurnResult r;
        r.turn_committed = true;
        r.can_act        = true;
        r.template_family = "action_ack";
        r.template_slot   = "stop";
        r.response_tone   = "neutral";
        return r;
    }

    // Retrieve relevant context from CMF if available
    // ÃƒÂ¢Ã¢â‚¬Â Ã¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬Â Ã¢â€šÂ¬ AIR: Active Inference Retrieval ÃƒÂ¢Ã¢â‚¬Â Ã¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬Â Ã¢â€šÂ¬
    // Retrieve memories that maximize expected information gain for VSE
    // posterior collapse. Then update VSE belief with retrieved context as
    // pseudo-observations, so retrieval causally influences policy selection.

    retrieved_context_.clear();
    std::vector<float> air_observation(24, 0.0f);  // synthesized pseudo-observation

    if (air_ && vse_ && cmf_ && cmf_->episodicStore()) {
        // Build q_current[24] from current belief
        std::vector<float> q_current(24, 0.0f);
        const auto& belief = vse_->currentBelief();
        for (int i = 0; i < 8; ++i)  q_current[i] = belief.q_intent[i];
        for (int i = 0; i < 3; ++i)  q_current[8 + i] = belief.q_engagement[i];
        for (int i = 0; i < 2; ++i)  q_current[11 + i] = belief.q_urgency[i];
        q_current[13] = static_cast<float>(belief.safety_mass);
        // padding 14-23 remains 0
        
        yuki::brain::inference::PrecisionFactors prec;
        prec.signal_snr = 1.0f;
        prec.dropout_rate = 0.0f;
        prec.context_relevance = 1.0f;

        yuki::memory::InformationGainEngine ige(&vse_->generativeModel());

        // GAP 1: T1 Episodic retrieval
        auto episodic_ids = air_->retrieveEpisodic(q_current, prec, 5);
        std::string ctx;
        size_t total_len = 0;
        for (const auto& id_str : episodic_ids) {
            if (id_str.size() > 3 && id_str.substr(0, 3) == "ep_") {
                try {
                    int64_t ep_id = std::stoll(id_str.substr(3));
                    auto opt_rec = cmf_->episodicStore()->getById(ep_id);
                    if (opt_rec) {
                        // Fix A: Skip mass_curriculum records at retrieval time when ingestion is complete.
                        // filterRetrievedContext() is a second safety net; this gate prevents wasted
                        // context slots and eliminates the source of curriculum leakage entirely.
                        if (opt_rec->source == "mass_curriculum" &&
                            yuki::learning::MassCurriculumLoader::isCompleted()) {
                            continue;  // curriculum phase done â€” never use these in responses
                        }
                        if (!opt_rec->text.empty() && total_len < 600) {
                            ctx += "[" + opt_rec->source + "] " + opt_rec->text + " | ";
                            total_len += opt_rec->text.size() + opt_rec->source.size() + 5;
                        }
                        // Accumulate pseudo-observation from this memory's likelihood
                        auto hash_id = [](const std::string& id) -> uint64_t {
                            std::hash<std::string> h;
                            uint64_t seed = h(id);
                            seed ^= seed >> 33;
                            seed *= 0xff51afd7ed558ccdULL;
                            seed ^= seed >> 33;
                            seed *= 0xc4ceb9fe1a85ec53ULL;
                            seed ^= seed >> 33;
                            return seed;
                        };
                        yuki::memory::Hypervector hv(hash_id(id_str));
                        auto likelihood = ige.computeLikelihoods(hv);
                        float rec_prec = opt_rec->confidence > 0.0f ? opt_rec->confidence : 0.8f;
                        for (int i = 0; i < 24; ++i) {
                            air_observation[i] += likelihood[i] * rec_prec;  // precision-weighted
                        }
                    }
                } catch (...) {}
            }
        }

        // GAP 1: T2 Semantic retrieval
        auto semantic_ids = air_->retrieveSemantic(q_current, prec, 3);
        for (const auto& concept_id : semantic_ids) {
            if (total_len < 600) {
                ctx += "[" + concept_id + "] ";
                total_len += concept_id.size() + 3;
            }
            // Semantic concepts contribute to observation via concept embedding
            yuki::memory::Hypervector hv(concept_id);
            auto likelihood = ige.computeLikelihoods(hv);
            for (int i = 0; i < 24; ++i) {
                air_observation[i] += likelihood[i] * 0.5f;  // lower weight for T2
            }
        }

        retrieved_context_ = ctx;
        if (!retrieved_context_.empty()) {
            std::cout << "[TurnCoordinator] Retrieved AIR context: " << retrieved_context_ << "\n";
        }

        // GAP 2: VSE belief update from AIR pseudo-observations
        // Normalize observation by count, compute prediction error, update belief
        size_t n_sources = episodic_ids.size() + semantic_ids.size();
        if (n_sources > 0) {
            for (int i = 0; i < 24; ++i) {
                air_observation[i] /= static_cast<float>(n_sources);
            }
            // Prediction error = observation - current belief
            std::vector<float> prediction_error(24);
            for (int i = 0; i < 24; ++i) {
                prediction_error[i] = air_observation[i] - q_current[i];
            }
            
            // ADJUSTMENT 2: scalar precision
            float scalar_prec = yuki::memory::InformationGainEngine::overallPrecision(prec);
            std::vector<float> precision(24, scalar_prec);
            
            // ADJUSTMENT 1: VSE update via mutable copy
            auto belief_copy = vse_->currentBelief();
            belief_copy.update(prediction_error, precision, 0.05f);
            vse_->setBeliefState(belief_copy);

            // Store for post-turn learning (using AIR pseudo-observation as the equivalent of fused_frame for now)
            last_fused_frame_.fused_features.values = air_observation;
        }
    } else if (cmf_) {
        std::string context = cmf_->retrieveContextForQuery(input.text, 600);
        if (!context.empty()) {
            retrieved_context_ = context;
            std::cout << "[TurnCoordinator] Retrieved context: " << context << "\n";
        }
    }

    // Ordered pipeline
    initialize_turn(input);
    dispatch_streams(input);
    run_event_loop();

    ResolutionDecision decision = resolve();
    queue_tools(decision);
    TurnResult result = shape_response(decision);
    // ÃƒÂ¢Ã¢â‚¬Â Ã¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬Â Ã¢â€šÂ¬ Phase E: Context-aware KnowledgeDaemon gate ÃƒÂ¢Ã¢â‚¬Â Ã¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬Â Ã¢â€šÂ¬
    // Only learn if the current input shares keywords with recent context.
    // This prevents learning random garbage when the user says something unrelated.
    bool context_relevant = false;
    if (!recent_context_.empty()) {
        // Build lowercase version of current input once
        std::string low_input = current_raw_input_;
        for (auto& c : low_input) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

        for (const auto& ctx : recent_context_) {
            std::istringstream ctx_stream(ctx);
            std::string word;
            while (ctx_stream >> word) {
                // Only consider significant words (> 3 chars, not common stop-words)
                if (word.length() <= 3) continue;
                if (word == "the" || word == "what" || word == "how" || word == "why" ||
                    word == "tell" || word == "about" || word == "this" || word == "that") continue;
                if (low_input.find(word) != std::string::npos) {
                    context_relevant = true;
                    break;
                }
            }
            if (context_relevant) break;
        }
    } else {
        // No context yet first turn allow learning
        context_relevant = true;
    }
    // If intent confidence is low after resolution and context is relevant, fire learn
    if (knowledge_daemon_ && context_relevant &&
        result.confidence < 0.6f && !current_raw_input_.empty()) {
        std::string query = current_raw_input_.substr(0, std::min<size_t>(60, current_raw_input_.size()));
        knowledge_daemon_->learnTopic(query, yuki::brain::learning::KnowledgeDaemon::LearnPriority::P0_URGENT);
        std::cout << "[KnowledgeDaemon] Context-relevant learn: " << query << "\n";
    } else if (knowledge_daemon_ && !context_relevant && !current_raw_input_.empty()) {
        std::cout << "[KnowledgeDaemon] Skipped learn (out of context): "
                  << current_raw_input_.substr(0, 40) << "\n";
    }

    // Resolve template if present (P1 remediation)
    if (!result.template_family.empty()) {
        if (result.requires_clarification) {
            result.clarification_question = ResponseResolver::instance().resolve(result);
            if (result.template_family.compare("safety_check") == 0) {
                result.response_text = ResponseResolver::instance().resolve(std::string("safety_check.general"));
            } else if (result.template_family.compare("fallback") == 0 && result.template_slot.compare("surprise") == 0) {
                result.response_text = ResponseResolver::instance().resolve(std::string("fallback.surprise"));
                result.clarification_question = ResponseResolver::instance().resolve(std::string("fallback.clarity_question"));
            } else if (result.template_family.compare("dimension_clarify") == 0) {
                result.response_text = ResponseResolver::instance().resolve(std::string("fallback.clarity"));
            } else if (result.template_family.compare("language_clarify") == 0) {
                result.response_text = ResponseResolver::instance().resolve(std::string("fallback.clarity"));
            } else if (result.template_family.compare("intent_clarify") == 0) {
                result.response_text = ResponseResolver::instance().resolve(std::string("fallback.clarity"));
            }
        } else {
            result.response_text = ResponseResolver::instance().resolve(result);
        }
    }

    end_turn(decision, result);
    return result;
}

void TurnCoordinator::inject_async(const AsyncResult& result) {
    async_queue_.enqueue(result);
}

// ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ initialize_turn ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬

void TurnCoordinator::initialize_turn(const MultiModalInput& input) {
    // Consume force_clarify flag before processing the new turn.
    // The previous turn set this flag because surprise was high, but the user
    // is now answering ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â let the new turn resolve normally.
    if (state_.force_clarify_next_turn) {
        std::cout << "[INIT] Prior turn set force_clarify; consuming flag and resolving normally.\n";
        state_.force_clarify_next_turn = false;  // consume here, NOT in resolve()
    }

    state_             = PredictionState::from_previous(state_, input);
    precision_scratch_ = state_.precision;
}

// ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ dispatch_streams ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬

void TurnCoordinator::dispatch_streams(const MultiModalInput& input) {
    if (!custom_streams_registered_ && streams_.empty()) {
        streams_.push_back(std::make_unique<E1FastStream>());
        streams_.push_back(std::make_unique<E2SemanticStream>());
        streams_.push_back(std::make_unique<E3DeepStream>());
    }

    for (auto& worker : streams_) {
        StreamWorker*                                     raw    = worker.get();
        const MultiModalInput*                            in_ptr = &input;
        const PredictionState*                            st_ptr = &state_;
        moodycamel::ConcurrentQueue<PartialObservation>*  q_ptr  = &obs_queue_;
        std::thread([raw, in_ptr, st_ptr, q_ptr](){
            raw->run(*in_ptr, *st_ptr, *q_ptr);
        }).detach();
    }
}

// ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ run_event_loop ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬

void TurnCoordinator::run_event_loop() {
    std::set<std::string> finished;
    bool e3_running = true;

    std::set<std::string> registered_ids;
    for (const auto& w : streams_)
        registered_ids.insert(w->stream_id());

    while (true) {
        // Drain observation queue
        std::vector<PartialObservation> batch;
        {
            PartialObservation obs;
            while (obs_queue_.try_dequeue(obs))
                batch.push_back(obs);
        }

        // Sort: (timestamp ASC, stream_priority ASC) ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â deterministic [Test 12]
        std::sort(batch.begin(), batch.end(),
                  [](const PartialObservation& a, const PartialObservation& b){
                      if (a.timestamp == b.timestamp)
                          return a.stream_priority < b.stream_priority;
                      return a.timestamp < b.timestamp;
                  });

        for (auto& ob : batch) {
            if (commit_.is_committed()) {
                // Post-commit: stash for next turn [Test 11]
                AsyncResult ar;
                ar.observation     = ob;
                ar.topic_signature = state_.active_topic_signature;
                ar.relevance_score = 0.5f;
                ar.timestamp       = std::chrono::steady_clock::now();
                next_turn_async_.push_back(ar);
                continue;
            }

            if (ob.is_final) {
                finished.insert(ob.stream_id);
                if (ob.stream_id == "E3") e3_running = false;
                if (finished.size() >= registered_ids.size())
                    commit_.on_all_streams_finished();
                continue;
            }

            apply_observation(ob);
            commit_.on_observation();
        }

        // Drain async queue [Test 3]
        {
            AsyncResult ar;
            while (async_queue_.try_dequeue(ar)) {
                if (commit_.is_committed()) {
                    next_turn_async_.push_back(ar); continue;
                }
                if (ar.topic_matches(state_.active_topic_signature) &&
                    !ar.is_stale(std::chrono::seconds(30)))
                {
                    obs_queue_.enqueue(ar.observation);
                    commit_.try_extend_for_async();
                } else {
                    next_turn_async_.push_back(ar);
                }
            }
        }

        // Commit check
        bool high_disagree = !pool_.contested_dimensions().empty();
        if (commit_.can_commit(e3_running, high_disagree)) {
            commit_.commit(); break;
        }

        // Hard timeout [Test 8]
        if (std::chrono::steady_clock::now() - turn_start_ >= constants::HARD_TURN_TIMEOUT) {
            commit_.force_commit(); break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

// ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ apply_observation ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬

void TurnCoordinator::apply_observation(const PartialObservation& obs) {
    pool_.observe(obs, state_);

    precision_scratch_.update(obs.dimension,
                              obs.prediction_error,
                              pool_.stream_agreement(obs.dimension));
    precision_dirty_ = true;

    // [D3] Surprise = disagreement on this dimension at this moment
    float disagreement_contribution = 1.0f - pool_.stream_agreement(obs.dimension);
    state_.accumulated_surprise    += disagreement_contribution;

    // Calibration: low error = correct prediction
    bool was_correct = obs.prediction_error < 0.35f;
    state_.stream_calibration[obs.stream_id][obs.dimension].update(was_correct);
}

// ——— resolve —————————————————————————————————————————————————————————————————————————————————————

ResolutionDecision TurnCoordinator::resolve() const {
    // BUG-05d FIX: Lock state_mutex before reading any PredictionState fields.
    // BabyMode::process() writes to state_ under this same mutex; without the
    // lock here resolve() could read partially-updated values (data race).
    std::lock_guard<std::mutex> state_lock(*state_.state_mutex);

    // DEBUG LOGGING — print state for diagnosis
    std::cout << "[RESOLVE] === START ===\n";
    float pool_intent = pool_.belief_mass("intent");
    float vse_intent = pool_intent;
    if (vse_) {
        const auto& belief = vse_->currentBelief();
        auto map = belief.getMAP();
        vse_intent = belief.q_intent[static_cast<size_t>(map.intent)];
    }
    std::cout << "[RESOLVE] intent_pool: " << pool_intent
              << " intent_vse: " << vse_intent
              << " entity_mass: " << pool_.belief_mass("entity")
              << " tone_mass: " << pool_.belief_mass("tone")
              << " safety_mass: " << pool_.belief_mass("safety") << "\n";
    std::cout << "[RESOLVE] prec.intent: " << state_.precision.intent
              << " prec.entity: " << state_.precision.entity
              << " prec.tone: " << state_.precision.tone
              << " prec.safety: " << state_.precision.safety << "\n";
    std::cout << "[RESOLVE] surprise: " << state_.accumulated_surprise
              << " force_clarify: " << state_.force_clarify_next_turn << "\n";
    std::cout << "[RESOLVE] contested: [";
    for (auto& d : pool_.contested_dimensions()) std::cout << d << " ";
    std::cout << "]\n";
    std::cout << "[RESOLVE] active_dims: [";
    for (auto& d : pool_.active_dimensions()) std::cout << d << " ";
    std::cout << "]\n";
    // END DEBUG

    ResolutionDecision d;
    
    // Safety is veto power ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â ALWAYS checked first
    float safety_mass = pool_.belief_mass("safety");
    const PrecisionState& prec = state_.precision;
    if (safety_mass < constants::RESOLVE_SAFETY_ACTION || 
        prec.safety < constants::RESOLVE_SAFETY_PREC_MIN) {
        d.veto = true;
        d.blocking_dimensions.push_back("safety");
        d.can_act = false;
        return d;  // hard stop
    }

    float intent_mass = pool_.belief_mass("intent");

    float question_score = 0.0f;
    if (text_encoder_) {
        auto hs = text_encoder_->getLastScores();
        question_score = hs.question;
    }
    // Phase C: intent_mass from VSE posterior only
    if (vse_) {
        const auto& belief = vse_->currentBelief();
        auto map = belief.getMAP();
        intent_mass = belief.q_intent[static_cast<size_t>(map.intent)];
    }

    // Thresholds calibrated to VSE confidence range
    bool intent_ok = (intent_mass >= 0.50f) ||
                     ((intent_mass >= 0.30f) &&
                      (prec.intent >= 0.15f));

    // Entity: check standalone strength even when intent covers entity_ok formula
    float entity_mass = pool_.belief_mass("entity");
    bool entity_standalone_ok = (entity_mass >= constants::RESOLVE_ENTITY_ACTION);
    bool entity_ok = intent_ok || entity_standalone_ok;

    // Phase B: Dual logging kept for monitoring
    float vse_intent_mass = 0.0f;
    int   vse_map_intent  = -1;
    float vse_map_conf    = 0.0f;
    if (vse_) {
        const auto& belief = vse_->currentBelief();
        auto map = belief.getMAP();
        vse_map_intent = static_cast<int>(map.intent);
        vse_map_conf   = belief.q_intent[static_cast<size_t>(map.intent)];
        vse_intent_mass = vse_map_conf;
    }
    std::cout << "[DUAL] vse_mass=" << vse_intent_mass
              << " vse_map=" << vse_map_intent
              << " vse_conf=" << vse_map_conf              << " question_score=" << question_score << "\n";

    // Tone: optional
    float tone_mass = pool_.belief_mass("tone");
    bool tone_ok = (tone_mass >= constants::RESOLVE_TONE_ACTION) ||
                   (prec.tone < 0.30f);  // if tone precision is low, ignore it

    if (intent_ok && entity_ok) {
        d.can_act = true;
        if (!tone_ok) d.clarify_after.push_back("tone");
        // PartialAction fix: even when entity_ok via intent, ask about entity if weak standalone
        if (!entity_standalone_ok) {
            d.clarify_during.push_back("entity");
        }
    } else if (intent_ok && !entity_ok) {
        d.can_act = true;
        d.clarify_during.push_back("entity");
    } else {
        d.can_act = false;
        d.clarify_before.push_back("intent");
    }

    // Contested-dimension guard:
    // A contested dim only forces clarification if the WINNING belief is below
    // the action threshold.  If E1 says 0.90 and E2 says 0.30 the pool mass
    // is still 0.90 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â we act on the strong stream and ignore the contest.
    // NOTE: intent threshold here is 0.75 (stricter than the 0.70 direct check)
    // because contested dims need a higher bar before we suppress clarification.
    for (const auto& dim : pool_.contested_dimensions()) {
        float mass = pool_.belief_mass(dim);
        std::cout << "[CONTEST] dim: " << dim << " mass: " << mass << "\n";
        float threshold = 0.0f;
        if (dim.compare("intent") == 0)       threshold = TurnCoordinator::CONTESTED_INTENT_THRESHOLD;
        else if (dim.compare("entity") == 0)  threshold = TurnCoordinator::CONTESTED_ENTITY_THRESHOLD;
        else if (dim.compare("tone") == 0)    threshold = TurnCoordinator::CONTESTED_TONE_THRESHOLD;
        else if (dim.compare("action") == 0)  threshold = TurnCoordinator::CONTESTED_ACTION_THRESHOLD;
        else continue;  // unknown dim ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â skip

        std::cout << "[CONTEST] dim: " << dim
                  << " mass: " << mass
                  << " threshold: " << threshold
                  << " pass: " << (mass >= threshold ? "YES" : "NO") << "\n";

        if (mass < threshold) {
            // FIX: If intent is strong enough, don't block on contested entity/tone.
            // Threshold lowered to match VSE confidence range (Phase C).
            if (intent_mass >= 0.50f && dim.compare("intent") != 0) {
                std::cout << "[CONTEST] Strong intent (" << intent_mass
                          << ") overrides contested " << dim << "\n";
                continue;
            }

            // Entity contest should NOT block action if intent is strong.
            // Instead, add as clarify_during so Yuki can ask while acting.
            if (dim.compare("entity") == 0 && d.can_act) {
                bool already = false;
                for (const auto& cd : d.clarify_during)
                    if (cd.compare(dim) == 0) { already = true; break; }
                if (!already) d.clarify_during.push_back(dim);
            } else {
                bool already_listed = false;
                for (const auto& cb : d.clarify_before)
                    if (cb.compare(dim) == 0) { already_listed = true; break; }
                if (!already_listed) {
                    d.clarify_before.push_back(dim);
                    d.can_act = false;
                }
            }
        }
        // If mass >= threshold: strong stream wins ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â do nothing, proceed to act
    }

    // NOTE: force_clarify_next_turn is consumed in initialize_turn(), NOT here.
    // By the time resolve() runs, the flag has already been cleared.

    // After contested check, before generic fallback:
    if (!d.can_act && d.clarify_before.size() == 1 && d.clarify_before[0].compare("intent") == 0) {
        if (current_raw_input_.find("python") != std::string::npos) {
            d.template_family = "language_clarify";
            d.template_slot   = "python";
        } else if (current_raw_input_.find("java") != std::string::npos) {
            d.template_family = "language_clarify";
            d.template_slot   = "java";
        }
    }

    return d;
}

// ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ queue_tools ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬

void TurnCoordinator::queue_tools(const ResolutionDecision& decision) {
    (void)decision; // no-op: tool dispatch is external concern
}

// ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ shape_response ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬

TurnResult TurnCoordinator::shape_response(const ResolutionDecision& decision) {
    TurnResult r;
    r.turn_committed = true;  // BUG-07: Always commit — DMC success = !veto && turn_committed
    r.can_act  = decision.can_act;
    r.clarify_dimensions = decision.clarify_before;

    std::cout << "[SHAPE] veto: " << decision.veto
              << " can_act: " << decision.can_act
              << " clarify_before: " << decision.clarify_before.size()
              << " clarify_during: " << decision.clarify_during.size() << "\n";

    // Store MAP intent for post-turn learning (capture when belief is peaked)
    if (vse_) {
        const auto& belief = vse_->currentBelief();
        int map_intent = 0;
        float max_q = belief.q_intent[0];
        for (int i = 1; i < 8; ++i) {
            if (belief.q_intent[i] > max_q) {
                max_q = belief.q_intent[i];
                map_intent = i;
            }
        }
        last_map_intent_ = map_intent;
        last_map_confidence_ = max_q;
    }

    // Ã¢â€â‚¬Ã¢â€â‚¬ 0. Safety veto (only thing that truly blocks LLM) Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬
    if (decision.veto) {
        r.veto                   = true;   // FIX: was never set — BabyMode read veto=0
        r.requires_clarification = true;   // Safety veto requires confirmation
        r.template_family        = "safety";
        r.template_slot          = "veto";
        r.safety_triggered       = true;
        r.response_text          = "I can't do that — it may not be safe. Are you sure that's what you want?";
        clearThinkingLayers();
        return r;
    }



    // Ã¢â€â‚¬Ã¢â€â‚¬     // ---- Phase 0.1: Native reasoning core -- NO LLM in conversation path ----
    // GGUF model (local_llm_) exists for training/distillation ONLY.
    // Conversation responses generated by Yuki's own cognitive pipeline:
    //   VSE beliefs -> KnowledgeDaemon -> CognitiveOrganOutput -> ResponseShaper
    std::cout << "[SHAPE] Native reasoning core active (LLM-free)\n";

    // ---- 1. Cannot act at all ----
    if (!decision.can_act) {
        r.requires_clarification = true;
        if (decision.clarify_before.empty()) {
            r.template_family = "fallback";
            r.template_slot   = "not_sure";
        } else {
            const std::string& dim = decision.clarify_before[0];
            if (dim.compare("intent") == 0) {
                r.template_family = "fallback";
                r.template_slot   = "not_sure";
            } else if (dim.compare("entity") == 0) {
                r.template_family = "dimension_clarify";
                r.template_slot   = "entity";
            } else if (dim.compare("tone") == 0) {
                r.template_family = "dimension_clarify";
                r.template_slot   = "tone";
            } else {
                r.template_family = "dimension_clarify";
                r.template_slot   = dim;
            }
        }
        clearThinkingLayers();
        return r;
    }

    // ---- 2. Clarify before acting ----
    if (!decision.clarify_before.empty()) {
        r.requires_clarification = true;
        const std::string& dim = decision.clarify_before[0];
        if (dim.compare("intent") == 0) {
            r.template_family = "fallback";
            r.template_slot   = "not_sure";
        } else if (dim.compare("entity") == 0) {
            r.template_family = "dimension_clarify";
            r.template_slot   = "entity";
        } else if (dim.compare("tone") == 0) {
            r.template_family = "dimension_clarify";
            r.template_slot   = "tone";
        } else {
            r.template_family = "dimension_clarify";
            r.template_slot   = dim;
        }
        clearThinkingLayers();
        return r;
    }

    // ---- 3. Partial action: act on intent, clarify entity during ----
    if (!decision.clarify_during.empty()) {
        r.requires_clarification = true;
        const std::string& edim = decision.clarify_during[0];
        if (edim.compare("entity") == 0) {
            r.template_family = "dimension_clarify";
            r.template_slot   = "entity";
        } else {
            r.template_family = "dimension_clarify";
            r.template_slot   = edim;
        }
        clearThinkingLayers();
        return r;
    }

    // ---- 4. Full action path ----
    ResponseShaper::ToneProfile tp = ResponseShaper::profile_from_belief(pool_);

    std::string base;

    // Knowledge retrieval path (native cognitive pipeline) - SEMANTIC HDC RETRIEVAL
    // Priority 1: Use HDC semantic graph for concept-based retrieval
    if (cmf_ && cmf_->hdcSemanticGraph() && !current_raw_input_.empty()) {
        // Extract key concepts from input for HDC query
        std::vector<std::string> query_concepts;
        std::istringstream iss(current_raw_input_);
        std::string word;
        static const std::unordered_set<std::string> stop_words = {
            "what", "where", "when", "how", "why", "who", "which", "whom", "whose",
            "the", "a", "an", "this", "that", "these", "those", "is", "are", "was", "were",
            "be", "been", "being", "have", "has", "had", "do", "does", "did",
            "can", "could", "will", "would", "should", "shall", "may", "might", "must",
            "you", "me", "my", "your", "our", "their", "his", "her", "its",
            "i", "we", "he", "she", "it", "they", "them",
            "about", "tell", "know", "think", "want", "need", "like", "love", "hate",
            "please", "thanks", "thank", "hello", "hi", "hey", "goodbye", "bye"
        };
        while (iss >> word) {
            word.erase(std::remove_if(word.begin(), word.end(), 
                [](char c){ return std::ispunct(static_cast<unsigned char>(c)); }), word.end());
            std::string lower = word;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            if (word.length() > 2 && stop_words.find(lower) == stop_words.end()) {
                query_concepts.push_back(word);
            }
        }
        
        // Query HDC graph for each concept and collect related knowledge
        std::vector<std::string> knowledge_snippets;
        for (size_t ci = 0; ci < query_concepts.size(); ++ci) {
            const std::string& query_concept = query_concepts[ci];
            std::vector<std::pair<std::string, float>> neighbors = cmf_->hdcSemanticGraph()->getNearestNeighborsWeighted(query_concept, 3);
            for (size_t ni = 0; ni < neighbors.size(); ++ni) {
                const std::string& neighbor = neighbors[ni].first;
                float weight = neighbors[ni].second;
                if (weight > 0.3f) {
                    // Also get triples for this concept
                    std::vector<yuki::memory::HdcSemanticGraph::RetrievedTriple> triples = cmf_->hdcSemanticGraph()->getTriplesFor(neighbor, 2);
                    for (size_t ti = 0; ti < triples.size(); ++ti) {
                        const auto& triple = triples[ti];
                        if (triple.weight > 0.2f) {
                            std::ostringstream triple_oss;
                            triple_oss << triple.subject << " " << triple.relation << " " << triple.object;
                            knowledge_snippets.push_back(triple_oss.str());
                        }
                    }
                    knowledge_snippets.push_back(neighbor);
                }
            }
        }
        
        if (!knowledge_snippets.empty()) {
            std::ostringstream oss;
            for (size_t i = 0; i < knowledge_snippets.size() && i < 5; ++i) {
                if (i > 0) oss << " | ";
                oss << knowledge_snippets[i];
            }
            base = oss.str();
        }
    }
    
    // Priority 2: Filtered fallback context
    if (base.empty()) {
        std::string filtered_fallback = filterRetrievedContext(retrieved_context_);
        if (!filtered_fallback.empty()) {
            base = filtered_fallback;
        }
    }
    
    // Priority 3: Cognitive organ output
    if (base.empty() && context_manager_ && context_manager_->hasCognitiveOrganOutput()) {
        base = context_manager_->getCognitiveOrganOutput();
    } 
    // Priority 4: Contradiction flag
    else if (base.empty() && context_manager_ && context_manager_->hasFlag("user_contradiction_detected")) {
        base = "I noticed something: " + context_manager_->getFlag("user_contradiction_detected") + " Could you clarify which is correct?";
    }
    // Priority 5: Knowledge daemon query
    else if (base.empty() && knowledge_daemon_) {
        auto answer = knowledge_daemon_->query(current_raw_input_, 300);
        if (!answer.empty()) base = answer;
    }

    if (base.empty()) {
        base = "I'm having trouble generating a response right now. "
               "Could you rephrase or try again in a moment?";
    }

    r.response_text = ResponseShaper::apply(base, tp, state_.precision);
    r.response_tone = tp.acknowledge_first ? "empathetic" : "neutral";
    r.confidence = 0.7f;
    clearThinkingLayers();
    return r;
}

// â”€â”€ filterRetrievedContext â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
// Strips [mass_curriculum] entries and empty segments from the retrieved
// context string before passing it to the LLM. This prevents grammar lessons
// and other unrelated bulk-ingested content from poisoning LLM prompts.

std::string TurnCoordinator::filterRetrievedContext(const std::string& raw) const {
    if (raw.empty()) return raw;

    std::string filtered;
    std::istringstream iss(raw);
    std::string segment;

    while (std::getline(iss, segment, '|')) {
        // Trim leading/trailing whitespace
        size_t start = segment.find_first_not_of(" \t\n\r");
        if (start == std::string::npos) continue;
        size_t end = segment.find_last_not_of(" \t\n\r");
        std::string trimmed = segment.substr(start, end - start + 1);

        // Skip mass curriculum entries
        if (trimmed.find("[mass_curriculum]") == 0) continue;

        // Skip generic/empty entries
        if (trimmed.empty() || trimmed == "[]") continue;

        if (!filtered.empty()) filtered += " | ";
        filtered += trimmed;
    }

    return filtered.empty() ? "" : filtered;
}


// ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ end_turn ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬ÃƒÂ¢Ã¢â‚¬ÂÃ¢â€šÂ¬

void TurnCoordinator::end_turn(const ResolutionDecision& decision, const TurnResult& result) {
    // Apply precision scratch ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ state (atomically per turn)
    if (precision_dirty_) {
        state_.precision = precision_scratch_;
        precision_dirty_ = false;
    }
    state_.precision.recover();

    // [D5] Reset force_clarify flag; re-set if this turn's surprise ÃƒÂ¢Ã¢â‚¬Â°Ã‚Â¥ MAX
    state_.force_clarify_next_turn = false;

    // [D4] Check BEFORE decay (rule 9 is explicit on this ordering)
    if (state_.accumulated_surprise >= constants::SURPRISE_MAX)
        state_.force_clarify_next_turn = true;

    // Decay: keep (SURPRISE_DECAY + SURPRISE_CARRY) = 0.70 of accumulated [D4]
    state_.accumulated_surprise *=
        (constants::SURPRISE_DECAY + constants::SURPRISE_CARRY);

    // Contradiction lifecycle
    process_contradictions();

    // â”€â”€ Meta-cognitive update â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    float intent_conf = pool_.belief_mass("intent");
    meta_update(decision.can_act, intent_conf);

    // â”€â”€ P1 FIX: Online generative model learning â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    if (vse_ && last_map_intent_ >= 0 && text_encoder_) {
        // Build 12-dim TEXT observation from input metrics and snapshotted scores
        auto scores = last_turn_scores_;
        std::vector<float> text_obs(12, 0.0f);
        
        // Derive text length and word count metrics mathematically (no hardcoded placeholders)
        float length_norm = static_cast<float>(current_raw_input_.size()) / 100.0f;
        text_obs[0] = std::min(length_norm, 1.0f);

        size_t word_count = 0;
        std::istringstream word_iss(current_raw_input_);
        std::string w_tok;
        while (word_iss >> w_tok) ++word_count;
        float word_count_norm = static_cast<float>(word_count) / 20.0f;
        text_obs[1] = std::min(word_count_norm, 1.0f);

        text_obs[2] = scores.question;      // [2] question
        text_obs[3] = scores.command;       // [3] command
        text_obs[4] = scores.emotional;     // [4] emotional
        text_obs[5] = scores.technical;     // [5] technical
        text_obs[6] = scores.greeting;      // [6] greeting
        text_obs[7] = scores.urgency;       // [7] urgency
        text_obs[8] = 0.0f;                 // [8] yuki_name
        text_obs[9] = scores.action;        // [9] action_cue
        text_obs[10] = scores.polarity;     // [10] polarity

        float max_sig = std::max({scores.question, scores.command, scores.emotional,
                                  scores.technical, scores.greeting, scores.urgency, scores.action});
        text_obs[11] = std::clamp(max_sig, 0.1f, 0.99f); // [11] derived confidence

        // Derive training label from scores
        yuki::IntentClass heuristic_intent = static_cast<yuki::IntentClass>(last_map_intent_);
        if      (text_obs[2] > 0.4f)  heuristic_intent = yuki::IntentClass::QUERY;
        else if (text_obs[3] > 0.4f)  heuristic_intent = yuki::IntentClass::COMMAND;
        else if (text_obs[4] > 0.4f)  heuristic_intent = yuki::IntentClass::EMOTIONAL_VENT;
        else if (text_obs[6] > 0.4f)  heuristic_intent = yuki::IntentClass::META_QUESTION;
        else if (text_obs[5] > 0.4f)  heuristic_intent = yuki::IntentClass::TUTORIAL;

        {
            vse_->generativeModel().updateMapping(
                yuki::brain::inference::Modality::TEXT,
                text_obs,
                static_cast<yuki::brain::inference::IntentClass>(heuristic_intent),
                0.1f
            );

            {
                int map_intent = static_cast<int>(vse_->currentBelief().getMAP().intent);
                int label = static_cast<int>(heuristic_intent);
                bool has_signal = text_obs[2] > 0.4f || text_obs[3] > 0.4f ||
                                  text_obs[4] > 0.4f || text_obs[5] > 0.4f ||
                                  text_obs[6] > 0.4f || text_obs[9] > 0.4f;
                if (has_signal) {
                    float error    = (map_intent == label) ? 0.0f : 1.0f;
                    float new_prec = state_.precision.intent * 0.9f + (1.0f - error) * 0.1f;
                    state_.precision.intent = std::clamp(new_prec, 0.05f, 0.98f);
                }
            }
        }

        // Train precision predictor on turn outcome
        float targetPrecision = (result.requires_clarification ||
                                 result.template_family == "clarification" ||
                                 result.template_family == "dimension_clarify") ? 0.1f : 0.7f;
        vse_->trainPrecision(lastPrecisionUsed_, targetPrecision, current_raw_input_, lastUserText_, {});

        // Metacognition observation & closed loop feedback
        if (metacognition_ && vse_) {
            yuki::metacognition::TurnOutcome outcome;
            outcome.predicted_intent = static_cast<uint8_t>(last_map_intent_ >= 0 ? last_map_intent_ : 0);
            outcome.actual_response_family = result.template_family;
            outcome.precision_used = lastPrecisionUsed_;
            outcome.clarification_triggered = (result.requires_clarification ||
                                              result.template_family.find("clarif") != std::string::npos);

            outcome.belief_entropy = [&]() -> float {
                const auto& b = vse_->currentBelief();
                float h = 0.0f;
                float h_max = std::log(static_cast<float>(b.q_intent.size()));
                for (float q : b.q_intent) {
                    if (q > 0.0f) h -= q * std::log(q);
                }
                return (h_max > 0.0f) ? (h / h_max) : 0.0f;
            }();

            metacognition_->observeTurnOutcome(outcome);
            metacognition_->observePrecisionPredictor(vse_->precisionPredictor());
        }

        if (validation_loop_) {
            validation_loop_->processQueue();
        }

        static int turn_persistence_counter = 0;
        if (++turn_persistence_counter % 10 == 0 && metacognition_) {
            persistence::StateBundle bundle;
            bundle.addChunk(1, "competence", metacognition_->serializeCompetence());
            if (vse_ && vse_->precisionPredictor()) {
                bundle.addChunk(2, "predictor_weights", vse_->precisionPredictor()->serialize());
            }
            persistence::StateSerializer::write("yuki_state.bin", bundle);
        }

        // Reset for next turn
        last_map_intent_ = -1;


        last_map_confidence_ = 0.0f;

    }

    // [Gate F FIX] Write live episode to CMF EpisodicStore so SleepThread can
    // process it during idle consolidation.
    // Root cause: bootstrap seeds 20 episodes with consolidated=1; SleepThread
    // queries episode_chain WHERE consolidated=0 → always got 0 results.
    // Fix: call cmf_->ingest() (fire-and-forget) at end of every turn.
    // Path: ingest() → CMF worker → processPacket() → encoder_->encodeScores()
    //       → episodic_->insert() → episode_chain row with consolidated=0.
    //
    // TODO: yuki_response is not threaded into end_turn() — the TurnResult is
    //       returned by shape_response() but not propagated here. PatternCompletion
    //       in SleepThread may need the full response for input/output pair
    //       clustering. Threading TurnResult through end_turn() is out of Phase A
    //       scope; input-only episodes are sufficient for patternSeparation() which
    //       clusters by timestamp + intent, not response content.
    if (cmf_ && !current_raw_input_.empty()) {
        yuki::brain::memory::MemoryPacket live_pkt;
        live_pkt.type = yuki::brain::memory::MemoryPacket::USER_UTTERANCE;
        live_pkt.timestamp_ms = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count());
        live_pkt.source        = "live_turn";
        live_pkt.text          = current_raw_input_;
        live_pkt.intent_label  = (last_map_intent_ >= 0)
                                   ? std::to_string(last_map_intent_) : "unknown";
        live_pkt.confidence    = last_map_confidence_;
        live_pkt.topic_tag     = "live";
        cmf_->ingest(live_pkt);
    }

    if (!current_raw_input_.empty() && knowledge_daemon_) {
        knowledge_daemon_->updateContextKeywords(current_raw_input_);
    }

    // Publish turn completion to GlobalWorkspace / CoreBus
    // CMF, NeuralSpine, and SelfModel subscribe to ACTION_COMPLETED
    yuki::gw::Message gc_msg;
    gc_msg.topic         = yuki::gw::Topic::ACTION_COMPLETED;
    gc_msg.source_module = "TurnCoordinator";
    gc_msg.salience      = 0.5f;
    gc_msg.payload_json  = "{\"can_act\":" + std::string(decision.can_act ? "true" : "false") +
                           ",\"intent_conf\":" + std::to_string(intent_conf) + "}";
    yuki::gw::CoreBus::instance().publish(gc_msg);
    yuki::infra::ModuleRegistry::instance().heartbeat("TurnCoordinator");

    // ── Per-Turn Learning Hook (Step 5: Knowledge Weight Calibration) ──────────
    if (per_turn_hook_) {
        float reward = decision.can_act ? 0.5f : -0.5f;
        if (result.requires_clarification) reward = -0.2f;
        if (result.veto) reward = -1.0f;
        if (decision.can_act && !result.veto && !result.requires_clarification) reward = 1.0f;
        
        per_turn_hook_->onTurnEnd(
            decision.can_act && !result.veto && !result.requires_clarification,
            reward,
            intent_conf,
            vse_ ? std::vector<float>(vse_->currentBelief().q_intent.begin(), vse_->currentBelief().q_intent.end()) : std::vector<float>{},
            0, // action_idx - would need to be tracked
            vse_ ? std::vector<float>(vse_->currentBelief().q_intent.begin(), vse_->currentBelief().q_intent.end()) : std::vector<float>{},
            current_raw_input_
        );
    }

    // ── Metabolism/Economy wiring (Stage 1) ──────────────────────────────────
    if (organism_controller_) {
        auto& metabolism = organism_controller_->metabolism();
        auto& economy = organism_controller_->economy();

        // Consume compute budget for this turn
        float gflop_cost = 0.5f; // Base compute cost per turn
        if (!metabolism.consumeCompute(gflop_cost)) {
            // Compute exhausted - organism is "hungry"
            organism_controller_->onTaskFailed();
        } else {
            economy.payInference(gflop_cost);
        }

        // Consume power budget
        float kwh_cost = 0.001f;
        if (!metabolism.consumePower(kwh_cost)) {
            organism_controller_->onTaskFailed();
        } else {
            economy.payUpkeep(0.1); // Small upkeep per turn
        }

        // If turn was successful and actionable, earn credits
        if (decision.can_act && !result.veto && !result.requires_clarification) {
            float quality = intent_conf; // Use intent confidence as quality proxy
            organism_controller_->onTaskCompleted(quality);
        } else if (result.veto) {
            organism_controller_->onTaskFailed();
        }

        // User interaction resets atrophy
        organism_controller_->onUserInteraction();
    }
    // ──────────────────────────────────────────────────────────────────────────

    // Step 3.5: Update SelfModel (disabled until self_model_ member added)
    // if (self_model_ && vse_) {
    //     self_model_->updateFromTurn(result, current_raw_input_, vse_->currentBelief().entropy());
    // }
}

void TurnCoordinator::process_contradictions() {
    for (auto& c : state_.active_contradictions)
        c.tick();

    state_.active_contradictions.erase(
        std::remove_if(state_.active_contradictions.begin(),
                       state_.active_contradictions.end(),
                       [](const ContradictionEvent& c){ return c.should_archive(); }),
        state_.active_contradictions.end());

    for (auto& c : state_.active_contradictions)
        if (c.should_surface() && !c.surfaced_to_user)
            c.surfaced_to_user = true;
}

void TurnCoordinator::meta_update(bool action_was_correct, float predicted_confidence) {
    meta_.update(action_was_correct, predicted_confidence);
    if (meta_.should_enter_anti_hesitation())
        meta_.anti_hesitation_mode = true;
}

void TurnCoordinator::distill_memory() {
    if (memory_store_) {
        memory_store_->distill({});
    }
}

void TurnCoordinator::setMemoryFabric(yuki::memory::MemoryFabric* fabric) {
    memory_fabric_ = fabric;
}

yuki::memory::MemoryFabric* TurnCoordinator::getMemoryFabric() const {
    return memory_fabric_;
}

void TurnCoordinator::setSelfIntrospection(yuki::introspection::SelfIntrospectionTool* introspection) {
    self_introspection_ = introspection;
}

void TurnCoordinator::updateThinkingLayers(const std::vector<PresenceShell::CognitiveLayer>& layers) const {
    if (shell_) {
        shell_->setCognitiveLayers(layers);
    }
}

void TurnCoordinator::clearThinkingLayers() const {
    if (shell_) {
        shell_->clearCognitiveLayers();
    }
}

void TurnCoordinator::setYNC(ync::NeuromorphicSimulator*           sim,
                              ync::YNCPipelineBridge*               bridge,
                              ync::YNCTrainingSupervisor*           trainer,
                              yuki::CognitiveOrchestrator* orc) noexcept {
    ync_sim_          = sim;
    ync_bridge_       = bridge;
    ync_trainer_      = trainer;
    ync_orchestrator_ = orc;
}

void TurnCoordinator::initializeYNC(uint32_t neuron_count) {
    pacl_orch_owned_ = std::make_unique<yuki::CognitiveOrchestrator>();
    pacl_orch_owned_->recordActivity();
    ync_orch_owned_ = std::make_unique<ync::YncOrchestrator>();
    ync_orch_owned_->initialize();
    ync_sim_owned_ = std::make_unique<ync::NeuromorphicSimulator>();
    ync::SimulatorConfig cfg;
    cfg.num_neurons          = neuron_count;
    cfg.num_cores            = 4;
    cfg.sensory_neurons      = 128;
    cfg.motor_neurons        = 16;
    cfg.connectivity_density = 0.05f;
    ync_sim_owned_->initialize(cfg, 42);
    ync_sim_owned_->start();
    ync_bridge_owned_  = std::make_unique<ync::YNCPipelineBridge>();
    ync_trainer_owned_ = std::make_unique<ync::YNCTrainingSupervisor>();
    ync_sim_     = ync_sim_owned_.get();
    ync_bridge_  = ync_bridge_owned_.get();
    ync_trainer_ = ync_trainer_owned_.get();
}

void TurnCoordinator::shutdownYNC() {
    if (ync_sim_owned_) ync_sim_owned_->stop();
    ync_sim_owned_.reset();
    ync_bridge_owned_.reset();
    ync_orch_owned_.reset();
    ync_trainer_owned_.reset();
    pacl_orch_owned_.reset();
    ync_sim_     = nullptr;
    ync_bridge_  = nullptr;
    ync_trainer_ = nullptr;
}

void TurnCoordinator::feedYNC(const std::string& raw_text) noexcept {
    if (!ync_bridge_ || !ync_sim_) return;
    static constexpr uint64_t kFNVOffset = 14695981039346656037ULL;
    static constexpr uint64_t kFNVPrime  = 1099511628211ULL;
    uint64_t hash = kFNVOffset;
    for (unsigned char c : raw_text) {
        hash ^= static_cast<uint64_t>(c);
        hash *= kFNVPrime;
    }
    std::vector<bool> percept(10000, false);
    for (size_t i = 0; i < 10000; ++i) {
        percept[i] = ((hash >> (i % 64)) & 1u) == 1u;
        if (i % 64 == 63) {
            hash ^= kFNVPrime;
            hash *= kFNVOffset ^ static_cast<uint64_t>(i);
        }
    }
    ync_bridge_->feedSensory(percept, *ync_sim_);
    if (ync_orchestrator_) ync_orchestrator_->recordActivity();
    if (ync_orch_owned_)   ync_orch_owned_->recordUserActivity();
}

void TurnCoordinator::feedYNC(const std::vector<bool>& percept_bits) noexcept {
    if (!ync_bridge_ || !ync_sim_) return;
    ync_bridge_->feedSensory(percept_bits, *ync_sim_);
}

void TurnCoordinator::feedYNCOutcome(bool success, float confidence) noexcept {
    if (!ync_bridge_ || !ync_sim_) return;
    ync_bridge_->feedOutcome(success, confidence, *ync_sim_);
    if (ync_trainer_) {
        ync::TrainingEpisode ep{};
        ep.sensory_input     = {};
        ep.motor_output      = {};
        ep.pipeline_decision = yuki::policy::ExecutionMode::CLARIFY;
        ep.pipeline_used_ync = true;
        ep.outcome_success   = success;
        ep.user_satisfaction = success ? confidence : -confidence;
        ep.timestamp         = static_cast<uint64_t>(
            std::chrono::steady_clock::now().time_since_epoch().count());
        ync_trainer_->recordEpisode(ep);
    }
    if (ync_orch_owned_) { ync_orch_owned_->recordUserActivity(); ync_orch_owned_->tick(); }
    if (pacl_orch_owned_) pacl_orch_owned_->tick();
}

void TurnCoordinator::setInputAnalyzer(yuki::input::InputAnalyzer* ptr) {
    input_analyzer_.reset(ptr);
}

void TurnCoordinator::setContextManager(yuki::memory::ContextManager* ptr) {
    context_manager_.reset(ptr);
}

void TurnCoordinator::setProactiveEngine(yuki::organism::ProactiveEngine* ptr) {
    proactive_engine_.reset(ptr);
}

void TurnCoordinator::onWakeWordDetected() {
    // Wake word triggered turn start
}

} // namespace yuki
