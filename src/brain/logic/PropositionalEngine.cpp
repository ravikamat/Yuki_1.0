#include "PropositionalEngine.h"
#include <algorithm>
#include <cassert>

namespace yuki::logic {

bool Clause::contains(const Literal& lit) const {
    for (const auto& l : literals) {
        if (l == lit) return true;
    }
    return false;
}

bool Clause::tautology() const {
    for (size_t i = 0; i < literals.size(); ++i) {
        for (size_t j = i + 1; j < literals.size(); ++j) {
            if (literals[i].var == literals[j].var && literals[i].negated != literals[j].negated) {
                return true;
            }
        }
    }
    return false;
}

std::optional<Clause> Clause::resolve(const Clause& other, uint32_t var) const {
    Literal me_lit{var, false};
    Literal me_neg{var, true};
    bool has_me = contains(me_lit);
    bool has_neg = contains(me_neg);

    Literal other_lit{var, false};
    Literal other_neg{var, true};
    bool other_has = other.contains(other_lit);
    bool other_has_neg = other.contains(other_neg);

    if (!((has_me && other_has_neg) || (has_neg && other_has))) {
        return std::nullopt; // Cannot resolve on this variable
    }

    Clause resolvent;
    for (const auto& l : literals) {
        if (l.var != var) resolvent.literals.push_back(l);
    }
    for (const auto& l : other.literals) {
        if (l.var != var) {
            bool dup = false;
            for (const auto& r : resolvent.literals) {
                if (r == l) { dup = true; break; }
            }
            if (!dup) resolvent.literals.push_back(l);
        }
    }
    if (resolvent.tautology()) return std::nullopt;
    return resolvent;
}

void CNF::addClause(Clause c) {
    if (!c.tautology()) {
        clauses.push_back(std::move(c));
        for (const auto& lit : clauses.back().literals) {
            if (lit.var >= num_vars) num_vars = lit.var + 1;
        }
    }
}

bool CNF::isSatisfied(const std::vector<bool>& assignment) const {
    for (const auto& clause : clauses) {
        bool satisfied = false;
        for (const auto& lit : clause.literals) {
            if (lit.var < assignment.size()) {
                bool val = assignment[lit.var];
                if (val == !lit.negated) { satisfied = true; break; }
            }
        }
        if (!satisfied) return false;
    }
    return true;
}

PropositionalEngine::Solution PropositionalEngine::solve(const CNF& formula) {
    CNF f = formula;
    std::vector<bool> assignment(formula.num_vars, false);
    return dpll(f, assignment);
}

PropositionalEngine::Solution PropositionalEngine::dpll(CNF formula, std::vector<bool> assignment) {
    // Unit propagation
    while (true) {
        auto unit = unitPropagate(formula, assignment);
        if (!unit.has_value()) break;
    }

    // Pure literal elimination
    while (true) {
        auto pure = pureLiteralAssign(formula, assignment);
        if (!pure.has_value()) break;
    }

    // Check for empty clause (UNSAT)
    for (const auto& c : formula.clauses) {
        if (c.isEmpty()) return {Result::UNSAT, {}};
    }

    // Check if all clauses satisfied
    if (formula.clauses.empty() || formula.isSatisfied(assignment)) {
        return {Result::SAT, assignment};
    }

    // Choose variable (deterministic: smallest unassigned index)
    uint32_t var = chooseVariable(formula, assignment);
    if (var >= formula.num_vars) {
        return {Result::SAT, assignment};
    }

    // Branch: try true first
    {
        CNF f_true = formula;
        std::vector<bool> a_true = assignment;
        a_true[var] = true;
        f_true.addClause(Clause{{Literal{var, false}}});
        auto sol = dpll(f_true, a_true);
        if (sol.result == Result::SAT) return sol;
    }

    // Branch: try false
    {
        CNF f_false = formula;
        std::vector<bool> a_false = assignment;
        a_false[var] = false;
        f_false.addClause(Clause{{Literal{var, true}}});
        auto sol = dpll(f_false, a_false);
        if (sol.result == Result::SAT) return sol;
    }

    return {Result::UNSAT, {}};
}

std::optional<Literal> PropositionalEngine::unitPropagate(CNF& formula, std::vector<bool>& assignment) {
    for (auto& clause : formula.clauses) {
        if (clause.isUnit()) {
            Literal lit = clause.literals[0];
            assignment[lit.var] = !lit.negated;
            std::vector<Clause> new_clauses;
            for (auto& c : formula.clauses) {
                if (c.contains(lit)) continue;
                Clause reduced;
                for (const auto& l : c.literals) {
                    if (l.var != lit.var || l.negated == lit.negated) {
                        reduced.literals.push_back(l);
                    }
                }
                if (!reduced.tautology()) new_clauses.push_back(std::move(reduced));
            }
            formula.clauses = std::move(new_clauses);
            return lit;
        }
    }
    return std::nullopt;
}

std::optional<Literal> PropositionalEngine::pureLiteralAssign(CNF& formula, std::vector<bool>& assignment) {
    std::vector<int> pos_count(formula.num_vars, 0);
    std::vector<int> neg_count(formula.num_vars, 0);
    for (const auto& c : formula.clauses) {
        for (const auto& l : c.literals) {
            if (l.var < formula.num_vars) {
                if (l.negated) neg_count[l.var]++;
                else pos_count[l.var]++;
            }
        }
    }
    for (uint32_t v = 0; v < formula.num_vars; ++v) {
        if (pos_count[v] > 0 && neg_count[v] == 0) {
            assignment[v] = true;
            std::vector<Clause> new_clauses;
            for (const auto& c : formula.clauses) {
                bool has_v = false;
                for (const auto& l : c.literals) {
                    if (l.var == v && !l.negated) { has_v = true; break; }
                }
                if (!has_v) new_clauses.push_back(c);
            }
            formula.clauses = std::move(new_clauses);
            return Literal{v, false};
        }
        if (neg_count[v] > 0 && pos_count[v] == 0) {
            assignment[v] = false;
            std::vector<Clause> new_clauses;
            for (const auto& c : formula.clauses) {
                bool has_v = false;
                for (const auto& l : c.literals) {
                    if (l.var == v && l.negated) { has_v = true; break; }
                }
                if (!has_v) new_clauses.push_back(c);
            }
            formula.clauses = std::move(new_clauses);
            return Literal{v, true};
        }
    }
    return std::nullopt;
}

uint32_t PropositionalEngine::chooseVariable(const CNF& formula, const std::vector<bool>& assignment) {
    for (uint32_t v = 0; v < formula.num_vars; ++v) {
        bool used = false;
        for (const auto& c : formula.clauses) {
            for (const auto& l : c.literals) {
                if (l.var == v) { used = true; break; }
            }
            if (used) break;
        }
        if (!used) continue;
        bool assigned = true;
        for (const auto& c : formula.clauses) {
            for (const auto& l : c.literals) {
                if (l.var == v) { assigned = false; break; }
            }
            if (!assigned) break;
        }
        if (!assigned) return v;
    }
    return formula.num_vars;
}

bool PropositionalEngine::isConsistent(const std::vector<std::string>& facts) {
    std::unordered_map<std::string, uint32_t> var_map;
    CNF formula;
    for (const auto& fact : facts) {
        bool negated = false;
        std::string name = fact;
        if (!name.empty() && name[0] == '!') {
            negated = true;
            name = name.substr(1);
        }
        auto it = var_map.find(name);
        uint32_t var;
        if (it == var_map.end()) {
            var = static_cast<uint32_t>(var_map.size());
            var_map[name] = var;
        } else {
            var = it->second;
        }
        formula.addClause(Clause{{Literal{var, negated}}});
    }
    auto sol = solve(formula);
    return sol.result == Result::SAT;
}

bool PropositionalEngine::proveByResolution(const CNF& knowledge_base, const Clause& negated_goal) {
    CNF working = knowledge_base;
    working.addClause(negated_goal);

    bool added = true;
    while (added) {
        added = false;
        size_t n = working.clauses.size();
        for (size_t i = 0; i < n; ++i) {
            for (size_t j = i + 1; j < n; ++j) {
                for (const auto& lit1 : working.clauses[i].literals) {
                    auto resolvent = working.clauses[i].resolve(working.clauses[j], lit1.var);
                    if (resolvent.has_value()) {
                        if (resolvent->isEmpty()) return true;
                        bool dup = false;
                        for (const auto& c : working.clauses) {
                            if (c.literals.size() == resolvent->literals.size()) {
                                bool same = true;
                                for (size_t k = 0; k < c.literals.size(); ++k) {
                                    if (c.literals[k] != resolvent->literals[k]) { same = false; break; }
                                }
                                if (same) { dup = true; break; }
                            }
                        }
                        if (!dup) {
                            working.addClause(*resolvent);
                            added = true;
                        }
                    }
                }
            }
        }
    }
    return false;
}

std::vector<std::vector<bool>> PropositionalEngine::allModels(const CNF& formula) {
    std::vector<std::vector<bool>> models;
    if (formula.num_vars > 20) return models;
    size_t total = 1ULL << formula.num_vars;
    for (size_t mask = 0; mask < total; ++mask) {
        std::vector<bool> assignment(formula.num_vars);
        for (uint32_t v = 0; v < formula.num_vars; ++v) {
            assignment[v] = (mask >> v) & 1ULL;
        }
        if (formula.isSatisfied(assignment)) {
            models.push_back(assignment);
        }
    }
    return models;
}

} // namespace yuki::logic
