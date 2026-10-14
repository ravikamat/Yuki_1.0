// ClingoStore.cpp — ASP-based Knowledge Store implementation
#include "ClingoStore.h"

#include <clingo.h>
#include <algorithm>
#include <sstream>
#include <iostream>

namespace yuki {
namespace brain {
namespace reasoning {

static uint64_t currentTimestamp() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

ClingoStore::ClingoStore(const ClingoStoreConfig& cfg)
    : config_(cfg) {
    initControl();
    
    // Add standard Gridworld constraints
    addConstraint("on(X,Y), on(X,Z), Y != Z");  // Block can only be on one thing
    addConstraint("holding(X), holding(Y), X != Y");  // Can only hold one block
    addConstraint("on(X,Y), holding(X)");  // Can't hold and be on
    addConstraint("clear(X), on(Y,X)");  // Clear means nothing on top
    addConstraint("at(X,P1), at(X,P2), P1 != P2");  // Block at one position
}

ClingoStore::~ClingoStore() {
    if (ctl_) {
        clingo_control_free(ctl_);
        ctl_ = nullptr;
    }
}

void ClingoStore::initControl() const {
    std::lock_guard<std::mutex> lock(mtx_);
    
    if (ctl_) {
        clingo_control_free(ctl_);
    }
    
    char const* argv[] = { config_.clingo_options.c_str() };
    int argc = 1;
    
    clingo_control* new_ctl = nullptr;
    clingo_error_t err = clingo_control_new(argv, argc, nullptr, nullptr, 20, &new_ctl);
    if (err != CLINGO_SUCCESS || !new_ctl) {
        // Fallback: create without options
        clingo_control_new(nullptr, 0, nullptr, nullptr, 20, &new_ctl);
    }
    
    ctl_ = new_ctl;
    
    // Configure
    clingo_control_configure(ctl_, ("--models=" + std::to_string(config_.max_models)).c_str());
    clingo_control_configure(ctl_, ("--time-limit=" + std::to_string(config_.solve_timeout_ms / 1000.0)).c_str());
    clingo_control_configure(ctl_, ("--threads=" + std::to_string(config_.thread_count)).c_str());
}

void ClingoStore::addFact(const Atom& atom, FactSource source, float confidence) {
    std::lock_guard<std::mutex> lock(mtx_);
    
    Fact fact;
    fact.atom = atom;
    fact.source = source;
    fact.confidence = confidence;
    fact.timestamp = currentTimestamp();
    
    // Replace if exists
    auto it = std::find_if(facts_.begin(), facts_.end(),
        [&atom](const Fact& f) { return f.atom == atom; });
    
    if (it != facts_.end()) {
        *it = fact;
    } else {
        facts_.push_back(fact);
    }
    
    stats_.num_facts = facts_.size();
}

void ClingoStore::addFact(const std::string& predicate, const std::vector<std::string>& args,
                          FactSource source, float confidence) {
    addFact(Atom(predicate, args), source, confidence);
}

void ClingoStore::retractFact(const Atom& atom) {
    std::lock_guard<std::mutex> lock(mtx_);
    
    facts_.erase(std::remove_if(facts_.begin(), facts_.end(),
        [&atom](const Fact& f) { return f.atom == atom; }), facts_.end());
    
    stats_.num_facts = facts_.size();
}

void ClingoStore::clearFacts() {
    std::lock_guard<std::mutex> lock(mtx_);
    facts_.clear();
    stats_.num_facts = 0;
}

void ClingoStore::addRule(const Rule& rule) {
    std::lock_guard<std::mutex> lock(mtx_);
    rules_.push_back(rule);
    stats_.num_rules = rules_.size();
}

void ClingoStore::addRule(const std::string& head, const std::vector<std::string>& body, float weight) {
    Rule rule;
    rule.head = head;
    rule.body = body;
    rule.weight = weight;
    addRule(rule);
}

void ClingoStore::addConstraint(const std::string& body) {
    std::lock_guard<std::mutex> lock(mtx_);
    constraints_.push_back(body);
    stats_.num_constraints = constraints_.size();
}

void ClingoStore::addWeakConstraint(const std::string& body, int priority, float weight) {
    std::lock_guard<std::mutex> lock(mtx_);
    weak_constraints_.emplace_back(body, priority, weight);
}

void ClingoStore::clearRules() {
    std::lock_guard<std::mutex> lock(mtx_);
    rules_.clear();
    constraints_.clear();
    weak_constraints_.clear();
    stats_.num_rules = 0;
    stats_.num_constraints = 0;
}

ClingoStore::QueryResult ClingoStore::query(const Atom& atom) const {
    std::lock_guard<std::mutex> lock(mtx_);
    
    QueryResult result;
    auto solve_result = executeSolve(buildProgram());
    
    if (solve_result.satisfiable && !solve_result.models.empty()) {
        // Check if atom appears in ANY model (cautious reasoning) or ALL models (brave reasoning)
        // We use cautious: entailed if in all models
        bool in_all = true;
        for (const auto& model : solve_result.models) {
            bool found = std::find(model.begin(), model.end(), atom) != model.end();
            if (!found) {
                in_all = false;
                break;
            }
        }
        
        result.entailed = in_all;
        if (in_all) {
            result.confidence = 1.0f;
        } else {
            // Check if in some models (possible)
            int count = 0;
            for (const auto& model : solve_result.models) {
                if (std::find(model.begin(), model.end(), atom) != model.end()) {
                    ++count;
                }
            }
            result.confidence = solve_result.models.empty() ? 0.0f : 
                static_cast<float>(count) / solve_result.models.size();
        }
        
        result.models = solve_result.models;
    }
    
    return result;
}

ClingoStore::QueryResult ClingoStore::query(const std::string& predicate, 
                                             const std::vector<std::string>& args) const {
    return query(Atom(predicate, args));
}

ClingoStore::SolveResult ClingoStore::solve() const {
    std::lock_guard<std::mutex> lock(mtx_);
    return executeSolve(buildProgram());
}

std::vector<Atom> ClingoStore::getEntailedFacts() const {
    std::lock_guard<std::mutex> lock(mtx_);
    auto solve_result = executeSolve(buildProgram());
    
    std::vector<Atom> entailed;
    if (solve_result.satisfiable && !solve_result.models.empty()) {
        // Find atoms present in ALL models (cautious reasoning)
        const auto& first_model = solve_result.models[0];
        for (const auto& atom : first_model) {
            bool in_all = true;
            for (size_t i = 1; i < solve_result.models.size(); ++i) {
                if (std::find(solve_result.models[i].begin(), solve_result.models[i].end(), atom) 
                    == solve_result.models[i].end()) {
                    in_all = false;
                    break;
                }
            }
            if (in_all) {
                entailed.push_back(atom);
            }
        }
    }
    return entailed;
}

void ClingoStore::beginIncremental() {
    std::lock_guard<std::mutex> lock(mtx_);
    if (!incremental_active_) {
        incremental_active_ = true;
        incremental_step_ = 0;
    }
}

void ClingoStore::endIncremental() {
    std::lock_guard<std::mutex> lock(mtx_);
    incremental_active_ = false;
}

void ClingoStore::groundIncremental() {
    std::lock_guard<std::mutex> lock(mtx_);
    if (incremental_active_ && ctl_) {
        // Ground current step
        std::ostringstream oss;
        oss << "#program base.\n" << buildProgram() << "\n";
        oss << "#program step(" << incremental_step_ << ").\n";
        
        clingo_control_add(ctl_, "step", oss.str().c_str());
        clingo_control_ground(ctl_, nullptr, 0, nullptr, nullptr);
        ++incremental_step_;
    }
}

std::string ClingoStore::atomToClingo(const Atom& atom) {
    std::string s = atom.predicate;
    // Convert to lowercase for clingo
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    
    if (!atom.args.empty()) {
        s += "(";
        for (size_t i = 0; i < atom.args.size(); ++i) {
            if (i > 0) s += ",";
            std::string arg = atom.args[i];
            // Variables start with uppercase, constants with lowercase
            if (!arg.empty() && std::isupper(arg[0])) {
                s += arg;  // Variable
            } else {
                s += "\"" + arg + "\"";  // Constant
            }
        }
        s += ")";
    }
    return s;
}

std::string ClingoStore::buildProgram() const {
    std::ostringstream oss;
    
    // Facts
    for (const auto& fact : facts_) {
        oss << atomToClingo(fact.atom) << ".\n";
    }
    
    // Rules
    for (const auto& rule : rules_) {
        oss << rule.head << " :- ";
        for (size_t i = 0; i < rule.body.size(); ++i) {
            if (i > 0) oss << ", ";
            oss << rule.body[i];
        }
        oss << ".\n";
    }
    
    // Constraints
    for (const auto& constraint : constraints_) {
        oss << ":- " << constraint << ".\n";
    }
    
    // Weak constraints
    for (const auto& [body, priority, weight] : weak_constraints_) {
        oss << ":~ " << body << ". [" << static_cast<int>(weight) << "@" << priority << "]\n";
    }
    
    return oss.str();
}

std::vector<Atom> ClingoStore::parseModel(clingo_model* model) {
    std::vector<Atom> atoms;
    
    // Get symbolic atoms from model
    clingo_symbolic_atoms* sym_atoms = nullptr;
    clingo_model_symbolic_atoms(model, &sym_atoms);
    
    if (sym_atoms) {
        size_t num_atoms = 0;
        clingo_symbolic_atoms_size(sym_atoms, &num_atoms);
        
        for (size_t i = 0; i < num_atoms; ++i) {
            clingo_symbol* sym = nullptr;
            clingo_symbolic_atoms_at(sym_atoms, i, &sym);
            
            if (sym) {
                char* str = nullptr;
                size_t str_size = 0;
                clingo_symbol_to_string(sym, &str, &str_size);
                
                if (str) {
                    // Parse "pred(arg1,arg2)" format
                    std::string atom_str(str);
                    free(str);
                    
                    size_t paren = atom_str.find('(');
                    if (paren != std::string::npos) {
                        std::string pred = atom_str.substr(0, paren);
                        std::string args_str = atom_str.substr(paren + 1, atom_str.size() - paren - 2);
                        
                        std::vector<std::string> args;
                        size_t start = 0;
                        while (start < args_str.size()) {
                            size_t comma = args_str.find(',', start);
                            if (comma == std::string::npos) comma = args_str.size();
                            std::string arg = args_str.substr(start, comma - start);
                            // Remove quotes from constants
                            if (arg.size() >= 2 && arg.front() == '"' && arg.back() == '"') {
                                arg = arg.substr(1, arg.size() - 2);
                            }
                            args.push_back(arg);
                            start = comma + 1;
                        }
                        
                        atoms.emplace_back(pred, args);
                    } else {
                        // Zero-arity atom
                        atoms.emplace_back(atom_str, std::vector<std::string>{});
                    }
                }
            }
        }
    }
    
    return atoms;
}

ClingoStore::SolveResult ClingoStore::executeSolve(const std::string& program) const {
    SolveResult result;
    auto start = std::chrono::high_resolution_clock::now();
    
    if (!ctl_) {
        initControl();
    }
    
    // Add base program
    clingo_control_add(ctl_, "base", program.c_str());
    
    // Ground
    clingo_control_ground(ctl_, nullptr, 0, nullptr, nullptr);
    
    // Solve
    clingo_solve_handle* handle = nullptr;
    clingo_control_solve(ctl_, CLINGO_SOLVE_MODE_YIELD, nullptr, 0, &handle);
    
    result.satisfiable = false;
    result.models.clear();
    result.violated_constraints.clear();
    
    if (handle) {
        bool done = false;
        while (!done) {
            clingo_model* model = nullptr;
            clingo_solve_handle_resume(handle, &model, &done);
            
            if (model) {
                result.satisfiable = true;
                result.models.push_back(parseModel(model));
                clingo_model_free(model);
            }
            
            if (result.models.size() >= static_cast<size_t>(config_.max_models)) {
                break;
            }
        }
        clingo_solve_handle_close(handle);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    result.solve_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    stats_.last_solve_models = result.models.size();
    stats_.last_solve_time = result.solve_time;
    
    return result;
}

bool ClingoStore::hasFact(const Atom& atom) const {
    return std::any_of(facts_.begin(), facts_.end(),
        [&atom](const Fact& f) { return f.atom == atom; });
}

} // namespace reasoning
} // namespace brain
} // namespace yuki