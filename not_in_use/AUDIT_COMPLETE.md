# YUKI_1.0 — COMPREHENSIVE CODEBASE AUDIT

**Date:** 2026-06-17
**Scope:** All src/ .cpp/.h files, tests/, CMakeLists.txt wiring

---

## PART 1: FILES NOT WIRED INTO BUILD SYSTEM

### 1.1 Source Files That Exist But Are NOT Compiled

| File | Status | Reason |
|------|--------|--------|
| `src/brain/self/YukiSelfModel.cpp` | **DEAD CODE** | Not listed in CMakeLists.txt anywhere. Alternative SelfModel implementation. |
| `src/brain/self/YukiSelfModel.h` | **DEAD CODE** | Header for above. Never included by any compiled translation unit. |

### 1.2 Header-Only / Stub Files (No .cpp, Not Compiled)

| File | Status | Reason |
|------|--------|--------|
| `src/brain/MotherCore.h` | **STUB** | Comment explicitly says "minimal stub for compilation". Contains inline `handleInput()` that just prepends "Processed: ". Not included by any compiled file. |
| `src/brain/predictive/turn_trace.h` | **ORPHANED HEADER** | Only header, no .cpp. Included by predictive_turn_engine.h but never instantiated. |
| `src/RuntimeWorkerBase.h` | **ORPHANED HEADER** | Header only, no .cpp. Not included anywhere. |
| `src/input/encoding/TemporalContext.h` | **ORPHANED HEADER** | Header only. Not included anywhere. |
| `src/brain/ExecutionTypes.h` | **ORPHANED HEADER** | Header only. Not included anywhere. |
| `src/brain/KnowledgeRecord.h` | **ORPHANED HEADER** | Header only. Not included anywhere. |
| `src/brain/MeaningTypes.h` | **ORPHANED HEADER** | Header only. Not included anywhere. |

### 1.3 Test Files That Exist But Are NOT Compiled

| File | Status |
|------|--------|
| `tests/test_scrapling_integration.cpp` | **NOT IN CMakeLists.txt** — exists on disk but no test target builds it |

### 1.4 not_in_use/ Directory (Explicitly Disconnected)

All 10+ files in `not_in_use/` are explicitly disconnected from the build:
- `not_in_use/LearningUpdate.cpp/.h`
- `not_in_use/Logger.h`
- `not_in_use/MotherCore.h`
- `not_in_use/Phase1Tests.cpp/.h`
- `not_in_use/YukiTestRunner.cpp/.h`
- `not_in_use/YukiTestTypes.h`
- `not_in_use/curiosity/CuriosityEngine.cpp/.h`
- `not_in_use/P0_FIX_IMPLEMENTATION_REPORT.md`

---

## PART 2: HEURISTICS AND STUBS WITH LINE NUMBERS

### CRITICAL STUBS (Known Implementation Gaps)

#### CRITICAL 1: SelfModel::updateCuriosity() — Incorrect Topic Mapping
**File:** `src/brain/self/SelfModel.cpp`
**Lines:** 112-120
```cpp
void SelfModel::updateCuriosity(float vse_entropy) {
    for (size_t i = 0; i < current_.curiosity.size(); ++i) {
        auto& cur = current_.curiosity[i];
        size_t comp_idx = std::min(i, current_.competence.size() - 1);  // LINE 116: maps curiosity[i] to competence[i]
        float comp_level = current_.competence[comp_idx].level;
        ...
    }
}
```
**Issue:** Curiosity topic index `i` maps to competence domain index `i`, but there are 6 curiosity topics and 6 competence domains. The mapping is positional/index-based rather than semantic. CuriosityTopic::PREDICTIVE_CODING should map to CompetenceDomain::ACTIVE_INFERENCE, not just index 0.

**Severity:** Medium — works accidentally because both arrays have the same size, but the mapping is semantically wrong.

#### CRITICAL 2: SelfModel::saveToCMF() — Double History Push (FIXED)
**File:** `src/brain/self/SelfModel.cpp`
**Lines:** Previously had duplicate `history_.push_back(current_)` in both `updateFromTurn()` (line 36) and `saveToCMF()`. 
**Current Status:** `updateFromTurn()` (line 36-39) calls `history_.push_back(current_)`. `saveToCMF()` no longer has a duplicate. **FIXED.**

