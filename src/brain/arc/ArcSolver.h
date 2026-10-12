#pragma once

#include "ArcTask.h"
#include <vector>
#include <memory>
#include <chrono>

namespace yuki {
namespace brain {
namespace arc {

struct SolveResult {
    std::vector<Grid> predictions;
    std::chrono::microseconds solve_time;
    bool success = false;
    std::string error_message;
    
    SolveResult() = default;
    SolveResult(std::vector<Grid> preds, std::chrono::microseconds time, bool ok = true, std::string err = "")
        : predictions(std::move(preds)), solve_time(time), success(ok), error_message(std::move(err)) {}
};

class ArcSolver {
public:
    virtual ~ArcSolver() = default;
    
    virtual SolveResult solve(const Task& task, size_t test_idx = 0) = 0;
    virtual std::string name() const = 0;
    virtual void reset() {}
};

using SolverPtr = std::unique_ptr<ArcSolver>;

class CompositeSolver : public ArcSolver {
public:
    void addSolver(SolverPtr solver) {
        solvers_.push_back(std::move(solver));
    }
    
    SolveResult solve(const Task& task, size_t test_idx = 0) override {
        auto start = std::chrono::high_resolution_clock::now();
        
        for (auto& solver : solvers_) {
            auto result = solver->solve(task, test_idx);
            if (result.success && !result.predictions.empty()) {
                auto end = std::chrono::high_resolution_clock::now();
                result.solve_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
                return result;
            }
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        return SolveResult{{}, std::chrono::duration_cast<std::chrono::microseconds>(end - start), false, "All solvers failed"};
    }
    
    std::string name() const override { return "CompositeSolver"; }
    void reset() override { for (auto& s : solvers_) s->reset(); }
    
private:
    std::vector<SolverPtr> solvers_;
};

} // namespace arc
} // namespace brain
} // namespace yuki