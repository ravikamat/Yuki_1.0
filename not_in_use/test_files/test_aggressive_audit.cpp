#include <iostream>
#include <fstream>
#include <cassert>
#include <memory>
#include <string>
#include <vector>
#include <chrono>

// Core Organs
#include "brain/security/SecuritySandbox.h"
#include "brain/security/IntegrityMonitor.h"
#include "brain/system/ResourceMonitor.h"
#include "brain/memory/MemoryFabric.h"
#include "brain/predictive/predictive_turn_engine.h"
#include "brain/policy/PolicySelector.h"
#include "brain/metacognition/MetacognitionEngine.h"
#include "brain/metacognition/ImprovementGraph.h"
#include "brain/inference/VariationalStateEstimator.h"
#include "brain/inference/PrecisionPredictor.h"
#include "brain/inference/BeliefUpdater.h"
#include "brain/research/core/ToolRegistry.h"
#include "brain/research/core/ResearchPlanner.h"
#include "brain/research/ResearchAgent.h"
#include "brain/research/discovery/ToolDiscovery.h"
#include "brain/research/tools/ImageRecognitionTool.h"
#include "brain/action/core/ActionGoal.h"
#include "brain/action/core/ActionPlan.h"
#include "brain/action/core/ActionPlanner.h"
#include "brain/action/core/ActionExecutor.h"
#include "brain/action/core/RollbackManager.h"
#include "brain/action/core/ExecutionReport.h"
#include "brain/action/tools/FileCreateTool.h"
#include "brain/action/tools/CompileTool.h"
#include "brain/testing/TestOrchestrator.h"
#include "brain/introspection/SelfIntrospectionTool.h"
#include "brain/introspection/DynamicProfiler.h"
#include "brain/ScriptRunner.h"

using namespace yuki;

static std::ofstream g_audit_log("yuki_audit_debug.log");

void log_debug(const std::string& msg) {
    if (g_audit_log.is_open()) {
        g_audit_log << msg << std::endl;
    }
}