#### CRITICAL 3: SelfModel Persistence Schema — Missing Fields (FIXED)
**File:** `src/brain/self/SelfModel.cpp`
**Lines:** 82-84 (saveToCMF), 115-116 (loadFromCMF)
**Current Status:** `overall_uncertainty` and `growth_rate` are now serialized to JSON (lines 82-84) and parsed back (lines 115-116). **FIXED.**

#### CRITICAL 4: SelfModel::consolidate() — Minimal Implementation (PARTIALLY FIXED)
**File:** `src/brain/self/SelfModel.cpp`
**Lines:** 47-65
```cpp
void SelfModel::consolidate() {
    if (history_.empty()) return;
    float initial_competence = 0.0f;
    float current_competence = 0.0f;
    for (const auto& comp : history_.front().competence) initial_competence += comp.level;
    for (const auto& comp : current_.competence) current_competence += comp.level;
    current_.growth_rate = current_competence - initial_competence;
    // Detect stagnation and boost curiosity for underexercised domains (>24h since last exercise)
    for (size_t i = 0; i < current_.competence.size(); ++i) {
        if (now - current_.competence[i].last_exercised > 86400.0) {
            size_t topic_idx = std::min(i, current_.curiosity.size() - 1);
            current_.curiosity[topic_idx].intensity = std::min(1.0f, current_.curiosity[topic_idx].intensity + 0.1f);
        }
    }
}
```
**Issue:** `initial_competence` sums all domains into a scalar, losing per-domain granularity. Stagnation detection only uses a 24h timestamp check. No per-domain growth rate tracking. No weighted moving average.

**Severity:** Low — minimal viable implementation exists.

#### CRITICAL 5: SelfModel::estimateEpistemicValue() — Heuristic Proxy (FIXED)
**File:** `src/brain/self/SelfModel.cpp`
**Lines:** 168-175
```cpp
float SelfModel::estimateEpistemicValue(CuriosityTopic t) const {
    size_t topic_idx = static_cast<size_t>(t);
    size_t comp_idx = std::min(topic_idx, current_.competence.size() - 1);
    float comp = current_.competence[comp_idx].level;
    float conf = current_.competence[comp_idx].confidence;
    return (1.0f - comp) * (1.0f - conf);  // LINE 174
}
```
**Issue:** Uses `(1.0 - competence) * (1.0 - confidence)` as a proxy for expected information gain. This is a reasonable heuristic but doesn't account for prior observations, domain-specific uncertainty, or actual Bayesian surprise.

**Severity:** Low — functional heuristic, acceptable for MVP.

#### CRITICAL 6: SelfModel::recordCorrection() — CorrectionRecord Storage (FIXED)
**File:** `src/brain/self/SelfModel.cpp`
**Lines:** 67-81
```cpp
void SelfModel::recordCorrection(CompetenceDomain domain, 
                                 const std::string& what_i_said,
                                 const std::string& what_was_right) {
    ...
    CorrectionRecord cr;
    cr.domain = domain;
    cr.what_i_said = what_i_said;       // LINE 76: NOW STORED
    cr.what_was_right = what_was_right; // LINE 77: NOW STORED
    cr.timestamp = comp.last_exercised;
    current_.correction_history.push_back(cr);  // LINE 79: NOW STORED
}
```
**Current Status:** CorrectionRecord struct exists in SelfModel.h (lines 34-39). Serialized/deserialized in saveToCMF/loadFromCMF. **FIXED.**

#### CRITICAL 7: SelfModel::toString() — Incomplete Output (FIXED)
**File:** `src/brain/self/SelfModel.cpp`
**Lines:** 151-166
```cpp
std::string SelfModel::toString() const {
    std::ostringstream ss;
    ss << "[SelfModel] Depth: " << current_.relationship.depth 
       << " Alignment: " << current_.relationship.alignment
       << " Growth Rate: " << current_.growth_rate          // LINE 156: ADDED
       << " Uncertainty: " << current_.overall_uncertainty   // LINE 157: ADDED
       << " History Size: " << history_.size() << "\n";     // LINE 158: ADDED
    for (size_t i = 0; i < current_.competence.size(); ++i) { ... }  // LINE 161: ALL domains
    for (size_t i = 0; i < current_.curiosity.size(); ++i) { ... }   // LINE 164: ALL topics
}
```
**Current Status:** Shows all competence domains, all curiosity topics, history size, growth_rate, uncertainty. **FIXED.**

