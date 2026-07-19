# Yuki Brain Pipeline — Complete Understanding Architecture
## From Raw Input to Human-Like Understanding (No Preloaded DB)

---

## Overview

This document defines the complete Yuki understanding pipeline — from raw noisy input to correctly routed response or action. The core principle is this: **Yuki does not need to start by knowing everything. She needs to know how to handle not knowing.** Every module in this pipeline exists to move Yuki from raw signals toward structured meaning, without relying on a pre-filled knowledge database or a generic LLM.[cite:561][cite:565]

The fundamental law of the pipeline:

> **Unknown word handling → before meaning failure.**
> **Meaning formation → before capability judgment.**
> **Request classification → before any action or web lookup.**

---

## Why the Current System Fails

Yuki currently:
1. Receives raw text input.
2. Fires a few keyword/trigger patterns.
3. Jumps immediately to capability checking or Wikipedia lookup.
4. Gets bad results from mismatched search queries.
5. Falls into a generic rescue flow.

This is backwards. A human first understands what kind of thing is being asked, then decides how to respond. The fix is a **sequential understanding pipeline** that forms meaning before routing.[cite:630][cite:662][cite:648]

---

## The Master Pipeline — 10 Stages

```
RAW INPUT (text / voice / screen / environment)
        ↓
[Stage 1]  InputNormalizer
        ↓
[Stage 2]  UncertaintyDetector
        ↓
[Stage 3]  CandidateGenerator
        ↓
[Stage 4]  QueryRewriter
        ↓
[Stage 5]  EntitySpanDetector + EntityLinker
        ↓
[Stage 6]  CandidateRanker
        ↓
[Stage 7]  MeaningStateBuilder  ← central brain object
        ↓
[Stage 8]  RequestClassifier
        ↓
[Stage 9]  GoalBuilder
        ↓
[Stage 10] KnowledgeRouter / TaskRouter / ClarificationGate
        ↓
    ANSWER / PLAN / CLARIFICATION QUESTION
        ↓
[Stage 11] Verifier + LearningUpdate
```

---

## Stage 1 — InputNormalizer

### Purpose
Produce a clean, stable version of the raw input without destroying the original. All later modules depend on normalized text, not messy raw strings.

