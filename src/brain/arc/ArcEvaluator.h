#pragma once

#include "ArcTask.h"
#include "ArcSolver.h"
#include <vector>
#include <string>
#include <chrono>
#include <numeric>
#include <algorithm>

namespace yuki {
namespace brain {
namespace arc {

struct EvaluationResult {
    std::string task_id;
    size_t test_idx;
    bool correct = false;
    float pixel_accuracy = 0.0f;
    float io_accuracy = 0.0f;
    std::chrono::microseconds solve_time;
    std::string solver_name;
    std::string error_message;
    
    EvaluationResult() = default;
};

inline float pixelAccuracy(const Grid& pred, const Grid& target) {
    if (!sameShape(pred, target)) return 0.0f;
    
    size_t total = 0;
    size_t correct = 0;
    for (size_t i = 0; i < pred.size(); ++i) {
        for (size_t j = 0; j < pred[i].size(); ++j) {
            total++;
            if (pred[i][j] == target[i][j]) correct++;
        }
    }
    return total > 0 ? static_cast<float>(correct) / total : 0.0f;
}

inline float ioAccuracy(const Grid& pred, const Grid& target) {
    return gridsEqual(pred, target) ? 1.0f : 0.0f;
}

inline EvaluationResult evaluatePrediction(const Task& task, size_t test_idx, 
                                           const Grid& prediction, 
                                           const SolveResult& solve_result) {
    EvaluationResult result;
    result.task_id = task.id;
    result.test_idx = test_idx;
    result.solve_time = solve_result.solve_time;
    result.solver_name = solve_result.predictions.empty() ? "none" : "solver";
    result.error_message = solve_result.error_message;
    
    if (!task.hasTestOutput(test_idx)) {
        result.correct = false;
        result.pixel_accuracy = -1.0f;
        result.io_accuracy = -1.0f;
        return result;
    }
    
    const Grid& target = task.test[test_idx].output;
    result.pixel_accuracy = pixelAccuracy(prediction, target);
    result.io_accuracy = ioAccuracy(prediction, target);
    result.correct = result.io_accuracy > 0.5f;
    
    return result;
}

struct AggregateMetrics {
    size_t total_tasks = 0;
    size_t solved_tasks = 0;
    size_t correct_tasks = 0;
    float mean_pixel_accuracy = 0.0f;
    float mean_io_accuracy = 0.0f;
    std::chrono::microseconds total_time{0};
    float tasks_per_second = 0.0f;
    
    void add(const EvaluationResult& r) {
        total_tasks++;
        if (!r.error_message.empty()) return;
        solved_tasks++;
        if (r.correct) correct_tasks++;
        mean_pixel_accuracy = (mean_pixel_accuracy * (solved_tasks - 1) + r.pixel_accuracy) / solved_tasks;
        mean_io_accuracy = (mean_io_accuracy * (solved_tasks - 1) + r.io_accuracy) / solved_tasks;
        total_time += r.solve_time;
    }
    
    void finalize() {
        if (total_time.count() > 0) {
            tasks_per_second = static_cast<float>(solved_tasks) / (total_time.count() / 1e6f);
        }
    }
    
    std::string toString() const {
        std::string result;
        result += "=== Aggregate Metrics ===\n";
        result += "Total tasks: " + std::to_string(total_tasks) + "\n";
        result += "Solved: " + std::to_string(solved_tasks) + "\n";
        result += "Correct: " + std::to_string(correct_tasks) + "\n";
        result += "Accuracy (IO): " + std::to_string(mean_io_accuracy * 100) + "%\n";
        result += "Accuracy (Pixel): " + std::to_string(mean_pixel_accuracy * 100) + "%\n";
        result += "Total time: " + std::to_string(total_time.count() / 1e6) + "s\n";
        result += "Throughput: " + std::to_string(tasks_per_second) + " tasks/s\n";
        return result;
    }
};

class ArcEvaluator {
public:
    ArcEvaluator() = default;
    
    explicit ArcEvaluator(std::vector<std::unique_ptr<ArcSolver>>&& solvers)
        : solvers_(std::move(solvers)) {}
    
    void addSolver(std::unique_ptr<ArcSolver> solver) {
        solvers_.push_back(std::move(solver));
    }
    
    void addSolvers(std::vector<std::unique_ptr<ArcSolver>>&& solvers) {
        for (auto& s : solvers) {
            solvers_.push_back(std::move(s));
        }
    }
    
    std::vector<EvaluationResult> evaluate(const std::vector<Task>& tasks, 
                                           size_t max_tests_per_task = 1) {
        std::vector<EvaluationResult> results;
        results.reserve(tasks.size() * max_tests_per_task);
        
        for (const auto& task : tasks) {
            size_t tests_to_run = std::min(max_tests_per_task, task.numTest());
            for (size_t test_idx = 0; test_idx < tests_to_run; ++test_idx) {
                for (auto& solver : solvers_) {
                    auto solve_result = solver->solve(task, test_idx);
                    if (!solve_result.predictions.empty()) {
                        results.push_back(evaluatePrediction(task, test_idx, 
                                                            solve_result.predictions[0], 
                                                            solve_result));
                        break;
                    }
                }
            }
        }
        return results;
    }
    
    AggregateMetrics computeMetrics(const std::vector<EvaluationResult>& results) {
        AggregateMetrics metrics;
        for (const auto& r : results) {
            metrics.add(r);
        }
        metrics.finalize();
        return metrics;
    }
    
    std::string runEvaluation(const std::vector<Task>& tasks, 
                              size_t max_tests_per_task = 1) {
        auto results = evaluate(tasks, max_tests_per_task);
        auto metrics = computeMetrics(results);
        return metrics.toString();
    }
    
private:
    std::vector<std::unique_ptr<ArcSolver>> solvers_;
};

} // namespace arc
} // namespace brain
} // namespace yuki