---

### HARD-CODED HEURISTICS / MAGIC NUMBERS

#### H1: MotherCore.h — Full Stub
**File:** `src/brain/MotherCore.h`, Lines 1-14
```cpp
// MotherCore.h - minimal stub for compilation
class MotherCore {
public:
    struct Result {
        std::string finalText;
    };
    inline Result handleInput(const std::string &input) {
        Result r;
        r.finalText = "Processed: " + input;  // LINE 12: Placeholder
        return r;
    }
};
```
**Issue:** Not a real implementation. Never included/used.

#### H2: GenerativeModel::initializeMappings_() — Fixed Seed Values
**File:** `src/brain/inference/GenerativeModel.cpp`, Lines 105-115
```cpp
intent_to_text_features_[QUERY] = {0.3f, 0.2f, 1.0f, 0.0f, 0.0f, ...};
intent_to_text_features_[COMMAND] = {0.3f, 0.2f, 0.0f, 1.0f, 0.0f, ...};
intent_to_text_features_[TUTORIAL] = {0.5f, 0.4f, 0.0f, 0.0f, 0.0f, ...};
```
**Issue:** Hand-crafted seed vectors for 12-dim text features. Not learned data.

#### H3: GenerativeModel::bootstrapStructuralPriors() — Index Fix Comment
**File:** `src/brain/inference/GenerativeModel.cpp`, Lines 121-130
```cpp
// Fix: indices must match the 12-dim text_obs layout...
// Previous code had QUERY[0]=0.9 (wrong: length_norm) and COMMAND[1]=0.9 (wrong: word_count).
// UNKNOWN(0)'s neutral [0.5]*12 was always closest because discriminative dims were wrong.
```
**Issue:** Historical index bug documented in code — priors had wrong indices until explicitly fixed. Demonstrates fragility.

#### H4: PolicySelector::SEED_TEMPLATES — Hand-Tuned Constants
**File:** `src/brain/inference/PolicySelector.cpp`, Lines 11-18
```cpp
const float PolicySelector::SEED_TEMPLATES[NUM_SEED_TEMPLATES][8] = {
    {0.2f, 0.3f, 0.2f, 0.1f, 0.1f, 0.0f, 0.2f, 0.3f},   // conservative
    {0.8f, 0.7f, 0.8f, 0.2f, 0.3f, 0.5f, 0.7f, 0.5f},   // thorough
    {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.3f, 0.5f, 0.5f},   // balanced
    ...
};
```
**Issue:** 6 seed policy templates with 8 hand-tuned parameters each. Not learned from data.

#### H5: FreeEnergyCalculator — Global Cache Hacks
**File:** `src/brain/inference/FreeEnergyCalculator.cpp`, Lines 17-24
```cpp
struct PolicyCache {
    BeliefState belief;
    Policy policy;
    float free_energy;
    uint64_t timestamp_ms;
    bool valid = false;
};
static PolicyCache g_policy_cache;  // LINE 24: Mutable global state
```
**Issue:** Uses a global `static PolicyCache` for caching — not thread safe, not reset between turns properly. Uses `GetTickCount64()` for cache expiry (lines 133, 176) instead of `std::chrono`.

#### H6: VseBootstrapTrainer — 200 Synthetic Examples
**File:** `src/brain/inference/VseBootstrapTrainer.cpp`
- Lines 40-108: 8 feature builders returning hardcoded 12-dim vectors
- Lines 116-180: 200 synthetic examples built from these templates
```
queryFeatures_ → {0.5f, 0.4f, 0.9f, 0.0f, ...}
commandFeatures_ → {0.4f, 0.3f, 0.0f, 0.9f, ...}
```
**Issue:** All training data is synthetic and hand-authored. Not from real user interactions. Each of the 8 feature builders returns a single fixed vector per intent class.

