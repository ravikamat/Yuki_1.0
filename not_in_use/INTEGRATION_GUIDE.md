YUKI 2.0 — BACKGROUND JOB ENGINE INTEGRATION GUIDE
═══════════════════════════════════════════════════════════════════════════════

ARCHITECTURE PRINCIPLE: Only extreme basics are hardcoded. Everything else is
learned dynamically at runtime through user interactions and self-research.

This module is PART OF THE ENGLISH LEARNING CURRICULUM. Every background job
is an English language exercise:
  • Parsing user intent → grammar & vocabulary practice
  • Scheduling tasks → temporal language understanding
  • Researching topics → reading comprehension & vocabulary acquisition
  • Summarizing results → sentence construction practice

═══════════════════════════════════════════════════════════════════════════════
FILES TO ADD TO YOUR PROJECT
═══════════════════════════════════════════════════════════════════════════════

1. src/learning/BackgroundJobEngine.h       (8700 bytes)
2. src/learning/BackgroundJobEngine.cpp     (40000 bytes)
3. src/learning/BackgroundJobIntegration.h  (minimal bridge)
4. src/learning/BackgroundJobIntegration.cpp

═══════════════════════════════════════════════════════════════════════════════
STEP 1: ADD TO CMakeLists.txt
═══════════════════════════════════════════════════════════════════════════════

Add these to your source files list:

    src/learning/BackgroundJobEngine.cpp
    src/learning/BackgroundJobIntegration.cpp

═══════════════════════════════════════════════════════════════════════════════
STEP 2: MODIFY src/dialogue/BrainInterface.cpp
═══════════════════════════════════════════════════════════════════════════════

A. Add include at top:
    #include "../learning/BackgroundJobIntegration.h"

B. Add to BrainHandle struct:
    struct BrainHandle {
        // ... existing members ...
        BackgroundJobIntegration bg_jobs;  // ← NEW
        // ...
    };

C. In yuki_init(), after other initializations:
    h->bg_jobs.initialize("data/");

D. In yuki_shutdown(), before delete h:
    h->bg_jobs.shutdown();

E. In yuki_process_turn(), BEFORE the normal conversation flow:
    // Try background job commands first (dynamic intent detection)
    std::string bg_response;
    if (h->bg_jobs.tryHandleInput(input, bg_response)) {
        std::cout << "\n" << COL_CYAN << COL_BOLD << "Yuki: " << COL_RESET
                  << COL_GREEN << bg_response << COL_RESET << "\n\n";
        return 1;
    }

═══════════════════════════════════════════════════════════════════════════════
STEP 3: MODIFY src/dialogue/ConversationEngine.cpp (Optional Enhancement)
═══════════════════════════════════════════════════════════════════════════════

In ConversationEngine::internal_turn(), add a hook so that when Yuki says
"I don't fully understand", it can automatically queue a background job:

    // After enterLearningLoop() returns:
    if (!fully_understood) {
        auto result = enterLearningLoop(user_input, deep_parse, unknown_concepts);

        // NEW: Auto-queue background research on unknown concepts
        for (auto& unk : unknown_concepts) {
            if (unk.length() > 2) {
                // Find the bg_jobs integration via AgentCore or pass it in constructor
                // This requires adding a reference to BackgroundJobIntegration in
                // ConversationEngine's constructor
            }
        }

        return result;
    }

ALTERNATIVE (cleaner): Don't modify ConversationEngine at all. The
BackgroundJobIntegration in BrainInterface.cpp handles everything before
ConversationEngine even sees the input.

═══════════════════════════════════════════════════════════════════════════════
STEP 4: ADD ENGLISH CURRICULUM SEED (Optional)
═══════════════════════════════════════════════════════════════════════════════

In CurriculumEngine::seedEnglish(), add background job-based lessons:

    void CurriculumEngine::seedEnglish() {
        // ... existing topics ...

        // NEW: Background job-driven English learning
        std::vector<std::string> bg_topics = {
            "intent parsing and command understanding",
            "temporal expressions and scheduling language",
            "web research reading comprehension",
            "sentence summarization and paraphrasing",
            "vocabulary acquisition from context",
            "grammar pattern recognition in wild text"
        };
        for (int i = 0; i < (int)bg_topics.size(); ++i) {
            Lesson l{Subject::ENGLISH, bg_topics[i], bg_topics[i], i + 11, false, 0.0f};
            tracks_[Subject::ENGLISH].push_back(l);
        }
    }

