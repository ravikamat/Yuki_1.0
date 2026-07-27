#pragma once
#include <vector>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <optional>

namespace yuki::logic {

// A literal: variable index + sign (positive or negative)
struct Literal {
    uint32_t var;
    bool negated; // true = NOT(var)

    bool operator==(const Literal& other) const {
        return var == other.var && negated == other.negated;
    }
    bool operator!=(const Literal& other) const { return !(*this == other); }
    Literal negate() const { return {var, !negated}; }
};

// A clause: disjunction of literals (OR of literals)
struct Clause {
    std::vector<Literal> literals;

    bool isEmpty() const { return literals.empty(); }
    bool isUnit() const { return literals.size() == 1; }
    bool contains(const Literal& lit) const;
    bool tautology() const; // Contains both p and ~p
    std::optional<Clause> resolve(const Clause& other, uint32_t var) const;
};

// CNF formula: conjunction of clauses (AND of clauses)
struct CNF {
    std::vector<Clause> clauses;
    uint32_t num_vars = 0;

    void addClause(Clause c);
    bool isSatisfied(const std::vector<bool>& assignment) const;
};

// DPLL SAT solver (deterministic, no randomness)
class PropositionalEngine {
public:
    enum class Result { SAT, UNSAT, UNKNOWN };

    // Solve a CNF formula. Returns SAT + assignment, or UNSAT.
    struct Solution {
        Result result = Result::UNKNOWN;
        std::vector<bool> assignment; // assignment[var] = true/false
    };

    Solution solve(const CNF& formula);

    // Simplified interface: check if a set of facts is consistent
    bool isConsistent(const std::vector<std::string>& facts);

    // Resolution refutation: derive empty clause from KB + negated_goal
    bool proveByResolution(const CNF& knowledge_base, const Clause& negated_goal);

    // Truth table evaluation (exponential — only for N <= 20)
    std::vector<std::vector<bool>> allModels(const CNF& formula);

private:
    Solution dpll(CNF formula, std::vector<bool> assignment);
    std::optional<Literal> unitPropagate(CNF& formula, std::vector<bool>& assignment);
    std::optional<Literal> pureLiteralAssign(CNF& formula, std::vector<bool>& assignment);
    uint32_t chooseVariable(const CNF& formula, const std::vector<bool>& assignment);
};

} // namespace yuki::logic