### Logic
- Accept raw text from any channel: typed text, Whisper STT transcript, OCR output, or screen-captured text.
- Produce **three versions**:
  - `raw_text` — exact original, preserved always for user style and error tracing.
  - `clean_text` — repeated punctuation removed, spacing normalized, leading/trailing cleaned.
  - `canonical_text` — lowercased, common contractions expanded (can't → cannot), STT artifacts removed (um, uh stripped).
- **Do NOT correct unknown words here** — that is Stage 3's job. Normalizer only cleans formatting, not meaning.
- Detect input language using character distribution and common stop-word presence. Tag detected language as `lang_code`.

### Output struct (C++)
```cpp
struct NormalizedInput {
    std::string raw_text;
    std::string clean_text;
    std::string canonical_text;
    std::string lang_code;         // "en", "hi", "hinglish", "unknown"
    std::string input_source;      // "voice", "text", "screen", "mobile"
    int64_t     timestamp_ms;
};
```

### File
`src/brain/InputNormalizer.h` / `.cpp`

---

## Stage 2 — UncertaintyDetector

### Purpose
Mark every token or span in the canonical text that Yuki is uncertain about. This is how a blank-start system signals "I may not understand this" before attempting to interpret. It prevents the system from confidently misrouting misspelled or ambiguous input.

### Logic

**Lexical uncertainty** (token-level):
- Token not in local vocabulary index → flag as `UNKNOWN_WORD`.
- Token has unusual character patterns such as mixed case, random vowels, or abnormal length → flag as `SUSPICIOUS`.
- Token is a rare bigram pair with its neighbors → flag as `UNEXPECTED_CONTEXT`.
- Phonetically unlikely sequence for the language → flag as `POSSIBLE_STT_ERROR`.

**Semantic uncertainty** (sentence-level):
- Sentence contains multiple conflicting action verbs → flag as `MULTI_INTENT`.
- Sentence has no clear action verb at all → flag as `INTENT_MISSING`.
- Entity span looks incomplete — name is partial or has no identifying context → flag as `WEAK_ENTITY`.
- Confidence of whole sentence meaning falls below threshold → flag as `LOW_CONFIDENCE`.

### Vocabulary bootstrap (blank-start solution)
Yuki starts with **no full dictionary**, but she can build a local vocabulary index organically:
- A small seed list of ~5,000 most common English words (open source, no license issues).
- Every word that survives a successful interaction gets added to the index with a frequency counter.
- Every word fetched during a knowledge lookup gets added with an entity-type tag.
- Over time Yuki's vocabulary grows entirely from real use — matching the child-like learning model.

### Output
```cpp
struct UncertaintyReport {
    struct TokenFlag {
        int         token_index;
        std::string token;
        std::string flag_type;   // UNKNOWN_WORD, SUSPICIOUS, STT_ERROR, etc.
        float       confidence;  // 0.0 = very uncertain, 1.0 = very certain
    };
    std::vector<TokenFlag> token_flags;
    std::string sentence_flag;   // MULTI_INTENT, INTENT_MISSING, etc.
    float overall_certainty;     // aggregate sentence confidence
};
```

### File
`src/brain/UncertaintyDetector.h` / `.cpp`

---

## Stage 3 — CandidateGenerator

### Purpose
For every uncertain token or span, produce a ranked list of what it most likely actually is. This is the first and most important step toward handling unknown input — generate guesses, then rank them.

### Logic — three methods used in parallel

#### Method A: Edit Distance (Levenshtein)
- For each uncertain token, compute the edit distance to all tokens in the current vocabulary index.
- Edit distance measures minimum insertions, deletions, and substitutions to convert one string to another.
- `alber` → `albert` (1 substitution), `einstine` → `einstein` (2 substitutions).
- Candidates within edit distance ≤ 3 are kept; those within ≤ 1 are ranked highest.
- Complexity per token: O(n × m) where n is token length and m is vocabulary size; use index pruning to stay fast.

#### Method B: Fuzzy Matching
- For broader similarity — when the token shares significant substring overlap with a known token even if the edit distance is moderate.
- Useful for longer names or compound words where edit distance alone is noisy.
- `Similarity = shared_ngrams / total_ngrams` using trigrams (3-character windows).
- A trigram similarity ≥ 0.6 with a known vocabulary token qualifies as a candidate.

#### Method C: Phonetic / Sound Similarity (STT errors)
- For voice input specifically, many errors are sound-based not spelling-based.
- Apply Soundex or a double-metaphone encoding to both the uncertain token and the vocabulary.
- Tokens with the same phonetic code as a known word are added as phonetic candidates.
- `einstine` and `einstein` share the same phonetic signature and would match here.

### Candidate struct
```cpp
struct Candidate {
    std::string original_token;
    std::string candidate_text;
    float edit_distance_score;    // 1.0 = identical, 0.0 = very different
    float fuzzy_score;
    float phonetic_score;
    float combined_score;         // weighted sum
    std::string generation_method; // "edit", "fuzzy", "phonetic"
};

struct CandidateSet {
    std::string original_token;
    std::vector<Candidate> candidates;  // top 5, sorted by combined_score
};
```

### File
`src/brain/CandidateGenerator.h` / `.cpp`

---

## Stage 4 — QueryRewriter

### Purpose
Build multiple whole-sentence hypothesis rewrites, not only individual token fixes. A full-sentence rewrite is often much cleaner to classify and search than the original noisy sentence.

### Logic

- Take the top candidate substitutions from Stage 3 for each uncertain token.
- Combine them into complete sentence hypotheses by substituting uncertain tokens with their top candidates.
- Generate up to 3-5 whole-sentence rewrites ranked by the product of their constituent candidate scores.
- Preserve the original as hypothesis 0 (even if noisy) so ranking can still pick it if it turns out to be fine.
- Also apply sentence-level structural rewrites:
  - "do you know about X" → "who is X" / "tell me about X" (knowledge question normalization).
  - "could you X if Y" → detect conditional task pattern, split into main task + condition.
  - "what is X" → knowledge query rewrite.
  - Imperative commands without subject → add implied subject "user wants me to..."

### Output
```cpp
struct QueryHypothesis {
    std::string rewritten_query;
    float       hypothesis_score;    // combined candidate scores
    std::string pattern_label;       // "knowledge_q", "conditional_task", "command", "chat"
    std::string substitution_notes;  // e.g. "replaced: alber→albert, einstine→einstein"
};

struct RewriteSet {
    std::string original_query;
    std::vector<QueryHypothesis> hypotheses;  // sorted by score
};
```

### File
`src/brain/QueryRewriter.h` / `.cpp`

---

## Stage 5 — EntitySpanDetector + EntityLinker

### Purpose
Identify spans in the query that refer to real-world things — persons, apps, files, places, conditions, times, and systems — and link them to a type or identity. This is the shift from treating input as words to treating it as references to actual things.

### EntitySpanDetector logic

Detect spans using a combination of:
- **Structural cues**: capitalized words, named-entity-like patterns, quoted phrases.
- **Positional patterns**: token following "about," "for," "on," "to," "with" often names an entity.
- **Type-pattern rules**:
  - `[time]: "after 1 hour," "at 5pm," "tomorrow"` → TIME entity.
  - `[person]: capitalized proper-name-shaped tokens` → PERSON_CANDIDATE.
  - `[app/system]: known app name list + fuzzy match` → APP entity.
  - `[condition]: "if hot," "if rainy," "if X"` → CONDITION entity.
  - `[file/path]: path-like or extension-bearing tokens` → FILE entity.

### EntityLinker logic

Linking connects a detected span to an actual identity or category:
- **Local memory first**: check if this entity was seen before in UserMemory or ConceptVault.
- **Vocabulary + type heuristics second**: match entity shape to known type categories.
- **External evidence third**: if local match fails, trigger a lightweight search for the entity span + top candidate to gather confirming evidence.
- Assign link confidence based on how strong the match is.

### Output
```cpp
enum EntityType {
    PERSON, APP, FILE, PLACE, TIME, CONDITION,
    WEATHER_STATE, DIGITAL_OBJECT, SYSTEM_RESOURCE,
    UNKNOWN_ENTITY
};

struct LinkedEntity {
    std::string   raw_span;
    std::string   canonical_form;    // e.g. "Albert Einstein"
    EntityType    type;
    float         link_confidence;
    std::string   link_source;       // "memory", "heuristic", "web_evidence"
};
```

### File
`src/brain/EntitySpanDetector.h` / `.cpp`  
`src/brain/EntityLinker.h` / `.cpp`

---

## Stage 6 — CandidateRanker

### Purpose
Score every query hypothesis from Stage 4 using multiple signals to determine the best interpretation before forming final MeaningState. Raw string similarity alone is not enough — context must guide the final choice.

### Scoring formula
```
score = w1 × string_similarity
      + w2 × pattern_fit
      + w3 × context_fit
      + w4 × entity_fit
      + w5 × external_evidence
      + w6 × memory_support
      - w7 × ambiguity_penalty
```

### Signal definitions

| Signal | What it measures | How computed |
|---|---|---|
| `string_similarity` | How close tokens are to known words | Avg candidate edit/fuzzy scores |
| `pattern_fit` | Whether sentence form matches a known intent pattern | Rule-based pattern match score |
| `context_fit` | Whether surrounding words support interpretation | Co-occurrence in vocabulary index |
| `entity_fit` | Whether entity types agree with the interpretation | EntityLinker confidence scores |
| `external_evidence` | Whether a quick web check confirms the candidate | Evidence retrieval score |
| `memory_support` | Whether prior user context supports interpretation | UserMemory lookup score |
| `ambiguity_penalty` | Penalize when multiple interpretations score similarly | Score gap between top-2 candidates |

### Default weights (tuneable at runtime)
- `w1=0.20, w2=0.25, w3=0.15, w4=0.15, w5=0.10, w6=0.10, w7=0.05`
- Weights should update over time as Yuki learns which signals matter most for this user.

### Output
Ranked list of `QueryHypothesis` + `LinkedEntity` combinations with final confidence scores. Top-ranked hypothesis becomes the working interpretation.

### File
`src/brain/CandidateRanker.h` / `.cpp`

---

## Stage 7 — MeaningStateBuilder

### Purpose
Build the central brain object that represents everything Yuki currently understands about the input. This is the most important data structure in the pipeline — every later module reads from it, not from raw text.

### Struct
```cpp
struct MeaningState {
    // === Input representation ===
    std::string raw_input;
    std::string normalized_input;
    std::string best_hypothesis;       // top-ranked rewrite

    // === Entities ===
    std::vector<LinkedEntity> entities;    // all detected and linked entities
    std::vector<std::string>  action_verbs;
    std::vector<std::string>  objects;
    std::vector<std::string>  conditions; // "if rainy", "if hot"
    std::string               time_constraint; // "after 1 hour", "at 5pm"

    // === Understanding state ===
    float    overall_confidence;
    bool     has_ambiguity;
    bool     has_multi_intent;
    bool     needs_clarification;
    std::vector<std::string> ambiguous_spans;
    std::vector<std::string> unknown_spans_remaining;

    // === Request classification (filled by Stage 8) ===
    std::string request_type;   // KNOWLEDGE_QUERY, TASK_REQUEST, etc.

    // === Goal (filled by Stage 9) ===
    std::string goal_summary;
    std::map<std::string, std::string> known_slots;
    std::vector<std::string>           missing_slots;

    // === Metadata ===
    std::string lang_code;
    std::string input_source;
    int64_t     pipeline_start_ms;
};
```

### Key rule
MeaningState is **read-only after it is sealed** by MeaningStateBuilder. Later stages can read it and produce derived objects, but they should not mutate MeaningState directly. This prevents state corruption when multiple modules run near-concurrently.

### File
`src/brain/MeaningStateBuilder.h` / `.cpp`

---

## Stage 8 — RequestClassifier

### Purpose
Decide what kind of thing the user is requesting. This is the exact stage missing from Yuki's current pipeline. Without it, she treats all inputs the same way and routes them wrongly.

### Classification labels

| Label | Meaning | Example |
|---|---|---|
| `KNOWLEDGE_QUERY` | User wants information about something | "do you know about Einstein?" |
| `TASK_REQUEST` | User wants Yuki to do something | "open Chrome and search for recipes" |
| `CONDITIONAL_TASK` | Task that depends on a runtime condition | "update schedule if it rains" |
| `SOCIAL_CHAT` | Conversational/social exchange | "hi", "how are you", "thanks" |
| `EMOTIONAL_STATE` | User expressing feeling, not requesting action | "I'm not feeling well" |
| `IDENTITY_QUERY` | User asking about Yuki herself | "what can you do?", "who are you?" |
| `CLARIFICATION_NEEDED` | Yuki cannot determine intent from current input | Low-confidence, ambiguous, or multi-intent |
| `UNSUPPORTED` | Request is clear but outside any possible route | Explicitly illegal, impossible on this system |

### Logic — layered approach (no heavy LLM needed)

**Layer 1 — Pattern rules (fastest, highest priority):**
- Sentence starts with "what is / who is / tell me about / do you know about" → `KNOWLEDGE_QUERY`.
- Sentence starts with "open / start / launch / run / install / send / delete / move / copy" → `TASK_REQUEST`.
- Sentence contains "if [condition] then [action]" or "when [event] do [action]" → `CONDITIONAL_TASK`.
- Sentence is a greeting, thank-you, or acknowledgment → `SOCIAL_CHAT`.
- First-person emotional verbs: "I feel / I'm sad / I'm tired / I'm frustrated" → `EMOTIONAL_STATE`.
- Questions about Yuki's own abilities or identity → `IDENTITY_QUERY`.

**Layer 2 — Feature-based classification (for cases Layer 1 misses):**
- Build feature vector: action_verb_present, entity_type, has_condition, has_time_ref, sentence_length, question_word.
- Use a lightweight trained classifier (sklearn decision tree or naive bayes via Python subprocess).
- No LLM — fast local inference, well under 50ms.

**Layer 3 — Confidence gate:**
- If top classification confidence < 0.60 → classify as `CLARIFICATION_NEEDED`.
- If multiple labels score within 0.05 of each other → also `CLARIFICATION_NEEDED`.

### File
`src/brain/RequestClassifier.h` / `.cpp`

---

## Stage 9 — GoalBuilder

### Purpose
Convert MeaningState + RequestClassifier label into a concrete internal goal. This is where Yuki stops asking "what words are these?" and starts asking "what outcome is being requested, and what do I need to achieve it?"

### Goal patterns

**Knowledge goal:**
```
Goal {
  type: KNOWLEDGE_QUERY
  action: explain / define / summarize / list
  target_entity: Albert Einstein
  known_slots: { entity: "Albert Einstein", type: PERSON }
  missing_slots: []
  route: KNOWLEDGE_FLOW
}
```

**Simple task goal:**
```
Goal {
  type: TASK_REQUEST
  action: open + search
  target_app: Chrome
  target_query: "recipes"
  known_slots: { app: "Chrome", query: "recipes" }
  missing_slots: []
  route: TASK_FLOW
}
```

**Conditional task goal:**
```
Goal {
  type: CONDITIONAL_TASK
  main_action: modify(schedule)
  trigger_condition: weather == hot OR weather == rainy
  trigger_timing: after 1 hour
  known_slots: { condition: "rainy/hot", timing: "1 hour" }
  missing_slots: { schedule_target: UNKNOWN }  // needs clarification
  route: TASK_FLOW → CLARIFICATION for missing_slots
}
```

### GoalBuilder logic
1. Read `request_type` from MeaningState.
2. Map action verbs and entity types to goal pattern.
3. Fill `known_slots` from resolved entities and conditions.
4. List any `missing_slots` that are required but absent.
5. If `missing_slots` is non-empty, set route as `CLARIFICATION → then TASK_FLOW`.
6. Set final `route`: `KNOWLEDGE_FLOW`, `TASK_FLOW`, `CLARIFICATION`, `SOCIAL_RESPONSE`, `EMOTIONAL_RESPONSE`, `IDENTITY_RESPONSE`, `UNSUPPORTED_RESPONSE`.

### File
`src/brain/GoalBuilder.h` / `.cpp`

---

## Stage 10 — Routers + ClarificationGate

### KnowledgeRouter
Handles all `KNOWLEDGE_FLOW` goals.

1. **Local memory first**: check ConceptVault and TraceStore for cached answer.
2. **Confidence check**: if local answer confidence ≥ 0.80, respond immediately.
3. **Web research**: if local confidence is low, send the clean entity name + query to WebReconAgent.
4. **Synthesis**: pass retrieved evidence to SynthesisEngine for a coherent response.
5. **Learning**: store the result in ConceptVault with the confirmed entity name and answer confidence.

### TaskRouter
Handles all `TASK_FLOW` goals.

1. **Capability check**: look up CapabilityMap for every required action.
2. **Tool check**: verify required tools exist and are installed.
3. **Permission check**: gate through SafetyGovernor before touching system.
4. **Plan generation**: call AutonomousPlanner to build a step-by-step plan.
5. **Show plan + get approval**: always before execution.
6. **Execute via SystemExecutor** with the appropriate route: API → native → UI automation → OCR.
7. **Verify**: check execution result.

### ClarificationGate
Called whenever `needs_clarification = true` or `missing_slots` is non-empty.

Rules:
- Ask exactly **one** question per turn — the highest-priority missing slot.
- State any assumptions Yuki is making clearly before asking.
- Store the user's answer directly into MeaningState slots and re-run GoalBuilder.
- Never ask the same question twice — store all answers in UserMemory.

### File
`src/brain/KnowledgeRouter.h` / `.cpp`  
`src/brain/TaskRouter.h` / `.cpp`  
`src/brain/ClarificationGate.h` / `.cpp`

---

## Stage 11 — Verifier + LearningUpdate

### Verifier
After every answer or action:
- Knowledge answers: record whether user followed up with a correction or accepted the answer.
- Task actions: verify execution outcome with multi-source check.
- Flag anomalies for SelfAuditEngine.

### LearningUpdate
After every successful turn:
- Add confirmed entity → canonical name mapping to vocabulary index.
- Update CandidateRanker weights if a lower-weighted signal turned out to be correct.
- Update CapabilityMap success rate for any capability used.
- Add new skill or correction recipe to SkillRegistry if pattern is reusable.

This is how Yuki grows her own internal understanding without a preloaded database — every real interaction teaches her something new and permanent.

### File
`src/brain/LearningUpdate.h` / `.cpp`

---

## Full Worked Examples

### Example 1 — "do you know about alber einstine?"

| Stage | Result |
|---|---|
| InputNormalizer | `canonical_text = "do you know about alber einstine"` |
| UncertaintyDetector | `alber` → UNKNOWN_WORD, `einstine` → UNKNOWN_WORD |
| CandidateGenerator | `alber` → [albert:0.91], `einstine` → [einstein:0.88] |
| QueryRewriter | Hypothesis 1: "do you know about albert einstein" (0.89), Hypothesis 2: "who is albert einstein" (0.88) |
| EntitySpanDetector | Span: "albert einstein" → PERSON_CANDIDATE |
| EntityLinker | "albert einstein" → PERSON, confidence 0.82 |
| CandidateRanker | Top hypothesis: "who is albert einstein" (score 0.87) |
| MeaningStateBuilder | entity=Albert Einstein, type=PERSON, action=explain, confidence=0.87 |
| RequestClassifier | `KNOWLEDGE_QUERY` (pattern: "do you know about X") |
| GoalBuilder | goal=explain(Albert Einstein), route=KNOWLEDGE_FLOW |
| KnowledgeRouter | Search → synthesize → answer |
| LearningUpdate | Store "alber einstine" → "Albert Einstein" in vocabulary index |

**Response**: "Albert Einstein was a German-born theoretical physicist best known for developing the theory of relativity..."

---

### Example 2 — "could you check weather and update my schedule if its too hot or rainy after 1 hour"

| Stage | Result |
|---|---|
| InputNormalizer | `canonical_text = "could you check weather and update my schedule if it is too hot or rainy after 1 hour"` |
| UncertaintyDetector | No unknown words; sentence flag = MULTI_INTENT + CONDITION_PRESENT |
| CandidateGenerator | No token corrections needed |
| QueryRewriter | Pattern detected: conditional task → "check weather after 1 hour; if hot OR rainy → update schedule" |
| EntitySpanDetector | "weather" → WEATHER_STATE, "schedule" → DIGITAL_OBJECT, "after 1 hour" → TIME, "hot/rainy" → CONDITION |
| CandidateRanker | Conditional task interpretation scores 0.91 |
| MeaningStateBuilder | action1=check(weather), condition=hot OR rainy, timing=1h, action2=update(schedule), missing=which_schedule |
| RequestClassifier | `CONDITIONAL_TASK` |
| GoalBuilder | goal=monitor(weather) → if condition → modify(schedule_target), missing_slots=[schedule_target] |
| ClarificationGate | "Which schedule or calendar should I update — your phone calendar or a specific file?" |
| TaskRouter | After user answers → build plan → show plan → get approval → execute |

---

## Build Order

| Priority | Module | Why first |
|---|---|---|
| 1 | InputNormalizer | Everything else depends on clean text |
| 2 | UncertaintyDetector | Enables blank-start unknown handling |
| 3 | CandidateGenerator | Core correction / spelling logic |
| 4 | QueryRewriter | Sentence-level intent cleaning |
| 5 | EntitySpanDetector | Words → things |
| 6 | EntityLinker | Things → types + identities |
| 7 | CandidateRanker | Multi-signal scoring |
| 8 | MeaningStateBuilder | Central brain object |
| 9 | RequestClassifier | Route decision |
| 10 | GoalBuilder | Outcome formulation |
| 11 | KnowledgeRouter | Answer flow |
| 12 | TaskRouter + SafetyGovernor | Action flow |
| 13 | ClarificationGate | Blocker resolution |
| 14 | Verifier + LearningUpdate | Self-growing brain |

---

## File Map

```
src/brain/
├── InputNormalizer.h / .cpp
├── UncertaintyDetector.h / .cpp
├── CandidateGenerator.h / .cpp
├── QueryRewriter.h / .cpp
├── EntitySpanDetector.h / .cpp
├── EntityLinker.h / .cpp
├── CandidateRanker.h / .cpp
├── MeaningStateBuilder.h / .cpp
├── RequestClassifier.h / .cpp
├── GoalBuilder.h / .cpp
├── KnowledgeRouter.h / .cpp
├── TaskRouter.h / .cpp
├── ClarificationGate.h / .cpp
└── LearningUpdate.h / .cpp

data/
├── vocabulary_index.json       ← grows from real use
├── entity_type_rules.json      ← person/app/file/condition patterns
├── capability_map.json         ← what Yuki can do + confidence
└── candidate_rank_weights.json ← scoring weights, updated over time
```

---

## Key Principle (never forget)

> **Yuki does not start by knowing everything.**
> She starts by knowing how to handle not knowing.
> Every successful interaction makes her slightly more capable.
> That is the child-like grounding model — applied to a machine.

---

*This document is the Yuki Understanding Pipeline master reference. When building any brain module, follow this order and these interfaces.*
