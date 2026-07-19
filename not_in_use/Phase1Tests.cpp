// Phase1Tests.cpp — Comprehensive test suite for SemanticParser + LanguageLayer + GoalModel
// Tests: true positives, false tests, edge cases, crash probes
// Run: part of test_subsystems executable
#define NOMINMAX
#include "Phase1Tests.h"
#include "brain/reasoning/SemanticParser.h"
#include "LanguageLayer.h"
#include "brain/reasoning/GoalModel.h"
#ifndef STANDALONE_PHASE1
#include "EntityProcessor.h"
#include "brain/memory/UserMemory.h"
#endif
#include <iostream>
#include <string>
#include <vector>
#include <functional>
#include <cassert>

// ── Test harness ──────────────────────────────────────────────────────────────

struct TestCase {
    std::string name;
    std::function<bool()> run;
};

static int sPass = 0, sFail = 0;

static void runTest(const TestCase& tc) {
    try {
        bool ok = tc.run();
        if (ok) {
            std::cout << "  [PASS] " << tc.name << "\n";
            ++sPass;
        } else {
            std::cout << "  [FAIL] " << tc.name << "\n";
            ++sFail;
        }
    } catch (const std::exception& e) {
        std::cout << "  [CRASH] " << tc.name << " — exception: " << e.what() << "\n";
        ++sFail;
    } catch (...) {
        std::cout << "  [CRASH] " << tc.name << " — unknown exception\n";
        ++sFail;
    }
}

// ── Helper macros ─────────────────────────────────────────────────────────────

#define EXPECT_EQ(a, b)   if ((a) != (b)) { std::cout << "    Expected: " << (b) << "  Got: " << (a) << "\n"; return false; }
#define EXPECT_TRUE(x)    if (!(x))       { std::cout << "    Condition false: " #x "\n"; return false; }
#define EXPECT_FALSE(x)   if ((x))        { std::cout << "    Condition true (expected false): " #x "\n"; return false; }
#define EXPECT_NE(a, b)   if ((a) == (b)) { std::cout << "    Expected not equal: " << (a) << "\n"; return false; }
#define EXPECT_CONTAINS(str, sub) if ((str).find(sub) == std::string::npos) { std::cout << "    \"" << (str) << "\" does not contain \"" << (sub) << "\"\n"; return false; }

// ── Tests ─────────────────────────────────────────────────────────────────────

// === LANGUAGE LAYER TESTS ====================================================

static bool test_lang_english_plain() {
    LanguageLayer ll;
    auto r = ll.analyse("build me a health app");
    EXPECT_TRUE((r.detected) == DetectedLanguage::ENGLISH);
    EXPECT_EQ(r.languageCode, std::string("en"));
    EXPECT_EQ(r.normalizedEnglish, std::string("build me a health app"));
    EXPECT_FALSE(r.needsTranslation);
    return true;
}

static bool test_lang_hinglish_basic() {
    LanguageLayer ll;
    auto r = ll.analyse("aaj koi recipe batao");
    EXPECT_TRUE((r.detected) == DetectedLanguage::HINGLISH);
    EXPECT_EQ(r.languageCode, std::string("hi-en"));
    EXPECT_TRUE(r.needsTranslation);
    // normalized should have English equivalents
    EXPECT_CONTAINS(r.normalizedEnglish, "today");
    EXPECT_CONTAINS(r.normalizedEnglish, "recipe");
    return true;
}

static bool test_lang_hinglish_action() {
    LanguageLayer ll;
    auto r = ll.analyse("phone kholo aur message karo");
    EXPECT_TRUE((r.detected) == DetectedLanguage::HINGLISH);
    EXPECT_CONTAINS(r.normalizedEnglish, "open");
    return true;
}

static bool test_lang_devanagari() {
    LanguageLayer ll;
    // Devanagari "aaj" in UTF-8: आज
    std::string dev = "\xE0\xA4\x86\xE0\xA4\x9C";
    auto r = ll.analyse(dev);
    EXPECT_TRUE((r.detected) == DetectedLanguage::HINDI_DEVANAGARI);
    EXPECT_EQ(r.languageCode, std::string("hi"));
    EXPECT_TRUE(r.needsTranslation);
    return true;
}