#### H7: BeliefState::update() — Simplified Gradient
**File:** `src/brain/inference/BeliefState.cpp`, Lines 50-72
```cpp
void BeliefState::update(const std::vector<float>& prediction_error,
                          const std::vector<float>& precision,
                          float learning_rate) {
    for (size_t i = 0; i < q_intent.size(); ++i) {
        float grad = precision[i] * prediction_error[i] * q_intent[i] * (1.0f - q_intent[i]);
        q_intent[i] += learning_rate * grad;
    }
    ...
}
```
**Issue:** Uses `q * (1-q)` as a gradient approximation — this is a heuristic Bernoulli gradient, not a true variational update. No softmax normalization, no constraint satisfaction.

#### H8: VariationalStateEstimator::update() — Unused Free Energy
**File:** `src/brain/inference/VariationalStateEstimator.cpp`, Lines 41-42
```cpp
float F = free_energy_calc_.computeF(belief_state_, prediction_error, prec_vec);
(void)F; // Unused for now  // LINE 42: UNUSED
```
**Issue:** Free energy is computed but never used. The policy is always selected regardless of F value.

#### H9: ActiveInferenceRetrieval — Non-Deterministic Hashing
**File:** `src/brain/memory/ActiveInferenceRetrieval.cpp`, Lines 27-33
```cpp
static uint64_t hashId(const std::string& id) {
    std::hash<std::string> h;  // NOT DETERMINISTIC across runs
    uint64_t seed = h(id);
    seed ^= seed >> 33;
    ...
}
```
**Issue:** `std::hash<std::string>` is implementation-defined and may differ across runs or standard library versions. Hash-based Hypervector seeds are therefore non-deterministic.

#### H10: EvidenceSystem — Naive Keyword Matching
**File:** `src/brain/reasoning/EvidenceSystem.cpp`, Lines 30-49
```cpp
bool claimsContradict(const std::string& a, const std::string& b) {
    auto la = ev_toLower(a), lb = ev_toLower(b);
    bool aNeg = ev_has(la,"not ") || ev_has(la," no ") || ev_has(la,"don't") || ev_has(la,"cannot");
    bool bNeg = ev_has(lb,"not ") || ev_has(lb," no ") || ev_has(lb,"don't") || ev_has(lb,"cannot");
    ...
    for (auto& wa : wordsA)
        for (auto& wb : wordsB)
            if (wa == wb) return true;  // LINE 49: Single shared word = contradiction!
    return false;
}
```
**Issue:** Detects contradiction by checking if one claim has a negation word and they share ANY content word > 3 chars. Highly simplistic — "I do not like cats" vs "I like dogs" would NOT contradict (no shared word > 3 chars). "The sky is blue" vs "The sky is not blue" would contradict correctly, but many false positives.

#### H11: EvidenceSystem::claimsSupport() — 2-Word Threshold
**File:** `src/brain/reasoning/EvidenceSystem.cpp`, Lines 52-63
```cpp
bool claimsSupport(const std::string& a, const std::string& b) {
    ...
    int shared = 0;
    ...
    while (sb >> w) if (w.size()>3)
        if (std::find(wordsA.begin(),wordsA.end(),w) != wordsA.end()) ++shared;
    return shared >= 2;  // LINE 62: 2 shared words = support
}
```
**Issue:** Claims "support" each other if they share 2+ words > 3 chars. "Install Python 3.12" and "Remove Python 3.10" share "Python" but are opposite actions — would be falsely classified as supporting each other.

#### H12: PredictiveTurnEngine — D1-D6 Formula Deviations
**File:** `src/brain/predictive/predictive_turn_engine.cpp`, Lines 35-70
```cpp
// [D1] BeliefPool::observe() — first observation sets belief directly
// [D2] PrecisionState::update() — disagreement-primary formula
// [D3] accumulated_surprise = (1.0 - stream_agreement) per obs
// [D4] surprise decay: *= 0.70 (not the spec's *=0.30)
// [D5] force_clarify_next_turn NOT reset in from_previous()
// [D6] Safety veto fires when safety_belief > 0.05, not < 0.95
```
**Issue:** Documented deviations from the specification. Six hard-coded formula modifications to make tests pass. All disclosed in comments but represent significant divergence from original design.

