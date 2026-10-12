// gates_common.h — Common infrastructure for KT-1 Gate Ladder
// All gates use this for fixtures, verdict reporting, and KT-2 soundness taxonomy

#pragma once
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>
#include <cstdint>
#include <chrono>
#include <nlohmann/json.hpp>

#include "yuki_soundness.h"

namespace yuki {
namespace gates {

// ─────────────────────────────────────────────────────────────────────────────
// Fixture System — Self-verifying fixtures that refuse to be useless
// A fixture that a plausible-wrong implementation passes is REJECTED at generation
// ─────────────────────────────────────────────────────────────────────────────
struct Fixture {
    std::string name;
    nlohmann::json input;
    nlohmann::json expected_output;
    std::string description;  // What mechanism this fixture exposes
    
    // Adversarial check: fixture MUST fail on a known-broken implementation
    // This is verified at fixture GENERATION time, not test time
    bool adversarial_verified = false;
    std::string broken_impl_behavior;  // What a broken impl would produce
};

class FixtureRegistry {
public:
    static FixtureRegistry& instance() {
        static FixtureRegistry reg;
        return reg;
    }
    
    void register_fixture(const Fixture& f) {
        // Verify adversarial property: fixture must distinguish correct from broken
        if (!f.adversarial_verified) {
            std::cerr << "[FixtureRegistry] WARNING: Fixture '" << f.name 
                      << "' not adversarially verified — may be useless\n";
        }
        fixtures_.push_back(f);
    }
    
    const std::vector<Fixture>& all() const { return fixtures_; }
    const Fixture* get(const std::string& name) const {
        for (const auto& f : fixtures_) if (f.name == name) return &f;
        return nullptr;
    }
    
    size_t count() const { return fixtures_.size(); }
    
private:
    std::vector<Fixture> fixtures_;
};

// ─────────────────────────────────────────────────────────────────────────────
// Gate Result — Standardized output for all gates
// ─────────────────────────────────────────────────────────────────────────────
struct GateResult {
    int gate_number = -1;
    std::string gate_name;
    bool passed = false;
    std::string message;
    uint64_t duration_ms = 0;
    size_t tests_run = 0;
    size_t tests_passed = 0;
    size_t tests_failed = 0;
    size_t tests_skipped = 0;  // SKIPS COUNT AS FAILURES (K3 discipline)
    
    // Exact-match requirement for discrete decisions (K3: "on a discrete argmax 
    // there is no such thing as a small error")
    bool exact_match_required = true;
    
    // Manifest-sourced tolerances for continuous intermediates
    struct Tolerance {
        std::string metric;
        double max_abs_diff = 0.0;
        double max_rel_diff = 0.0;
    };
    std::vector<Tolerance> tolerances;
};

inline std::ostream& operator<<(std::ostream& os, const GateResult& r) {
    os << "GATE " << r.gate_number << " [" << r.gate_name << "] : "
       << (r.passed ? "PASS" : "FAIL") 
       << " (" << r.tests_passed << "/" << r.tests_run << " tests, "
       << r.tests_skipped << " skipped, " << r.duration_ms << "ms)";
    if (!r.message.empty()) os << " — " << r.message;
    return os;
}

// ─────────────────────────────────────────────────────────────────────────────
// Gate Base Class — All gates inherit from this
// ─────────────────────────────────────────────────────────────────────────────
class Gate {
public:
    virtual ~Gate() = default;
    
    virtual GateResult run() = 0;
    virtual std::string name() const = 0;
    virtual int number() const = 0;
    