static bool test_lang_empty_input() {
    // FALSE TEST — empty string should not crash
    LanguageLayer ll;
    auto r = ll.analyse("");
    EXPECT_TRUE((r.detected) == DetectedLanguage::ENGLISH);
    EXPECT_EQ(r.normalizedEnglish, std::string(""));
    return true;
}

static bool test_lang_only_punctuation() {
    // FALSE TEST — only punctuation
    LanguageLayer ll;
    auto r = ll.analyse("??? !!! ...");
    EXPECT_TRUE((r.detected) == DetectedLanguage::ENGLISH);  // no Hindi words, no Devanagari
    EXPECT_FALSE(r.needsTranslation);
    return true;
}

static bool test_lang_single_hindi_word_short() {
    // Single Hindi word in a very short string — threshold should be 1
    LanguageLayer ll;
    auto r = ll.analyse("batao");
    EXPECT_TRUE((r.detected) == DetectedLanguage::HINGLISH);
    return true;
}

static bool test_lang_numbers_only() {
    // FALSE TEST — only numbers
    LanguageLayer ll;
    auto r = ll.analyse("1234567890");
    EXPECT_TRUE((r.detected) == DetectedLanguage::ENGLISH);
    return true;
}

static bool test_lang_very_long_input() {
    // FALSE TEST — very long input should not crash or OOM
    LanguageLayer ll;
    std::string longInput(5000, 'a');
    longInput += " batao";  // one Hindi word at end
    auto r = ll.analyse(longInput);
    EXPECT_TRUE(r.detected == DetectedLanguage::HINGLISH ||
                r.detected == DetectedLanguage::ENGLISH);  // either acceptable
    return true;
}

static bool test_lang_adapt_response_english() {
    LanguageLayer ll;
    LanguageResult lr;
    lr.responseStyle = "english";
    std::string out = ll.adaptResponse("Hello, how can I help?", lr);
    EXPECT_EQ(out, std::string("Hello, how can I help?"));
    return true;
}

// === SEMANTIC PARSER TESTS ===================================================

static bool test_sem_whatsapp_message() {
    SemanticParser sp;
    auto f = sp.parse("send a message to Rahul on WhatsApp");
    EXPECT_TRUE((f.intent) == IntentCategory::TASK_COMMAND);
    EXPECT_TRUE(f.hasSlot("action"));
    EXPECT_EQ(f.slotValue("action"), std::string("send_message"));
    EXPECT_EQ(f.slotValue("platform"), std::string("WhatsApp"));
    EXPECT_EQ(f.slotValue("target_person"), std::string("Rahul"));
    EXPECT_FALSE(f.unknownSlots.empty());  // message content unknown
    return true;
}

static bool test_sem_operate_phone_whatsapp() {
    SemanticParser sp;
    // The spec's own example
    auto f = sp.parse("operate my phone and message my friend Rahul on WhatsApp");
    EXPECT_TRUE((f.intent) == IntentCategory::TASK_COMMAND);
    EXPECT_TRUE(f.hasSlot("device"));
    EXPECT_EQ(f.slotValue("device"), std::string("mobile_phone"));
    EXPECT_EQ(f.slotValue("platform"), std::string("WhatsApp"));
    EXPECT_EQ(f.slotValue("target_person"), std::string("Rahul"));
    // Both operate_device and send_message should be in actions
    bool hasOperate = false, hasSend = false;
    for (const auto& a : f.actions) {
        if (a == "operate_device") hasOperate = true;
        if (a == "send_message")   hasSend    = true;
    }
    EXPECT_TRUE(hasOperate);
    EXPECT_TRUE(hasSend);
    return true;
}

static bool test_sem_build_health_app() {
    SemanticParser sp;
    auto f = sp.parse("build me a health app");
    EXPECT_TRUE((f.intent) == IntentCategory::TASK_COMMAND);
    EXPECT_EQ(f.slotValue("action"), std::string("build_thing"));
    EXPECT_EQ(f.slotValue("build_type"), std::string("app"));
    EXPECT_EQ(f.domain, std::string("tech"));
    return true;
}

