#include <iostream>
#include <cassert>
#include <filesystem>
#include <fstream>
#include "brain/ExecutionTypes.h"
#include "brain/VerificationEngine.h"
#include "brain/SystemExecutor.h"
#include "brain/ScriptRunner.h"
#include "brain/FileOperator.h"
#include "brain/DependencyInstaller.h"
#include "brain/ResponseActPlanner.h"
#include "brain/MotherCore.h"

int main() {
    std::cout << "--- Starting Executor Pack 1 Tests ---\n";

    // Test 1 — ApprovalGatedPlanReturnsPendingApproval
    {
        ExecutionPlan plan;
        plan.requiresApproval = true;
        plan.approvalTypes.push_back(ApprovalType::EXECUTE_STATE_CHANGE);
        
        VerificationEngine engine;
        auto vb = engine.buildPendingApprovalResult(plan);
        
        assert(vb.pendingApproval == true);
        assert(vb.success == false);
        assert(vb.approval.required == true);
        std::cout << "Test 1 Passed.\n";
    }

    // Test 2 — SystemExecutorStopsWhenApprovalRequired
    {
        ExecutionPlan plan;
        plan.requiresApproval = true;
        
        SystemExecutor executor;
        auto result = executor.run(plan);
        
        assert(result.success == false);
        assert(result.steps.empty() == true);
        std::cout << "Test 2 Passed.\n";
    }

    // Test 3 — ScriptRunnerDispatchesKnownCommands
    {
        ScriptRunner runner;
        ActionStep step;
        step.id = "step1";
        step.commandOrApi = "powershell";
        step.args["script"] = "echo 'test'";
        
        auto result = runner.execute(step);
        assert(result.stepId == step.id);
        
        step.commandOrApi = "python";
        step.args["script"] = "print('test')";
        result = runner.execute(step);
        assert(result.stepId == step.id);
        
        step.commandOrApi = "invalid_cmd";
        result = runner.execute(step);
        assert(result.success == false);
        
        std::cout << "Test 3 Passed.\n";
    }

    // Test 4 — FileOperatorSafeDeleteMovesToReview
    {
        std::filesystem::path testPath = "test_temp_file.txt";
        std::ofstream out(testPath);
        out << "temp";
        out.close();
        
        ActionStep step;
        step.commandOrApi = "delete";
        step.args["path"] = testPath.string();
        
        FileOperator fOp;
        auto result = fOp.execute(step);
        
        assert(!std::filesystem::exists(testPath));
        // Note: the exact review path might have a timestamp, but the root is data/review/
        std::filesystem::path reviewRoot = std::filesystem::current_path() / "data" / "review";
        assert(std::filesystem::exists(reviewRoot));
        assert(result.success == true);
        
        std::cout << "Test 4 Passed.\n";
    }

    // Test 5 — DependencyInstallerApproval
    {
        DependencyInstaller installer;
        auto results = installer.ensure({"definitely_not_a_real_tool_xyz"});
        
        assert(!results.empty());
        assert(results[0].approvalRequested == true);
        assert(results[0].alreadyInstalled == false);
        
        std::cout << "Test 5 Passed.\n";
    }

    // Test 6 — ResponseActPlannerApprovalRequest
    {
        VerificationBundle vb;
        vb.pendingApproval = true;
        vb.approval.summary = "Need permission";
        vb.approval.riskySteps.push_back("Step 1");
        
        ResponseActPlanner planner;
        auto plan = planner.build(MeaningState{}, FactBundle{}, vb);
        
        assert(plan.act == ResponseAct::APPROVAL_REQUEST);
        assert(plan.requiresUserReply == true);
        assert(!plan.riskySteps.empty());
        
        std::cout << "Test 6 Passed.\n";
    }

    // Test 7 — MotherCoreSynthesizesApprovalText
    {
        MotherCore core;
        // Simulating the pipeline behavior: a command to build an android app
        auto result = core.handleInput("build an android app");
        
        assert(!result.finalText.empty());
        // Verify it doesn't just print raw structs
        assert(result.finalText.find("requiresApproval") == std::string::npos);
        assert(result.finalText.find("pendingApproval") == std::string::npos);
        
        std::cout << "Test 7 Passed.\n";
    }

    std::cout << "--- All Executor Pack 1 Tests Passed ---\n";
    return 0;
}