═══════════════════════════════════════════════════════════════════════════════
WHAT IS HARDCODED vs DYNAMIC
═══════════════════════════════════════════════════════════════════════════════

HARDCODED (minimal, never changes):
  • Job structure fields (id, status, priority, timestamps)
  • Thread scheduler loop (sleep intervals, queue processing)
  • JSON serialization format
  • 5 seed trigger words for "add" and "remove" actions
  • 12 seed time patterns ("at", "in", "tomorrow", "every day", etc.)

DYNAMIC (learned at runtime):
  • What phrases mean "add a job" (learns from user corrections)
  • What phrases mean "remove/pause/list" (learns from usage)
  • Time/date parsing rules (learns from user corrections like "no, I meant 5pm")
  • Job type → handler mapping (learns which intents work best)
  • Pattern confidence scores (self-adjusting based on success rate)
  • Vocabulary learned from job topics
  • Grammar patterns extracted from research text

═══════════════════════════════════════════════════════════════════════════════
USER INTERACTION EXAMPLES
═══════════════════════════════════════════════════════════════════════════════

User: "add researching neural networks to my background jobs"
Yuki: "Added background job [a3f7b2d1] for: neural networks"

User: "queue this up for tonight at 9pm"
Yuki: "Added background job [b8e2c4a5] for: this (scheduled: tonight at 9pm)"

User: "every morning learn 5 new English words"
Yuki: "Added background job [c1d9e3f2] for: learn 5 new English words (scheduled: daily)"

User: "pause my trading research job"
Yuki: "Paused job [d4a7b1c3]: trading research"

User: "show me what my background jobs are doing"
Yuki: "Your Background Jobs (3 total):
  [a3f7b2d1] PENDING | NORMAL | neural networks
  [b8e2c4a5] SCHEDULED | NORMAL | this [scheduled]
  [c1d9e3f2] RUNNING | NORMAL | learn 5 new English words (45%)"

User: "what's the status of job a3f7b2d1"
Yuki: "Job Report [a3f7b2d1]
  Topic: neural networks
  Status: COMPLETED
  New Words Learned: backpropagation gradient descent
  Comprehension Gain: +12%"

User: "add this to your background job" (vague)
Yuki: "I'm not sure if you want to add, remove, list, or check a background job.
  Try saying: 'add [topic] to my background jobs'..."
  → Normal conversation continues. The engine LEARNS from this interaction.

═══════════════════════════════════════════════════════════════════════════════
HOW THE ENGINE LEARNS
═══════════════════════════════════════════════════════════════════════════════

1. Pattern Learning:
   - User says "add researching X to bg"
   - Engine matches "add" trigger → creates job
   - Engine records: trigger phrase "add ... to bg" → intent "add_job"
   - Next time user says "queue X in bg", engine learns "queue ... in bg" also means add

2. Schedule Learning:
   - User says "do this at 5pm"
   - Engine parses "at 5pm" successfully
   - User later says "no, I meant 17:00"
   - Engine learns: "17:00" is also a valid time format
   - Time pattern vocabulary expands

3. Intent Clarification:
   - User says "handle this later"
   - Engine is unsure: "add" or "schedule"?
   - Engine asks for clarification
   - User says "yes, add it"
   - Engine learns: "handle ... later" → "add_job" with schedule

4. Self-Consolidation:
   - Every 60 idle cycles, engine merges similar patterns
   - Low-confidence patterns (< 0.2) are pruned
   - High-usage patterns are boosted
   - Everything is persisted to background_jobs.json

═══════════════════════════════════════════════════════════════════════════════
BUILD & TEST
═══════════════════════════════════════════════════════════════════════════════

After adding files and modifications:

    Remove-Item -Recurse -Force build -ErrorAction SilentlyContinue
    cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl
    cmake --build build --config Release --target yuki_brain

Test commands:
    "add learning about candlestick patterns to my background jobs"
    "schedule vocabulary practice for tomorrow morning"
    "list my background jobs"
    "what's my English curriculum progress"

═══════════════════════════════════════════════════════════════════════════════