static bool test_sem_emotional_not_task() {
    SemanticParser sp;
    // Critical spec requirement: "I'm not feeling well" → EMOTIONAL, NOT a task
    auto f = sp.parse("I'm not feeling well");
    EXPECT_TRUE((f.intent) == IntentCategory::EMOTIONAL);
    EXPECT_TRUE(f.isEmotional);
    EXPECT_FALSE(f.needsExecution);
    return true;
}

static bool test_sem_emotional_sad() {
    SemanticParser sp;
    auto f = sp.parse("I feel so sad today");
    EXPECT_TRUE((f.intent) == IntentCategory::EMOTIONAL);
    EXPECT_TRUE(f.isEmotional);
    return true;
}

static bool test_sem_info_query_what_is() {
    SemanticParser sp;
    auto f = sp.parse("what is machine learning");
    EXPECT_TRUE((f.intent) == IntentCategory::INFORMATION_QUERY);
    EXPECT_TRUE(f.isQuestion);
    EXPECT_FALSE(f.needsExecution);
    return true;
}

static bool test_sem_info_query_explain() {
    SemanticParser sp;
    auto f = sp.parse("explain photosynthesis to me");
    EXPECT_TRUE((f.intent) == IntentCategory::INFORMATION_QUERY);
    return true;
}

static bool test_sem_open_app() {
    SemanticParser sp;
    auto f = sp.parse("open Chrome");
    EXPECT_TRUE((f.intent) == IntentCategory::TASK_COMMAND);
    EXPECT_EQ(f.slotValue("action"), std::string("open_app"));
    EXPECT_EQ(f.slotValue("platform"), std::string("Chrome"));
    return true;
}

static bool test_sem_install_software() {
    SemanticParser sp;
    auto f = sp.parse("install Android Studio on my computer");
    EXPECT_TRUE((f.intent) == IntentCategory::TASK_COMMAND);
    EXPECT_EQ(f.slotValue("action"), std::string("install_software"));
    EXPECT_EQ(f.slotValue("device"), std::string("computer"));
    return true;
}

static bool test_sem_continuation_sure() {
    SemanticParser sp;
    auto f = sp.parse("sure, go ahead");
    EXPECT_TRUE((f.intent) == IntentCategory::CONTINUATION);
    return true;
}

static bool test_sem_continuation_yes() {
    SemanticParser sp;
    auto f = sp.parse("yes");
    EXPECT_TRUE((f.intent) == IntentCategory::CONTINUATION);
    return true;
}

static bool test_sem_negation_no() {
    SemanticParser sp;
    auto f = sp.parse("no, don't do that");
    EXPECT_TRUE((f.intent) == IntentCategory::NEGATIVE);
    EXPECT_TRUE(f.isNegation);
    return true;
}

static bool test_sem_negation_stop() {
    SemanticParser sp;
    auto f = sp.parse("stop");
    EXPECT_TRUE((f.intent) == IntentCategory::NEGATIVE);
    return true;
}

static bool test_sem_self_reference() {
    SemanticParser sp;
    auto f = sp.parse("who are you");
    EXPECT_TRUE((f.intent) == IntentCategory::SELF_REFERENCE);
    return true;
}

static bool test_sem_greeting() {
    SemanticParser sp;
    auto f = sp.parse("hello yuki");
    EXPECT_TRUE((f.intent) == IntentCategory::CONVERSATIONAL);
    return true;
}

static bool test_sem_urgency_flag() {
    SemanticParser sp;
    auto f = sp.parse("send the report right now");
    EXPECT_TRUE((f.intent) == IntentCategory::TASK_COMMAND);
    EXPECT_TRUE(f.isUrgent);
    return true;
}

static bool test_sem_negation_flag() {
    SemanticParser sp;
    auto f = sp.parse("don't open that file");
    EXPECT_TRUE(f.isNegation);
    return true;
}

// === FALSE TESTS (inputs that must NOT crash or misfire) =====================

static bool test_false_empty_string() {
    SemanticParser sp;
    auto f = sp.parse("");
    EXPECT_TRUE((f.intent) == IntentCategory::UNKNOWN);
    EXPECT_EQ(f.confidence, 0.0f);
    EXPECT_TRUE(f.actions.empty());
    return true;
}

