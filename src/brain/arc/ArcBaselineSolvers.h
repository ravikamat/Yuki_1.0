#pragma once

#include "ArcSolver.h"
#include "ArcTypes.h"
#include <unordered_map>
#include <random>

namespace yuki {
namespace brain {
namespace arc {

class IdentitySolver : public ArcSolver {
public:
    std::string name() const override { return "IdentitySolver"; }
    
    SolveResult solve(const Task& task, size_t test_idx = 0) override {
        auto start = std::chrono::high_resolution_clock::now();
        
        if (test_idx >= task.test.size()) {
            return SolveResult{{}, {}, false, "Test index out of bounds"};
        }
        
        std::vector<Grid> predictions = {task.test[test_idx].input};
        
        auto end = std::chrono::high_resolution_clock::now();
        return SolveResult(std::move(predictions), 
                          std::chrono::duration_cast<std::chrono::microseconds>(end - start), true);
    }
};

class MostCommonColorSolver : public ArcSolver {
public:
    std::string name() const override { return "MostCommonColorSolver"; }
    
    SolveResult solve(const Task& task, size_t test_idx = 0) override {
        auto start = std::chrono::high_resolution_clock::now();
        
        if (test_idx >= task.test.size()) {
            return SolveResult{{}, {}, false, "Test index out of bounds"};
        }
        
        std::unordered_map<Color, int> color_counts;
        for (const auto& ex : task.train) {
            for (const auto& row : ex.output) {
                for (Color c : row) {
                    color_counts[c]++;
                }
            }
        }
        
        Color most_common = COLOR_BLACK;
        int max_count = 0;
        for (const auto& [color, count] : color_counts) {
            if (count > max_count) {
                max_count = count;
                most_common = color;
            }
        }
        
        const Grid& test_input = task.test[test_idx].input;
        auto [h, w] = getShape(test_input);
        Grid prediction = makeGrid(h, w, most_common);
        
        auto end = std::chrono::high_resolution_clock::now();
        return SolveResult({std::move(prediction)}, 
                          std::chrono::duration_cast<std::chrono::microseconds>(end - start), true);
    }
};

class RandomSolver : public ArcSolver {
public:
    explicit RandomSolver(uint32_t seed = 42) : rng_(seed) {}
    
    std::string name() const override { return "RandomSolver"; }
    
    SolveResult solve(const Task& task, size_t test_idx = 0) override {
        auto start = std::chrono::high_resolution_clock::now();
        
        if (test_idx >= task.test.size()) {
            return SolveResult{{}, {}, false, "Test index out of bounds"};
        }
        
        const Grid& test_input = task.test[test_idx].input;
        auto [h, w] = getShape(test_input);
        Grid prediction = makeGrid(h, w);
        
        std::uniform_int_distribution<int> dist(0, static_cast<int>(NUM_COLORS) - 1);
        for (size_t i = 0; i < h; ++i) {
            for (size_t j = 0; j < w; ++j) {
                prediction[i][j] = static_cast<Color>(dist(rng_));
            }
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        return SolveResult({std::move(prediction)}, 
                          std::chrono::duration_cast<std::chrono::microseconds>(end - start), true);
    }
    
private:
    std::mt19937 rng_;
};

class TrainOutputModeSolver : public ArcSolver {
public:
    std::string name() const override { return "TrainOutputModeSolver"; }
    
    SolveResult solve(const Task& task, size_t test_idx = 0) override {
        auto start = std::chrono::high_resolution_clock::now();
        
        if (test_idx >= task.test.size()) {
            return SolveResult{{}, {}, false, "Test index out of bounds"};
        }
        
        std::unordered_map<std::string, int> output_counts;
        for (const auto& ex : task.train) {
            output_counts[gridToString(ex.output)]++;
        }
        
        std::string most_common_output;
        int max_count = 0;
        for (const auto& [output_str, count] : output_counts) {
            if (count > max_count) {
                max_count = count;
                most_common_output = output_str;
            }
        }
        
        Grid prediction = stringToGrid(most_common_output);
        const Grid& test_input = task.test[test_idx].input;
        auto [h, w] = getShape(test_input);
        
        if (getShape(prediction) != std::make_pair(h, w)) {
            prediction = makeGrid(h, w, COLOR_BLACK);
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        return SolveResult({std::move(prediction)}, 
                          std::chrono::duration_cast<std::chrono::microseconds>(end - start), true);
    }
};

} // namespace arc
} // namespace brain
} // namespace yuki