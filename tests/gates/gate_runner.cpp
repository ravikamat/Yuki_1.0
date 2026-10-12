// gate_runner.cpp — Main entry point for KT-1 Gate Ladder
// Runs gates 0→9 in order, exits with KT-2 taxonomy

#include "gates_common.h"

#include <memory>
#include <vector>
#include <iostream>

// Forward declarations for gate factories
std::unique_ptr<yuki::gates::Gate> create_gate_0();
std::unique_ptr<yuki::gates::Gate> create_gate_1();
std::unique_ptr<yuki::gates::Gate> create_gate_2();
std::unique_ptr<yuki::gates::Gate> create_gate_3();
std::unique_ptr<yuki::gates::Gate> create_gate_4();
std::unique_ptr<yuki::gates::Gate> create_gate_5();
std::unique_ptr<yuki::gates::Gate> create_gate_6();
std::unique_ptr<yuki::gates::Gate> create_gate_7();
std::unique_ptr<yuki::gates::Gate> create_gate_8();
std::unique_ptr<yuki::gates::Gate> create_gate_9();

int main(int argc, char* argv[]) {
    using namespace yuki;
    using namespace yuki::gates;
    
    std::cout << "=== YUKI KT-1 GATE LADDER ===\n";
    std::cout << "K3 Discipline: Exact-match discrete, manifest tolerance continuous, "
              << "bit-identity our-own-paths\n\n";
    
    // Parse arguments
    bool run_weightless = false;
    bool run_single = false;
    int single_gate = -1;
    bool json_output = false;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--weightless") run_weightless = true;
        else if (arg == "--gate" && i + 1 < argc) {
            run_single = true;
            single_gate = std::stoi(argv[++i]);
        } else if (arg == "--json") json_output = true;
        else if (arg == "--help") {
            std::cout << "Usage: gate_runner [--weightless] [--gate N] [--json]\n";
            std::cout << "  --weightless  : Run gates 0-4 only (no external artifacts, ~1 min)\n";
            std::cout << "  --gate N      : Run single gate N (0-9)\n";
            std::cout << "  --json        : Output results as JSON\n";
            return 0;
        }
    }
    
    // Build gate ladder
    std::vector<std::unique_ptr<Gate>> gates;
    
    if (run_single) {
        if (single_gate >= 0 && single_gate <= 9) {
            switch (single_gate) {
                case 0: gates.push_back(create_gate_0()); break;
                case 1: gates.push_back(create_gate_1()); break;
                case 2: gates.push_back(create_gate_2()); break;
                case 3: gates.push_back(create_gate_3()); break;
                case 4: gates.push_back(create_gate_4()); break;
                case 5: gates.push_back(create_gate_5()); break;
                case 6: gates.push_back(create_gate_6()); break;
                case 7: gates.push_back(create_gate_7()); break;
                case 8: gates.push_back(create_gate_8()); break;
                case 9: gates.push_back(create_gate_9()); break;
            }
        } else {
            std::cerr << "Invalid gate number: " << single_gate << " (must be 0-9)\n";
            return static_cast<int>(SoundnessStatus::REFUSED);
        }
    } else if (run_weightless) {
        gates.push_back(create_gate_0());
        gates.push_back(create_gate_1());
        gates.push_back(create_gate_2());
        gates.push_back(create_gate_3());
        gates.push_back(create_gate_4());
    } else {
        // Full ladder 0→9
        gates.push_back(create_gate_0());
        gates.push_back(create_gate_1());
        gates.push_back(create_gate_2());
        gates.push_back(create_gate_3());
        gates.push_back(create_gate_4());
        gates.push_back(create_gate_5());
        gates.push_back(create_gate_6());
        gates.push_back(create_gate_7());
        gates.push_back(create_gate_8());
        gates.push_back(create_gate_9());
    }
    
    // Run gates
    int exit_code = run_gate_ladder(gates);
    
    if (json_output) {
        // JSON output would go here
    }
    
    return exit_code;
}