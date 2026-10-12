#pragma once
// BaselineRunner — Runs baseline methods on COGNI-1 tasks
// Baselines: Structured Fact Store, Dense Embedding Index, Random Policy, Heuristic Policy

#include "GridworldSimulator.h"
#include "BruteForceOracle.h"
#include "MetricsLogger.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <chrono>
#include <random>

namespace yuki {
namespace test {
namespace cogni1 {

// Abstract baseline interface
class Baseline {
public:
    virtual ~Baseline() = default;
    virtual std::string name() const = 0;
    virtual SimResult run(const Task& task, MetricsLogger* logger = nullptr) = 0;
    virtual void reset() {}
};

// Baseline result summary
struct BaselineSummary {
    std::string baseline_name;
    int total_tasks = 0;
    int solved = 0;
    float success_rate = 0.0f;
    float avg_steps = 0.0f;
    float avg_reward = 0.0f;
    float avg_time_ms = 0.0f;
    std::unordered_map<std::string, float> per_type_success;  // task_type -> success_rate
    
    void print() const;
};

// 1. Structured Fact Store Baseline
// Uses HdcSemanticGraph as explicit graph (no VSA reasoning)
class StructuredFactStoreBaseline : public Baseline {
public:
    struct Config {
        int max_steps;
        
        Config() : max_steps(100) {}
    };
    
    explicit StructuredFactStoreBaseline(const Config& cfg = Config());
    std::string name() const override { return "structured_fact_store"; }
    SimResult run(const Task& task, MetricsLogger* logger = nullptr) override;
    void reset() override;
    
private:
    Config config_;
    // Simulated fact store: maps (subject, relation) -> object
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> facts_;
    
    void extractFacts(const WorldState& state);
    std::optional<Action> planAction(const Task& task, const WorldState& state);
    std::vector<std::string> getRelevantFacts(const std::string& query);
};

// 2. Dense Embedding Index Baseline
// Uses simulated encoder embeddings + nearest neighbor
class DenseEmbeddingBaseline : public Baseline {
public:
    struct Config {
        int max_steps;
        int embedding_dim;
        int knn_k;
        
        Config() : max_steps(100), embedding_dim(64), knn_k(5) {}
    };
    
    explicit DenseEmbeddingBaseline(const Config& cfg = Config());
    std::string name() const override { return "dense_embedding"; }
    SimResult run(const Task& task, MetricsLogger* logger = nullptr) override;
    void reset() override;
    
private:
    Config config_;
    std::vector<std::pair<std::vector<float>, WorldState>> memory_;  // (embedding, state)
    std::mt19937 rng_;
    
    std::vector<float> encode(const WorldState& state);
    float cosineSim(const std::vector<float>& a, const std::vector<float>& b);
    std::optional<Action> retrieveAction(const Task& task, const WorldState& state);
};

// 3. Random Policy Baseline
class RandomPolicyBaseline : public Baseline {
public:
    struct Config {
        int max_steps;
        uint32_t seed;
        
        Config() : max_steps(100), seed(42) {}
    };
    
    explicit RandomPolicyBaseline(const Config& cfg = Config());
    std::string name() const override { return "random_policy"; }
    SimResult run(const Task& task, MetricsLogger* logger = nullptr) override;
    void reset() override;
    
private:
    Config config_;
    std::mt19937 rng_;
};

// 4. Heuristic Policy Baseline
// Uses hand-coded rules (like ActiveInferencePolicyEngine with hand rules only)
class HeuristicPolicyBaseline : public Baseline {
public:
    struct Config {
        int max_steps;
        
        Config() : max_steps(100) {}
    };
    
    explicit HeuristicPolicyBaseline(const Config& cfg = Config());
    std::string name() const override { return "heuristic_policy"; }
    SimResult run(const Task& task, MetricsLogger* logger = nullptr) override;
    void reset() override;
    
private:
    Config config_;
    std::optional<Action> heuristicAction(const Task& task, const WorldState& state);
    bool isBlockClear(const WorldState& state, const std::string& block) const;
    Position findBlockPos(const WorldState& state, const std::string& block) const;
};

// Baseline Runner - orchestrates all baselines
class BaselineRunner {
public:
    struct Config {
        std::vector<std::string> enabled_baselines;
        int max_steps_per_task;
        bool log_details;
        
        Config() : enabled_baselines({"structured_fact_store", "dense_embedding", "random_policy", "heuristic_policy"}),
                   max_steps_per_task(100), log_details(true) {}
    };
    
    explicit BaselineRunner(const GridworldSimulator::Config& sim_cfg = GridworldSimulator::Config(),
                            const Config& runner_cfg = Config());
    
    // Run all baselines on a task
    std::unordered_map<std::string, SimResult> runTask(const Task& task, 
                                                        MetricsLogger* logger = nullptr);
    
    // Run all baselines on all tasks
    std::unordered_map<std::string, BaselineSummary> runAll(const std::vector<Task>& tasks,
                                                             MetricsLogger* logger = nullptr);
    
    // Get summaries
    const std::unordered_map<std::string, BaselineSummary>& summaries() const { return summaries_; }
    
    // Print summary table
    void printSummary() const;
    
private:
    GridworldSimulator::Config sim_config_;
    Config config_;
    std::unordered_map<std::string, std::unique_ptr<Baseline>> baselines_;
    std::unordered_map<std::string, BaselineSummary> summaries_;
    
    void initializeBaselines();
    void updateSummary(const std::string& baseline_name, const Task& task, 
                       const SimResult& result, float time_ms);
};

} // namespace cogni1
} // namespace test
} // namespace yuki