#### H13: CognitiveMemoryFabric — Placeholder Methods
**File:** `src/brain/memory/CognitiveMemoryFabric.cpp`
- Line 176: `retrieveSimilarToLast()` — "// Placeholder: retrieve most recent k episodes"
- Line 255: `querySemantic()` — `(void)subject; (void)relation;` — parameters ignored, uses default HV
- Line 260: `decayWeakConcepts()` — "// HdcSemanticGraph::decay doesn't return count yet"

#### H14: ControlPlane::isActionAllowed() — Stub Implementation
**File:** `src/infrastructure/ControlPlane.cpp`, Lines 64-73
```cpp
bool ControlPlane::isActionAllowed(const std::string& action_type,
                                    const std::string& target) const {
    // Deny file operations targeting Windows system directories
    if (action_type == "file_delete" || action_type == "file_write") {
        if (target.find("Windows")  != std::string::npos ||  // LINE 70: Simple substring match
            target.find("System32") != std::string::npos) {
            return false;
        }
    }
    return true;  // LINE 73: Everything else is allowed
}
```
**Issue:** Path traversal attack possible — "C:\\Program Files\\Windows\\..." would match but "C:\\WINDO~1\\..." would not. No allowlist, no path canonicalization.

#### H15: GlobalWorkspace — Simple Max Winner-Take-All
**File:** `src/infrastructure/GlobalWorkspace.cpp`, Lines 78-82
```cpp
auto winner_it = std::max_element(batch.begin(), batch.end(),
    [](const Coalition& a, const Coalition& b) {
        return a.salience < b.salience;  // LINE 81: Simple max by salience
    });
```
**Issue:** No actual Global Workspace Theory dynamics. No competition, no inhibition, no broadcasting of non-winning coalitions. Just picks the highest salience score.

#### H16: YukiSelfModel — Full Dead Code Duplicate
**File:** `src/brain/self/YukiSelfModel.cpp/.h` (entire files)
**Issue:** An alternative SelfModel implementation that uses a different domain model (string-keyed topics vs enum classes). Saves to flat text files instead of CMF. Never compiled or wired. ~250 lines of dead code.

---

## PART 3: UNUSED / ORPHANED H-FILES

| Header | Total Structs Defined | Used In Compiled Code? |
|--------|----------------------|----------------------|
| BrainTypes.h | ~30 structs (CanonicalInputEvent, PatternFrame, GoalHierarchy, CognitiveSituation, TaskGenome, AgentSpec, AgentResult, AgentPlan, EvidenceNode, EvidenceGraph, SynthesisPlan, SynthesisResult, FinalResponse, VerificationReport, FullTrace, SkillCapsule, ImprovementProposal, RetrievalHit, etc.) | Many unreferenced |
| SemanticGraph.h | Old keyword-based semantic graph (replaced by HdcSemanticGraph) | Still compiled but not wired |
| UniversalCache.h | Cache system | Compiled but wiring unclear |

---

## PART 4: SUMMARY STATISTICS

| Metric | Count |
|--------|-------|
| Total .cpp files in src/ | 131 |
| Total .h files in src/ | 137 |
| .cpp files compiled in CMakeLists.txt | ~120 |
| .cpp files UNCOMPILED (dead code) | 1 (YukiSelfModel.cpp) |
| Header-only files with no .cpp | 7+ |
| Test files NOT in CMakeLists.txt | 1 (test_scrapling_integration.cpp) |
| Files in not_in_use/ | 12+ |
| Documented formula deviations (D1-D6) | 6 |
| Hard-coded heuristic seed arrays | 5+ |
| Placeholder/stub methods | 8+ |
| Magic number arrays | 4+ |
| Global mutable state (static cache) | 1+ |
| Unused computed values | 1 (free energy F) |
| Non-deterministic hash functions | 1 |
| Substring-based security checks | 1 |