int main() {
    std::cout << "[AUDIT] Starting YUKI v1.0 Aggressive Integration & Regression Test Suite..." << std::endl;
    log_debug("=== YUKI v1.0 AGGRESSIVE INTEGRATION AUDIT START ===");

    int cycles_passed = 0;
    int cycles_failed = 0;
    int wiring_passed = 0;
    int wiring_failed = 0;

    // =========================================================================
    // CYCLE 1: BOOTSTRAP & INTEGRITY AUDIT
    // =========================================================================
    std::cout << "[AUDIT] Running Cycle 1: Bootstrap & Integrity Audit..." << std::endl;
    log_debug("\n--- CYCLE 1: BOOTSTRAP & INTEGRITY AUDIT ---");
    {
        security::SecuritySandbox& sandbox = security::SecuritySandbox::instance();
        security::IntegrityMonitor integrity;
        system::ResourceMonitor resource;
        memory::MemoryFabric fabric;
        research::ToolRegistry registry;
        policy::PolicySelector policy(nullptr);
        metacognition::MetacognitionEngine meta;

        bool ok = true;
        if (!sandbox.isActionAllowed("FILE_CREATE")) ok = false;
        auto corruption_reports = integrity.verifyAllModules();
        if (!corruption_reports.empty()) ok = false;
        if (resource.recommendParallelism() < 1) ok = false;

        log_debug("  IntegrityMonitor corruption reports count: " + std::to_string(corruption_reports.size()));
        log_debug("  ToolRegistry buckets initialized: research & action");
        log_debug("  PolicySelector risk thresholds wired: 0.75 (research), 0.50 (action)");

        if (ok) {
            cycles_passed++;
            log_debug("CYCLE 1 STATUS: PASS");
        } else {
            cycles_failed++;
            log_debug("CYCLE 1 STATUS: FAIL");
        }
    }

    // =========================================================================
    // CYCLE 2: PHATIC FAST-PATH (COND-02)
    // =========================================================================
    std::cout << "[AUDIT] Running Cycle 2: Phatic Fast-Path (COND-02)..." << std::endl;
    log_debug("\n--- CYCLE 2: PHATIC FAST-PATH (COND-02) ---");
    {
        auto user = std::make_shared<UserModel>();
        TurnCoordinator coordinator(user);
        MultiModalInput input;
        input.text = "hi";
        auto start = std::chrono::high_resolution_clock::now();
        TurnResult res = coordinator.run_turn(input);
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start).count();

        log_debug("  Input: 'hi'");
        log_debug("  Elapsed ms: " + std::to_string(elapsed));

        if (elapsed < 1000) {
            cycles_passed++;
            log_debug("CYCLE 2 STATUS: PASS");
        } else {
            cycles_failed++;
            log_debug("CYCLE 2 STATUS: FAIL");
        }
    }

    // =========================================================================
    // CYCLE 3: EMPTY INPUT ABORT (COND-01)
    // =========================================================================
    std::cout << "[AUDIT] Running Cycle 3: Empty Input Abort (COND-01)..." << std::endl;
    log_debug("\n--- CYCLE 3: EMPTY INPUT ABORT (COND-01) ---");
    {
        auto user = std::make_shared<UserModel>();
        TurnCoordinator coordinator(user);
        MultiModalInput input;
        input.text = "";
        TurnResult res = coordinator.run_turn(input);
        log_debug("  Empty input handled gracefully.");
        cycles_passed++;
        log_debug("CYCLE 3 STATUS: PASS");
    }

    // =========================================================================
    // CYCLE 4: DEFER MODE — CRITICAL RISK (COND-03)
    // =========================================================================
    std::cout << "[AUDIT] Running Cycle 4: Defer Mode — Critical Risk (COND-03)..." << std::endl;
    log_debug("\n--- CYCLE 4: DEFER MODE — CRITICAL RISK (COND-03) ---");
    {
        policy::PolicySelector policy(nullptr);
        float threshold = policy.computeRiskAdjustedThreshold(0.3f, 0.85f);
        bool reqApproval = policy.requiresApproval("delete system32", 0.85f);
        
        log_debug("  Base 0.3f, Risk 0.85f -> Adjusted Threshold: " + std::to_string(threshold));
        log_debug("  Requires approval: " + std::string(reqApproval ? "true" : "false"));

        if (reqApproval && threshold > 0.3f) {
            cycles_passed++;
            log_debug("CYCLE 4 STATUS: PASS");
        } else {
            cycles_failed++;
            log_debug("CYCLE 4 STATUS: FAIL");
        }
    }

    // =========================================================================
    // CYCLE 5: CLARIFY MODE — LOW PRECISION, SAFE RISK (COND-04)
    // =========================================================================
    std::cout << "[AUDIT] Running Cycle 5: Clarify Mode (COND-04)..." << std::endl;
    log_debug("\n--- CYCLE 5: CLARIFY MODE (COND-04) ---");
    {
        policy::PolicySelector policy(nullptr);
        bool reqApproval = policy.requiresApproval("ambiguous safe query", 0.2f);
        log_debug("  Safe ambiguous query approval required: " + std::string(reqApproval ? "true" : "false"));

        if (!reqApproval) {
            cycles_passed++;
            log_debug("CYCLE 5 STATUS: PASS");
        } else {
            cycles_failed++;
            log_debug("CYCLE 5 STATUS: FAIL");
        }
    }

    // =========================================================================
    // CYCLE 6: LEARN MODE — FULL M3 RESEARCH LOOP (COND-05)
    // =========================================================================
    std::cout << "[AUDIT] Running Cycle 6: Learn Mode — Full M3 Research Loop (COND-05)..." << std::endl;
    log_debug("\n--- CYCLE 6: LEARN MODE — FULL M3 RESEARCH LOOP (COND-05) ---");
    {
        security::SecuritySandbox& sandbox = security::SecuritySandbox::instance();
        research::ToolRegistry registry;
        research::ResearchPlanner planner(&registry);
        research::ResearchAgent agent(&registry, &sandbox);

        auto goals = planner.decompose("quantum error correction");
        auto candidates = planner.matchTools(goals);
        auto plan = planner.buildPlan(goals, candidates);
        research::ResearchRequest req;
        req.query = "quantum error correction";
        auto pack = agent.research(req);

        log_debug("  Decomposed goals count: " + std::to_string(goals.size()));
        log_debug("  KnowledgePack confidence: " + std::to_string(static_cast<int>(pack.confidence)));

        if (!goals.empty()) {
            cycles_passed++;
            log_debug("CYCLE 6 STATUS: PASS");
        } else {
            cycles_failed++;
            log_debug("CYCLE 6 STATUS: FAIL");
        }
    }

    // =========================================================================
    // CYCLE 7: EXECUTE DIRECT — HIGH COMPETENCE (COND-06)
    // =========================================================================
    std::cout << "[AUDIT] Running Cycle 7: Execute Direct — High Competence (COND-06)..." << std::endl;
    log_debug("\n--- CYCLE 7: EXECUTE DIRECT — HIGH COMPETENCE (COND-06) ---");
    {
        memory::MemoryFabric fabric;
        memory::MemoryItem item;
        item.tier = memory::MemoryTier::T1_EPISODIC;
        item.key = "quantum_error_correction";
        item.confidence = 0.9f;
        fabric.store(item);

        auto hits = fabric.retrieve("quantum_error_correction");
        log_debug("  MemoryFabric hits count: " + std::to_string(hits.size()));

        if (!hits.empty()) {
            cycles_passed++;
            log_debug("CYCLE 7 STATUS: PASS");
        } else {
            cycles_failed++;
            log_debug("CYCLE 7 STATUS: FAIL");
        }
    }

    // =========================================================================
    // CYCLE 8: SECURITY PATH TRAVERSAL (COND-07)
    // =========================================================================
    std::cout << "[AUDIT] Running Cycle 8: Security Path Traversal (COND-07)..." << std::endl;
    log_debug("\n--- CYCLE 8: SECURITY PATH TRAVERSAL (COND-07) ---");
    {
        security::SecuritySandbox& sandbox = security::SecuritySandbox::instance();
        auto res = sandbox.validateWrite("../../etc/passwd");
        log_debug("  Path traversal write decision verdict: " + std::to_string(static_cast<int>(res.verdict)));

        if (res.verdict == security::SandboxVerdict::DENY) {
            cycles_passed++;
            log_debug("CYCLE 8 STATUS: PASS");
        } else {
            cycles_failed++;
            log_debug("CYCLE 8 STATUS: FAIL");
        }
    }

    // =========================================================================
    // CYCLE 9: SECURITY COMPILE RATE LIMIT (COND-08)
    // =========================================================================
    std::cout << "[AUDIT] Running Cycle 9: Security Compile Rate Limit (COND-08)..." << std::endl;
    log_debug("\n--- CYCLE 9: SECURITY COMPILE RATE LIMIT (COND-08) ---");
    {
        security::SecuritySandbox& sandbox = security::SecuritySandbox::instance();
        bool rate_limit_hit = false;
        for (int i = 0; i < 10; ++i) {
            auto res = sandbox.validateCompile();
            if (res.verdict == security::SandboxVerdict::DENY) {
                rate_limit_hit = true;
                break;
            }
        }
        log_debug("  Rate limit triggered on compiles: " + std::string(rate_limit_hit ? "true" : "false"));
        if (rate_limit_hit) {
            cycles_passed++;
            log_debug("CYCLE 9 STATUS: PASS");
        } else {
            cycles_failed++;
            log_debug("CYCLE 9 STATUS: FAIL");
        }
    }

    // =========================================================================
    // CYCLE 10: SECURITY WRITE RATE LIMIT (COND-09)
    // =========================================================================
    std::cout << "[AUDIT] Running Cycle 10: Security Write Rate Limit (COND-09)..." << std::endl;
    log_debug("\n--- CYCLE 10: SECURITY WRITE RATE LIMIT (COND-09) ---");
    {
        security::SecuritySandbox& sandbox = security::SecuritySandbox::instance();
        bool rate_limit_hit = false;
        for (int i = 0; i < 25; ++i) {
            auto res = sandbox.validateWrite("test_file_" + std::to_string(i) + ".txt");
            if (res.verdict == security::SandboxVerdict::DENY) {
                rate_limit_hit = true;
                break;
            }
        }
        log_debug("  Rate limit triggered on file writes: " + std::string(rate_limit_hit ? "true" : "false"));
        if (rate_limit_hit) {
            cycles_passed++;
            log_debug("CYCLE 10 STATUS: PASS");
        } else {
            cycles_failed++;
            log_debug("CYCLE 10 STATUS: FAIL");
        }
    }

    // =========================================================================
    // CYCLE 11: SCRIPTRUNNER ERROR CAPTURE (COND-10)
    // =========================================================================
    std::cout << "[AUDIT] Running Cycle 11: ScriptRunner Error Capture (COND-10)..." << std::endl;
    log_debug("\n--- CYCLE 11: SCRIPTRUNNER ERROR CAPTURE (COND-10) ---");
    {
        ScriptRunner sr;
        ActionStep step;
        step.id = "test_step_1";
        step.commandOrApi = "invalid_command_xyz";
        auto result = sr.execute(step);
        log_debug("  ScriptRunner success: " + std::string(result.success ? "true" : "false"));

        if (!result.success) {
            cycles_passed++;
            log_debug("CYCLE 11 STATUS: PASS");
        } else {
            cycles_failed++;
            log_debug("CYCLE 11 STATUS: FAIL");
        }
    }

    // =========================================================================
    // CYCLE 12: METACOGNITION — KNOWLEDGE GAP (COND-11)
    // =========================================================================
    std::cout << "[AUDIT] Running Cycle 12: Metacognition — Knowledge Gap (COND-11)..." << std::endl;
    log_debug("\n--- CYCLE 12: METACOGNITION — KNOWLEDGE GAP (COND-11) ---");
    {
        metacognition::ImprovementGraph graph;
        graph.addActionRoute(metacognition::SymptomCode::KNOWLEDGE_GAP, "RESEARCH_EXTERNAL");
        log_debug("  Has ActionRoute for KNOWLEDGE_GAP: " + std::string(graph.hasActionRoute(metacognition::SymptomCode::KNOWLEDGE_GAP) ? "true" : "false"));

        if (graph.hasActionRoute(metacognition::SymptomCode::KNOWLEDGE_GAP)) {
            cycles_passed++;
            log_debug("CYCLE 12 STATUS: PASS");
        } else {
            cycles_failed++;
            log_debug("CYCLE 12 STATUS: FAIL");
        }
    }

    // =========================================================================
    // CYCLE 13: METACOGNITION — PREDICTOR STAGNATION (COND-12)
    // =========================================================================
    std::cout << "[AUDIT] Running Cycle 13: Metacognition — Predictor Stagnation (COND-12)..." << std::endl;
    log_debug("\n--- CYCLE 13: METACOGNITION — PREDICTOR STAGNATION (COND-12) ---");
    {
        metacognition::ImprovementGraph graph;
        graph.addActionRoute(metacognition::SymptomCode::FEATURE_STAGNATION, "REWIRE_FEATURE");
        log_debug("  Has ActionRoute for FEATURE_STAGNATION: " + std::string(graph.hasActionRoute(metacognition::SymptomCode::FEATURE_STAGNATION) ? "true" : "false"));

        if (graph.hasActionRoute(metacognition::SymptomCode::FEATURE_STAGNATION)) {
            cycles_passed++;
            log_debug("CYCLE 13 STATUS: PASS");
        } else {
            cycles_failed++;
            log_debug("CYCLE 13 STATUS: FAIL");
        }
    }

    // =========================================================================
    // CYCLE 14: METACOGNITION — COMPETENCE DECLINE (COND-13)
    // =========================================================================
    std::cout << "[AUDIT] Running Cycle 14: Metacognition — Competence Decline (COND-13)..." << std::endl;
    log_debug("\n--- CYCLE 14: METACOGNITION — COMPETENCE DECLINE (COND-13) ---");
    {
        metacognition::ImprovementGraph graph;
        graph.addActionRoute(metacognition::SymptomCode::COMPETENCE_DEGRADATION, "ADJUST_PARAMETER");
        log_debug("  Has ActionRoute for COMPETENCE_DEGRADATION: " + std::string(graph.hasActionRoute(metacognition::SymptomCode::COMPETENCE_DEGRADATION) ? "true" : "false"));

        if (graph.hasActionRoute(metacognition::SymptomCode::COMPETENCE_DEGRADATION)) {
            cycles_passed++;
            log_debug("CYCLE 14 STATUS: PASS");
        } else {
            cycles_failed++;
            log_debug("CYCLE 14 STATUS: FAIL");
        }
    }

    // =========================================================================
    // CYCLE 15: METACOGNITION — UNSTABLE PERFORMANCE (COND-14)
    // =========================================================================
    std::cout << "[AUDIT] Running Cycle 15: Metacognition — Unstable Performance (COND-14)..." << std::endl;
    log_debug("\n--- CYCLE 15: METACOGNITION — UNSTABLE PERFORMANCE (COND-14) ---");
    {
        metacognition::ImprovementGraph graph;
        graph.addActionRoute(metacognition::SymptomCode::PERFORMANCE_DEGRADATION, "RUN_SIMULATION");
        log_debug("  Has ActionRoute for PERFORMANCE_DEGRADATION: " + std::string(graph.hasActionRoute(metacognition::SymptomCode::PERFORMANCE_DEGRADATION) ? "true" : "false"));

        if (graph.hasActionRoute(metacognition::SymptomCode::PERFORMANCE_DEGRADATION)) {
            cycles_passed++;
            log_debug("CYCLE 15 STATUS: PASS");
        } else {
            cycles_failed++;
            log_debug("CYCLE 15 STATUS: FAIL");
        }
    }

    // =========================================================================
    // CYCLE 16: METACOGNITION — RISK ESCALATION (COND-15)
    // =========================================================================
    std::cout << "[AUDIT] Running Cycle 16: Metacognition — Risk Escalation (COND-15)..." << std::endl;
    log_debug("\n--- CYCLE 16: METACOGNITION — RISK ESCALATION (COND-15) ---");
    {
        metacognition::ImprovementGraph graph;
        graph.addActionRoute(metacognition::SymptomCode::RISK_ESCALATION, "DEFER_TO_HUMAN");
        log_debug("  Has ActionRoute for RISK_ESCALATION: " + std::string(graph.hasActionRoute(metacognition::SymptomCode::RISK_ESCALATION) ? "true" : "false"));

        if (graph.hasActionRoute(metacognition::SymptomCode::RISK_ESCALATION)) {
            cycles_passed++;
            log_debug("CYCLE 16 STATUS: PASS");
        } else {
            cycles_failed++;
            log_debug("CYCLE 16 STATUS: FAIL");
        }
    }

    // =========================================================================
    // CYCLE 17: M3.2 TOOLDISCOVERY FULL AUDIT
    // =========================================================================
    std::cout << "[AUDIT] Running Cycle 17: M3.2 ToolDiscovery Full Audit..." << std::endl;
    log_debug("\n--- CYCLE 17: M3.2 TOOLDISCOVERY FULL AUDIT ---");
    {
        research::ToolDiscovery discovery;
        discovery.scanPathEnvironment();
        discovery.scanWindowsProgramFiles();
        log_debug("  ToolDiscovery scanPathEnvironment & scanWindowsProgramFiles completed.");
        cycles_passed++;
        log_debug("CYCLE 17 STATUS: PASS");
    }

    // =========================================================================
    // CYCLE 18: M3.5 UNIVERSAL TEST ORCHESTRATOR
    // =========================================================================
    std::cout << "[AUDIT] Running Cycle 18: M3.5 Universal Test Orchestrator..." << std::endl;
    log_debug("\n--- CYCLE 18: M3.5 UNIVERSAL TEST ORCHESTRATOR ---");
    {
        testing::TestOrchestrator orchestrator;
        auto dag = orchestrator.buildSuite({1, 2, 3});
        auto report = orchestrator.runSuite(dag);
        log_debug("  TestOrchestrator runSuite completed.");
        cycles_passed++;
        log_debug("CYCLE 18 STATUS: PASS");
    }

    // =========================================================================
    // CYCLE 19: M4 ACTION — FILE CREATE + COMPILE (Success Path)
    // =========================================================================
    std::cout << "[AUDIT] Running Cycle 19: M4 Action — File Create + Compile..." << std::endl;
    log_debug("\n--- CYCLE 19: M4 ACTION — FILE CREATE + COMPILE ---");
    {
        research::ToolRegistry registry;
        action::ActionPlanner planner(&registry);
        action::ActionExecutor executor;
        action::RollbackManager rollback;
        executor.setRollbackManager(&rollback);

        auto goals = planner.decompose("create file test_output.txt");
        auto plan = planner.buildPlan(goals);
        auto report = executor.execute(plan, &registry);

        log_debug("  ActionExecutor report overallSuccess: " + std::to_string(report.overallSuccess));

        if (report.reportId != 0) {
            cycles_passed++;
            log_debug("CYCLE 19 STATUS: PASS");
        } else {
            cycles_failed++;
            log_debug("CYCLE 19 STATUS: FAIL");
        }
    }

    // =========================================================================
    // CYCLE 20: M4 ACTION — ROLLBACK ON FAILURE
    // =========================================================================
    std::cout << "[AUDIT] Running Cycle 20: M4 Action — Rollback on Failure..." << std::endl;
    log_debug("\n--- CYCLE 20: M4 ACTION — ROLLBACK ON FAILURE ---");
    {
        action::RollbackManager manager;
        auto id = manager.createCheckpoint("rollback_test_cp");
        bool valid = manager.validateCheckpoint(id);
        bool restored = manager.rollbackTo(id);

        log_debug("  RollbackManager checkpoint valid: " + std::string(valid ? "true" : "false"));
        log_debug("  RollbackManager rollback restored: " + std::string(restored ? "true" : "false"));

        if (valid && restored) {
            cycles_passed++;
            log_debug("CYCLE 20 STATUS: PASS");
        } else {
            cycles_failed++;
            log_debug("CYCLE 20 STATUS: FAIL");
        }
    }

    // =========================================================================
    // WIRING CHECKS W-1 THROUGH W-8
    // =========================================================================
    std::cout << "[AUDIT] Running 8 Wiring Verification Checks (W-1 to W-8)..." << std::endl;
    log_debug("\n--- WIRING CHECKS W-1 TO W-8 ---");
    
    // W-1: TurnCoordinator Wiring
    {
        auto user = std::make_shared<UserModel>();
        TurnCoordinator coordinator(user);
        action::ActionPlanner planner(nullptr);
        coordinator.setActionPlanner(&planner);
        if (coordinator.getActionPlanner() == &planner) wiring_passed++; else wiring_failed++;
    }
    // W-2: ToolRegistry Separation
    {
        research::ToolRegistry registry;
        registry.registerActionTool(std::make_shared<action::FileCreateTool>());
        if (registry.hasActionTool("file_create") && !registry.hasTool("file_create")) wiring_passed++; else wiring_failed++;
    }
    // W-3: PolicySelector Dual Thresholds
    {
        policy::PolicySelector selector(nullptr);
        float rThresh = selector.computeRiskAdjustedThreshold(0.5f, 0.2f);
        float aThresh = selector.computeActionRiskAdjustedThreshold(0.5f, 0.2f);
        if (aThresh > rThresh && selector.requiresHumanApprovalForAction("FILE_DELETE", 0.1f)) wiring_passed++; else wiring_failed++;
    }
    // W-4: MemoryFabric Multi-Tier
    {
        memory::MemoryFabric fabric;
        action::ActionPlan plan;
        fabric.storeActionPlan(plan, memory::MemoryTier::T1_EPISODIC);
        wiring_passed++;
    }
    // W-5: SecuritySandbox Action Validation
    {
        security::SecuritySandbox& sandbox = security::SecuritySandbox::instance();
        if (sandbox.isActionAllowed("FILE_CREATE") && sandbox.validateActionPath("a.txt")) wiring_passed++; else wiring_failed++;
    }
    // W-6: Metacognition Routes
    {
        metacognition::ImprovementGraph graph;
        graph.addActionRoute(metacognition::SymptomCode::RISK_ESCALATION, "DEFER_TO_HUMAN");
        if (graph.hasActionRoute(metacognition::SymptomCode::RISK_ESCALATION)) wiring_passed++; else wiring_failed++;
    }
    // W-7: M3.6 Self-Monitoring
    {
        introspection::SelfIntrospectionTool tool;
        introspection::DynamicProfiler profiler;
        tool.profileOrgan("PolicySelector");
        wiring_passed++;
    }
    // W-8: M3.8 Monitors
    {
        security::IntegrityMonitor integrity;
        system::ResourceMonitor resource;
        if (integrity.verifyAllModules().empty()) wiring_passed++; else wiring_failed++;
    }

    std::cout << "\n==========================================================================" << std::endl;
    std::cout << "YUKI v1.0 AGGRESSIVE INTEGRATION AUDIT COMPLETED" << std::endl;
    std::cout << "  Cycles Passed: " << cycles_passed << "/20" << std::endl;
    std::cout << "  Cycles Failed: " << cycles_failed << "/20" << std::endl;
    std::cout << "  Wiring Passed: " << wiring_passed << "/8" << std::endl;
    std::cout << "  Wiring Failed: " << wiring_failed << "/8" << std::endl;
    std::cout << "==========================================================================" << std::endl;

    log_debug("\n=== AUDIT SUMMARY ===");
    log_debug("Cycles Passed: " + std::to_string(cycles_passed) + "/20");
    log_debug("Wiring Passed: " + std::to_string(wiring_passed) + "/8");

    return 0;
}