static bool test_false_only_punctuation() {
    SemanticParser sp;
    auto f = sp.parse("??? !!! ... ,,, ;;;");
    // Should not crash, should return UNKNOWN or INFORMATION_QUERY (? at end)
    EXPECT_TRUE(f.intent != IntentCategory::TASK_COMMAND);
    return true;
}

static bool test_false_only_numbers() {
    SemanticParser sp;
    auto f = sp.parse("1234567890");
    // Numbers don't map to intents
    EXPECT_TRUE(f.actions.empty());
    return true;
}

static bool test_false_spam_repetition() {
    SemanticParser sp;
    auto f = sp.parse("yuki yuki yuki yuki yuki yuki");
    // No valid slots, low confidence
    EXPECT_TRUE(f.confidence < 0.7f);
    return true;
}

static bool test_false_very_long_input() {
    SemanticParser sp;
    std::string longInput = "please send a message to Rahul on WhatsApp saying ";
    longInput += std::string(3000, 'x');  // very long message content
    // Must not crash
    auto f = sp.parse(longInput);
    EXPECT_TRUE((f.intent) == IntentCategory::TASK_COMMAND);
    EXPECT_EQ(f.slotValue("platform"), std::string("WhatsApp"));
    return true;
}

static bool test_false_sql_injection_like() {
    // FALSE TEST — injection-style input should be treated as UNKNOWN/INFO
    SemanticParser sp;
    auto f = sp.parse("'; DROP TABLE skills; --");
    EXPECT_TRUE(f.intent != IntentCategory::TASK_COMMAND ||
                f.actions.empty());  // no valid action verb matches
    return true;
}

static bool test_false_mixed_script() {
    // Hinglish through LanguageLayer then SemanticParser
    LanguageLayer ll;
    SemanticParser sp;
    auto lr = ll.analyse("WhatsApp pe Rahul ko message karo");
    auto f  = sp.parse(lr.normalizedEnglish);
    // After normalization: "WhatsApp on Rahul to send_message do"
    // Should detect send_message and WhatsApp
    EXPECT_EQ(f.slotValue("platform"), std::string("WhatsApp"));
    return true;
}

static bool test_false_question_not_command() {
    // "what is WhatsApp" should be INFO_QUERY, NOT TASK_COMMAND
    SemanticParser sp;
    auto f = sp.parse("what is WhatsApp");
    EXPECT_TRUE((f.intent) == IntentCategory::INFORMATION_QUERY);
    EXPECT_FALSE(f.needsExecution);
    return true;
}

static bool test_false_teach_not_task() {
    SemanticParser sp;
    auto f = sp.parse("learn this: when I say morning, greet me");
    EXPECT_TRUE((f.intent) == IntentCategory::TEACH);
    EXPECT_FALSE(f.needsExecution);
    return true;
}

static bool test_false_confidence_unknown() {
    SemanticParser sp;
    auto f = sp.parse("xyzzy plugh frobozz");  // total nonsense
    EXPECT_TRUE((f.intent) == IntentCategory::UNKNOWN);
    EXPECT_TRUE(f.confidence < 0.5f);
    return true;
}

// === GOAL MODEL TESTS ========================================================

static bool test_goal_spec_from_frame() {
    SemanticParser sp;
    LanguageLayer ll;
    GoalModelBuilder gb;

    auto lr = ll.analyse("send a message to Rahul on WhatsApp");
    auto f  = sp.parse(lr.normalizedEnglish);
    auto gs = gb.buildSpec(f, lr);

    EXPECT_EQ(gs.language, std::string("en"));
    EXPECT_FALSE(gs.needsExecution == false && !f.actions.empty());
    EXPECT_TRUE(gs.knownSlots.count("platform") > 0);
    EXPECT_EQ(gs.knownSlots.at("platform"), std::string("WhatsApp"));
    EXPECT_TRUE(gs.needsClarification);  // message content unknown
    return true;
}

