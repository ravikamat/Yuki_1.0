#include "brain/arc/ArcTypes.h"
#include "brain/arc/ArcTask.h"
#include "brain/arc/ArcBaselineSolvers.h"
#include "brain/arc/ArcEvaluator.h"
#include <iostream>
#include <vector>
#include <chrono>
#include <fstream>
#include <nlohmann/json.hpp>

using namespace yuki::brain::arc;

std::vector<Task> createSampleTasks() {
    std::vector<Task> tasks;
    
    Task task1("sample_1");
    task1.train.emplace_back(
        Grid{{0, 1, 0}, {1, 1, 1}, {0, 1, 0}},
        Grid{{1, 0, 1}, {0, 0, 0}, {1, 0, 1}}
    );
    task1.train.emplace_back(
        Grid{{2, 2, 2}, {2, 0, 2}, {2, 2, 2}},
        Grid{{0, 0, 0}, {0, 2, 0}, {0, 0, 0}}
    );
    task1.test.emplace_back(
        Grid{{3, 3, 3}, {3, 0, 3}, {3, 3, 3}},
        Grid{{0, 0, 0}, {0, 3, 0}, {0, 0, 0}}
    );
    tasks.push_back(task1);
    
    Task task2("sample_2");
    task2.train.emplace_back(
        Grid{{1, 0, 1}, {0, 1, 0}, {1, 0, 1}},
        Grid{{0, 1, 0}, {1, 0, 1}, {0, 1, 0}}
    );
    task2.test.emplace_back(
        Grid{{2, 0, 2}, {0, 2, 0}, {2, 0, 2}},
        Grid{{0, 2, 0}, {2, 0, 2}, {0, 2, 0}}
    );
    tasks.push_back(task2);
    
    Task task3("sample_3");
    task3.train.emplace_back(
        Grid{{1, 2, 3}, {4, 5, 6}, {7, 8, 9}},
        Grid{{9, 8, 7}, {6, 5, 4}, {3, 2, 1}}
    );
    task3.test.emplace_back(
        Grid{{1, 1, 1}, {2, 2, 2}, {3, 3, 3}},
        Grid{{3, 3, 3}, {2, 2, 2}, {1, 1, 1}}
    );
    tasks.push_back(task3);
    
    Task task4("sample_4");
    task4.train.emplace_back(
        Grid{{0, 0, 1}, {0, 1, 0}, {1, 0, 0}},
        Grid{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}
    );
    task4.train.emplace_back(
        Grid{{2, 0, 0}, {0, 2, 0}, {0, 0, 2}},
        Grid{{0, 0, 2}, {0, 2, 0}, {2, 0, 0}}
    );
    task4.test.emplace_back(
        Grid{{3, 0, 0}, {0, 3, 0}, {0, 0, 3}},
        Grid{{0, 0, 3}, {0, 3, 0}, {3, 0, 0}}
    );
    tasks.push_back(task4);
    
    Task task5("sample_5");
    task5.train.emplace_back(
        Grid{{1, 1, 1}, {1, 0, 1}, {1, 1, 1}},
        Grid{{0, 0, 0}, {0, 1, 0}, {0, 0, 0}}
    );
    task5.test.emplace_back(
        Grid{{2, 2, 2}, {2, 0, 2}, {2, 2, 2}},
        Grid{{0, 0, 0}, {0, 2, 0}, {0, 0, 0}}
    );
    tasks.push_back(task5);
    
    return tasks;
}

std::vector<Task> loadArcDataset(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Could not open dataset: " << path << std::endl;
        return {};
    }
    
    try {
        nlohmann::json json;
        file >> json;
        return parseTasks(json);
    } catch (const std::exception& e) {
        std::cerr << "Failed to parse dataset: " << e.what() << std::endl;
        return {};
    }
}

void runBaselines(const std::vector<Task>& tasks) {
    std::cout << "Running baseline evaluation on " << tasks.size() << " tasks...\n\n";
    
    std::vector<std::unique_ptr<ArcSolver>> solvers;
    solvers.push_back(std::make_unique<IdentitySolver>());
    solvers.push_back(std::make_unique<MostCommonColorSolver>());
    solvers.push_back(std::make_unique<TrainOutputModeSolver>());
    solvers.push_back(std::make_unique<RandomSolver>(42));
    
    for (auto& solver : solvers) {
        std::cout << "--- " << solver->name() << " ---\n";
        ArcEvaluator single_eval;
        single_eval.addSolver(std::make_unique<IdentitySolver>());
        std::string results = single_eval.runEvaluation(tasks, 1);
        std::cout << results << "\n";
    }
    
    std::cout << "=== COMPOSITE SOLVER ===\n";
    CompositeSolver composite;
    composite.addSolver(std::make_unique<IdentitySolver>());
    composite.addSolver(std::make_unique<MostCommonColorSolver>());
    composite.addSolver(std::make_unique<TrainOutputModeSolver>());
    
    ArcEvaluator composite_eval;
    composite_eval.addSolver(std::make_unique<CompositeSolver>(std::move(composite)));
    std::string results = composite_eval.runEvaluation(tasks, 1);
    std::cout << results << "\n";
}

int main(int argc, char** argv) {
    std::vector<Task> tasks;
    
    if (argc > 1) {
        tasks = loadArcDataset(argv[1]);
        if (tasks.empty()) {
            std::cerr << "No tasks loaded, using sample tasks\n";
            tasks = createSampleTasks();
        }
    } else {
        std::cout << "No dataset provided, using sample tasks\n";
        tasks = createSampleTasks();
    }
    
    std::cout << "Loaded " << tasks.size() << " tasks\n";
    for (const auto& task : tasks) {
        std::cout << "  Task " << task.id << ": " << task.numTrain() << " train, " 
                  << task.numTest() << " test\n";
    }
    std::cout << "\n";
    
    runBaselines(tasks);
    
    return 0;
}