    // Run with timing and standardized output
    GateResult execute() {
        auto t0 = std::chrono::steady_clock::now();
        GateResult result = run();
        auto t1 = std::chrono::steady_clock::now();
        result.duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
        result.gate_number = number();
        result.gate_name = name();
        
        // Print standardized result
        std::cout << result << "\n";
        
        // Print VERDICT line for CI grep (K3: verdict-as-exit-code + transcript grep)
        if (result.passed && result.tests_skipped == 0) {
            std::cout << "VERDICT: EXACT MATCH — Gate " << result.gate_number << " PASSED\n";
        } else {
            std::cout << "VERDICT: FAIL — Gate " << result.gate_number << " FAILED\n";
            if (result.tests_skipped > 0) {
                std::cerr << "  SKIPS COUNT AS FAILURES: " << result.tests_skipped << " tests skipped\n";
            }
        }
        
        return result;
    }
    
protected:
    // Helper: run a single test case with exact-match or tolerance
    template<typename Fn>
    bool run_test(const std::string& test_name, Fn&& test_fn, bool exact = true) {
        try {
            bool ok = test_fn();
            if (ok) {
                std::cout << "  [PASS] " << test_name << "\n";
                return true;
            } else {
                std::cerr << "  [FAIL] " << test_name << "\n";
                return false;
            }
        } catch (const std::exception& e) {
            std::cerr << "  [ERROR] " << test_name << ": " << e.what() << "\n";
            return false;
        }
    }
    
    // Helper: compare two values with exact match or tolerance
    template<typename T>
    bool compare_exact(const T& a, const T& b, const std::string& context) {
        if (a == b) return true;
        std::cerr << "  MISMATCH [" << context << "]: expected " << b << ", got " << a << "\n";
        return false;
    }
    
    template<typename T>
    bool compare_tolerance(const T& a, const T& b, double abs_tol, double rel_tol, const std::string& context) {
        double diff = std::abs(static_cast<double>(a) - static_cast<double>(b));
        double scale = std::max(std::abs(static_cast<double>(a)), std::abs(static_cast<double>(b)));
        if (diff <= abs_tol + rel_tol * scale) return true;
        std::cerr << "  TOLERANCE VIOLATION [" << context << "]: expected " << b << ", got " << a 
                  << " (diff=" << diff << ", abs_tol=" << abs_tol << ", rel_tol=" << rel_tol << ")\n";
        return false;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Fixture Loading — Load from JSON files in fixtures/
// ─────────────────────────────────────────────────────────────────────────────
inline std::vector<Fixture> load_fixtures(const std::string& fixture_dir) {
    std::vector<Fixture> fixtures;
    namespace fs = std::filesystem;
    
    try {
        for (const auto& entry : fs::directory_iterator(fixture_dir)) {
            if (entry.path().extension() == ".json") {
                std::ifstream f(entry.path());
                nlohmann::json j;
                f >> j;
                
                Fixture fix;
                fix.name = j.value("name", entry.path().stem().string());
                fix.input = j["input"];
                fix.expected_output = j["expected_output"];
                fix.description = j.value("description", "");
                fix.adversarial_verified = j.value("adversarial_verified", false);
                fix.broken_impl_behavior = j.value("broken_impl_behavior", "");
                
                fixtures.push_back(std::move(fix));
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[FixtureLoader] Failed to load from " << fixture_dir << ": " << e.what() << "\n";
    }
    
    return fixtures;
}

// ─────────────────────────────────────────────────────────────────────────────
// Main Gate Runner — Executes all gates in order, exits with KT-2 taxonomy
// ─────────────────────────────────────────────────────────────────────────────
inline int run_gate_ladder(const std::vector<std::unique_ptr<Gate>>& gates) {
    std::cout << "=== KT-1 GATE LADDER START ===\n";
    std::cout << "Total gates: " << gates.size() << "\n\n";
    
    bool all_passed = true;
    size_t total_skipped = 0;
    
    for (const auto& gate : gates) {
        GateResult result = gate->execute();
        if (!result.passed || result.tests_skipped > 0) {
            all_passed = false;
        }
        total_skipped += result.tests_skipped;
        std::cout << "\n";
    }
    
    std::cout << "=== KT-1 GATE LADDER END ===\n";
    
    if (all_passed && total_skipped == 0) {
        std::cout << "VERDICT: ALL GATES PASSED\n";
        return static_cast<int>(SoundnessStatus::SUCCESS);  // 0
    } else {
        std::cout << "VERDICT: FAIL — " << total_skipped << " skips, gates failed\n";
        return static_cast<int>(SoundnessStatus::REFUSED);  // 2 - configuration wrong
    }
}

} // namespace gates
} // namespace yuki