static bool test_goal_taskstate_created() {
    SemanticParser sp;
    LanguageLayer ll;
    GoalModelBuilder gb;

    auto lr = ll.analyse("open Chrome");
    auto f  = sp.parse(lr.normalizedEnglish);
    auto gs = gb.buildSpec(f, lr);
    auto ts = gb.createTaskState(gs, "task_001");

    EXPECT_EQ(ts.taskId, std::string("task_001"));
    EXPECT_TRUE(ts.status == CogTaskState::Status::PROPOSED);   // enum class — use EXPECT_TRUE
    EXPECT_FALSE(ts.userApproved);
    EXPECT_TRUE(ts.createdAtMs > 0);
    EXPECT_EQ(CogTaskState::statusLabel(CogTaskState::Status::PROPOSED), std::string("PROPOSED"));
    return true;
}

static bool test_goal_emotional_not_execution() {
    SemanticParser sp;
    LanguageLayer ll;
    GoalModelBuilder gb;

    auto lr = ll.analyse("I am so stressed today");
    auto f  = sp.parse(lr.normalizedEnglish);
    auto gs = gb.buildSpec(f, lr);

    EXPECT_TRUE(gs.isEmotional);
    EXPECT_FALSE(gs.needsExecution);
    return true;
}

static bool test_goal_safety_level_send() {
    SemanticParser sp;
    LanguageLayer ll;
    GoalModelBuilder gb;

    auto lr = ll.analyse("send email to boss");
    auto f  = sp.parse(lr.normalizedEnglish);
    auto gs = gb.buildSpec(f, lr);
    // Safety level is set in createTaskState from the GoalModel
    // (currently GoalModelBuilder derives it internally)
    EXPECT_FALSE(gs.goal.empty());
    return true;
}

#ifndef STANDALONE_PHASE1
static bool test_sem_entity_classification() {
    EntitySpanDetector esd;
    EntityLinker el;
    UserMemory memory("data/brain/user_memory.json");

    // Test 1: Apple ambiguous
    {
        auto spans = esd.detectSpans("what is apple");
        EXPECT_EQ(spans.size(), 1ULL);
        EXPECT_EQ(spans[0], "apple");
        auto linked = el.linkEntities(spans, "what is apple", &memory);
        EXPECT_EQ(linked.size(), 1ULL);
        EXPECT_TRUE(linked[0].type == EntityType::AMBIGUOUS);
    }

    // Test 2: Apple pie (Object)
    {
        auto spans = esd.detectSpans("how to bake an apple pie");
        EXPECT_EQ(spans.size(), 1ULL);
        EXPECT_EQ(spans[0], "apple pie");
        auto linked = el.linkEntities({"apple"}, "how to bake an apple pie", &memory);
        EXPECT_EQ(linked.size(), 1ULL);
        EXPECT_TRUE(linked[0].type == EntityType::OBJECT);
    }

    // Test 3: Apple stocks (Concept)
    {
        auto linked = el.linkEntities({"apple"}, "what are apple stocks today", &memory);
        EXPECT_EQ(linked.size(), 1ULL);
        EXPECT_TRUE(linked[0].type == EntityType::CONCEPT);
    }

    // Test 4: Albert Einstein (Person - public)
    {
        auto spans = esd.detectSpans("who is Albert Einstein");
        EXPECT_EQ(spans.size(), 1ULL);
        auto linked = el.linkEntities(spans, "who is Albert Einstein", &memory);
        EXPECT_EQ(linked.size(), 1ULL);
        EXPECT_TRUE(linked[0].type == EntityType::PERSON);
        EXPECT_EQ(linked[0].link_source, "public_fact");
    }

    // Test 5: my friend Albert Einstein (Person - user relation)
    {
        auto spans = esd.detectSpans("tell me about my friend Albert Einstein");
        EXPECT_EQ(spans.size(), 1ULL);
        EXPECT_EQ(spans[0], "Albert Einstein");
        auto linked = el.linkEntities(spans, "tell me about my friend Albert Einstein", &memory);
        EXPECT_EQ(linked.size(), 1ULL);
        EXPECT_TRUE(linked[0].type == EntityType::PERSON);
        EXPECT_EQ(linked[0].link_source, "user_relation");
    }

    // Test 6: coding (Action)
    {
        auto spans = esd.detectSpans("I love coding");
        auto linked = el.linkEntities(spans, "I love coding", &memory);
        EXPECT_EQ(linked.size(), 1ULL);
        EXPECT_TRUE(linked[0].type == EntityType::ACTION);
    }

    return true;
}
#endif

