#pragma once
// ClingoStore — ASP-based Knowledge Store using clingo C API
// Provides fact/rule management, query, solve, and constraint checking

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <mutex>
#include <chrono>
#include <memory>
#include <variant>

#ifdef _WIN32
    #define CLINGO_API extern "C" __declspec(dllimport)
#else
    #define CLINGO_API extern "C"
#endif

// Forward declare clingo types (we use opaque pointers)
struct clingo_control;
struct clingo_solve_handle;
struct clingo_model;
struct clingo_symbol;
struct clingo_symbolic_atoms;
struct clingo_ast_builder;

namespace yuki {
namespace brain {
namespace reasoning {

// Forward declaration for use in Reasoner
struct Atom {
    std::string predicate;
    std::vector<std::string> args;

    Atom() = default;
    Atom(const std::string& pred, const std::vector<std::string>& a) : predicate(pred), args(a) {}

    std::string toString() const {
        std::string s = predicate;
        if (!args.empty()) {
            s += "(";
            for (size_t i = 0; i < args.size(); ++i) {
                if (i > 0) s += ",";
                s += args[i];
            }
            s += ")";
        }
        return s;
    }

    bool operator==(const Atom& other) const {
        return predicate == other.predicate && args == other.args;
    }
};

struct QueryResult {
    bool entailed = false;
    float confidence = 0.0f;
    std::vector<std::vector<Atom>> models;  // All models (for enumeration)
};

struct SolveResult {
    bool satisfiable = false;
    std::vector<std::vector<Atom>> models;  // Each model is a set of atoms
    std::vector<std::string> violated_constraints;
    std::chrono::microseconds solve_time{0};
};

enum class FactSource {
    BOOTSTRAP,
    PERCEPTION,
    INFERENCE,
    LEARNING,
    USER
};

struct Fact {
    Atom atom;
    FactSource source = FactSource::INFERENCE;
    float confidence = 1.0f;
    uint64_t timestamp = 0;
};

struct Rule {
    std::string head;
    std::vector<std::string> body;
    std::string constraint;  // For integrity constraints: ":- body."
    float weight = 1.0f;     // For weak constraints
};

struct ClingoStoreConfig {
    int max_models = 100;
    int solve_timeout_ms = 5000;
    bool enable_incremental = true;
    int thread_count = 1;
    std::string clingo_options = "--warn=none";
};

class ClingoStore {
public:
    explicit ClingoStore(const ClingoStoreConfig& cfg = ClingoStoreConfig{});
    ~ClingoStore();

    ClingoStore(const ClingoStore&) = delete;
    ClingoStore& operator=(const ClingoStore&) = delete;
    ClingoStore(ClingoStore&&) = default;
    ClingoStore& operator=(ClingoStore&&) = default;

    // Fact management
    void addFact(const Atom& atom, FactSource source = FactSource::INFERENCE, float confidence = 1.0f);
    void addFact(const std::string& predicate, const std::vector<std::string>& args, 
                 FactSource source = FactSource::INFERENCE, float confidence = 1.0f);
    void retractFact(const Atom& atom);
    void clearFacts();

    // Rule management
    void addRule(const Rule& rule);
    void addRule(const std::string& head, const std::vector<std::string>& body, float weight = 1.0f);
    void addConstraint(const std::string& body);  // Integrity constraint: :- body.
    void addWeakConstraint(const std::string& body, int priority = 1, float weight = 1.0f);
    void clearRules();

    // Query interface
    QueryResult query(const Atom& atom) const;
    QueryResult query(const std::string& predicate, const std::vector<std::string>& args) const;
    
    // Solve: compute stable models
    SolveResult solve() const;
    
    // Get all currently entailed facts
    std::vector<Atom> getEntailedFacts() const;

    // Statistics
    struct Stats {
        size_t num_facts = 0;
        size_t num_rules = 0;
        size_t num_constraints = 0;
        size_t last_solve_models = 0;
        std::chrono::microseconds last_solve_time{0};
    };
    Stats getStats() const { return stats_; }

    // Incremental solving support
    void beginIncremental();
    void endIncremental();
    void groundIncremental();

private:
    mutable std::mutex mtx_;
    ClingoStoreConfig config_;
    mutable Stats stats_;
    
    // Clingo control object
    mutable clingo_control* ctl_ = nullptr;
    bool incremental_active_ = false;
    int incremental_step_ = 0;

    // Cached program parts
    std::vector<std::string> base_program_;
    std::vector<Fact> facts_;
    std::vector<Rule> rules_;
    std::vector<std::string> constraints_;
    std::vector<std::tuple<std::string, int, float>> weak_constraints_;

    // Initialize clingo control
    void initControl() const;
    
    // Build program string from facts/rules/constraints
    std::string buildProgram() const;
    
    // Atom to clingo string
    static std::string atomToClingo(const Atom& atom);
    
    // Parse clingo model to atoms
    static std::vector<Atom> parseModel(clingo_model* model);
    
    // Execute solve with current program
    SolveResult executeSolve(const std::string& program) const;
    
    // Check if fact exists
    bool hasFact(const Atom& atom) const;
};

} // namespace reasoning
} // namespace brain
} // namespace yuki