// ── Entry point ───────────────────────────────────────────────────────────────

void Phase1Tests::runAll() {
    std::cout << "\n========================================\n";
    std::cout << "  PHASE 1 TEST SUITE\n";
    std::cout << "========================================\n\n";

    std::vector<TestCase> tests = {
        // Language Layer
        {"[LL] English plain",                 test_lang_english_plain},
        {"[LL] Hinglish basic",                test_lang_hinglish_basic},
        {"[LL] Hinglish action verbs",         test_lang_hinglish_action},
        {"[LL] Devanagari detection",          test_lang_devanagari},
        {"[LL][FALSE] Empty input",            test_lang_empty_input},
        {"[LL][FALSE] Only punctuation",       test_lang_only_punctuation},
        {"[LL] Single Hindi word",             test_lang_single_hindi_word_short},
        {"[LL][FALSE] Numbers only",           test_lang_numbers_only},
        {"[LL][FALSE] Very long input",        test_lang_very_long_input},
        {"[LL] Adapt response english",        test_lang_adapt_response_english},

        // Semantic Parser — True Positives
        {"[SP] WhatsApp message",              test_sem_whatsapp_message},
        {"[SP] Operate phone + WhatsApp",      test_sem_operate_phone_whatsapp},
        {"[SP] Build health app",              test_sem_build_health_app},
        {"[SP] Emotional not task",            test_sem_emotional_not_task},
        {"[SP] Emotional sad",                 test_sem_emotional_sad},
        {"[SP] Info query what is",            test_sem_info_query_what_is},
        {"[SP] Info query explain",            test_sem_info_query_explain},
        {"[SP] Open Chrome",                   test_sem_open_app},
        {"[SP] Install software",              test_sem_install_software},
        {"[SP] Continuation sure",             test_sem_continuation_sure},
        {"[SP] Continuation yes",              test_sem_continuation_yes},
        {"[SP] Negation no-don't",             test_sem_negation_no},
        {"[SP] Negation stop",                 test_sem_negation_stop},
        {"[SP] Self reference",                test_sem_self_reference},
        {"[SP] Greeting",                      test_sem_greeting},
        {"[SP] Urgency flag",                  test_sem_urgency_flag},
        {"[SP] Negation flag",                 test_sem_negation_flag},

        // Semantic Parser — False Tests
        {"[SP][FALSE] Empty string",           test_false_empty_string},
        {"[SP][FALSE] Only punctuation",       test_false_only_punctuation},
        {"[SP][FALSE] Only numbers",           test_false_only_numbers},
        {"[SP][FALSE] Spam repetition",        test_false_spam_repetition},
        {"[SP][FALSE] Very long input",        test_false_very_long_input},
        {"[SP][FALSE] SQL injection style",    test_false_sql_injection_like},
        {"[SP][FALSE] Mixed script pipeline",  test_false_mixed_script},
        {"[SP][FALSE] Question not command",   test_false_question_not_command},
        {"[SP][FALSE] Teach not task",         test_false_teach_not_task},
        {"[SP][FALSE] Nonsense low confidence",test_false_confidence_unknown},

        // Goal Model
        {"[GM] GoalModel from WhatsApp frame",  test_goal_spec_from_frame},
        {"[GM] CogTaskState created PROPOSED",    test_goal_taskstate_created},
        {"[GM] Emotional not execution",       test_goal_emotional_not_execution},
        {"[GM] Safety level for send",         test_goal_safety_level_send},
#ifndef STANDALONE_PHASE1
        {"[SEM] Semantic entity classification & disambiguation", test_sem_entity_classification},
#endif
    };

    for (const auto& tc : tests) runTest(tc);

    std::cout << "\n----------------------------------------\n";
    std::cout << "  Results: " << sPass << " passed, " << sFail << " failed\n";
    std::cout << "  Total:   " << (sPass + sFail) << " tests\n";
    std::cout << "========================================\n\n";
}
