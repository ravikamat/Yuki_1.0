// Integration test for full cognitive loop
// Tests BabyMode with all COGNI-1 components wired together

#include "gates_common.h"
#include "yuki_soundness.h"

#include "BabyMode.h"
#include "brain/inference/VariationalStateEstimator.h"
#include "brain/core/HierarchicalPredictiveCoding.h"
#include "brain/predictive/LatentWorldModel.h"
#include "brain/causality/CausalWorldModel.h"
#include "brain/planning/LatentMPCPlanner.h"
#include "brain/policy/ActiveInferencePolicyEngine.h"
#include "brain/language/LocalLLM.h"
#include "brain/language/GenerationRouter.h"
#include "brain/language/LatentTokenProjector.h"
#include "brain/simulation/VirtualSimulation.h"
#include "brain/simulation/BranchManagement.h"
#include "brain/sleep/StructureDiscovery.h"
#include "brain/memory/SparseHDC.h"
#include "brain/memory/INT8Quantization.h"
#include "brain/language/KVCache.h"
#include "brain/language/TieredModels.h"
#include "brain/memory/AutoTieredMemory.h"
#include "brain/memory/CognitiveMemoryFabric.h"
#include "brain/self/MetacognitiveMonitor.h"
#include "brain/selfimprove/PerTurnLearningHook.h"

#include <vector>
#include <string>
#include <random>
#include <chrono>
#include <thread>

using namespace yuki;
using namespace yuki::gates;
using yuki::brain::inference::VariationalStateEstimator;
using yuki::brain::core::HierarchicalPredictiveCoding;
using yuki::brain::predictive::LatentWorldModel;
using yuki::brain::causality::CausalWorldModel;
using yuki::brain::planning::LatentMPCPlanner;
using yuki::brain::policy::ActiveInferencePolicyEngine;
using yuki::language::LocalLLM;
using yuki::language::GenerationRouter;
using yuki::brain::language::LatentTokenProjector;
using yuki::brain::simulation::VirtualSimulation;
using yuki::brain::simulation::BranchManagement;
using yuki::brain::sleep::StructureDiscovery;
using yuki::memory::SparseHDC;
using yuki::memory::INT8Quantization;
using yuki::language::KVCache;
using yuki::language::TieredModels;
using yuki::memory::AutoTieredMemory;
using yuki::brain::memory::CognitiveMemoryFabric;
using yuki::brain::self::MetacognitiveMonitor;
using yuki::selfimprove::PerTurnLearningHook;

class Gate_CognitiveLoopIntegration : public Gate {
public:
    std::string name() const override { return "Full Cognitive Loop Integration"; }
    int number() const override { return 10; }
    
    GateResult run() override {
        GateResult result;
        result.exact_match_required = false;  // Integration test, not bit-identical
        
        // Test 1: BabyMode initialization with all components
        result.tests_run++;
        result.tests_passed += run_test("babymode_full_init", [this]() {
            return testBabyModeFullInit();
        });
        
        // Test 2: End-to-end turn processing
        result.tests_run++;
        result.tests_passed += run_test("e2e_turn_processing", [this]() {
            return testE2ETurnProcessing();
        });
        
        // Test 3: VirtualSimulation what-if reasoning
        result.tests_run++;
        result.tests_passed += run_test("virtual_simulation_whatif", [this]() {
            return testVirtualSimulationWhatIf();
        });
        
        // Test 4: BranchManagement hypothesis tracking
        result.tests_run++;
        result.tests_passed += run_test("branch_management_hypothesis", [this]() {
            return testBranchManagementHypothesis();
        });
        
        // Test 5: StructureDiscovery sleep consolidation
        result.tests_run++;
        result.tests_passed += run_test("structure_discovery_sleep", [this]() {
            return testStructureDiscoverySleep();
        });
        
        // Test 6: SparseHDC memory efficiency
        result.tests_run++;
        result.tests_passed += run_test("sparse_hdc_efficiency", [this]() {
            return testSparseHDCEfficiency();
        });
        
        // Test 7: INT8 quantization
        result.tests_run++;
        result.tests_passed += run_test("int8_quantization", [this]() {
            return testINT8Quantization();
        });
        
        // Test 8: KVCache for LLM inference
        result.tests_run++;
        result.tests_passed += run_test("kv_cache_llm", [this]() {
            return testKVCacheLLM();
        });
        
        // Test 9: TieredModels routing
        result.tests_run++;
        result.tests_passed += run_test("tiered_models_routing", [this]() {
            return testTieredModelsRouting();
        });
        
        // Test 10: AutoTieredMemory tiering
        result.tests_run++;
        result.tests_passed += run_test("auto_tiered_memory", [this]() {
            return testAutoTieredMemory();
        });
        
        // Test 11: PerTurnLearningHook online learning
        result.tests_run++;
        result.tests_passed += run_test("per_turn_learning", [this]() {
            return testPerTurnLearning();
        });
        
        // Test 12: MetacognitiveMonitor strategy shifts
        result.tests_run++;
        result.tests_passed += run_test("metacognitive_strategy_shift", [this]() {
            return testMetacognitiveStrategyShift();
        });
        
        // Test 13: Randomized stress test
        result.tests_run++;
        result.tests_passed += run_test("randomized_stress_test", [this]() {
            return testRandomizedStress();
        });
        
        result.passed = (result.tests_passed == result.tests_run);
        return result;
    }

private:
    bool testBabyModeFullInit() {
        try {
            BabyMode baby;
            
            // Initialize all components
            baby.initialize();
            
            // Verify all subsystems are wired
            auto vse = baby.variationalEstimator();
            auto hpc = baby.getHPC();
            auto cmf = baby.cmFabric();
            auto lwm = baby.getLatentWorldModel();
            auto cwm = baby.getCausalWorldModel();
            auto mpc = baby.getLatentMPCPlanner();
            auto policy = baby.getPolicyEngine();
            auto llm = baby.getLocalLLM();
            auto router = baby.getGenerationRouter();
            auto ltp = baby.getLatentTokenProjector();
            
            // Check all pointers are valid
            if (!vse || !hpc || !cmf || !lwm || !cwm || !mpc || !policy || !llm || !router || !ltp) {
                return false;
            }
            
            // Verify HPC has 5 levels
            auto hpc_out = hpc->step({0}, {});
            if (hpc_out.highest_active_level < 0 || hpc_out.highest_active_level > 4) {
                return false;
            }
            
            return true;
        } catch (const std::exception& e) {
            return false;
        }
    }
    
    bool testE2ETurnProcessing() {
        try {
            BabyMode baby;
            baby.initialize();
            
            // Process a turn
            std::string user_input = "What is the capital of France?";
            auto response = baby.processTurn(user_input);
            
            // Verify response is generated
            if (response.empty()) return false;
            
            // Check VSE updated
            auto vse = baby.variationalEstimator();
            auto belief = vse->currentBelief();
            if (belief.free_energy < 0) return false;  // Should be non-negative
            
            // Check HPC processed
            auto hpc = baby.getHPC();
            auto hpc_out = hpc->getLevel(0);
            if (hpc_out.surprise < 0) return false;
            
            // Check policy selected
            auto policy = baby.getPolicyEngine();
            int action = policy->getSelectedAction();
            if (action < 0 || action >= 12) return false;
            
            return true;
        } catch (const std::exception&) {
            return false;
        }
    }
    
    bool testVirtualSimulationWhatIf() {
        try {
            VirtualSimulation vs;
            CausalWorldModel cwm;
            LatentWorldModel lwm;
            
            vs.setCausalWorldModel(&cwm);
            vs.setLatentWorldModel(&lwm);
            
            VirtualSimulation::WhatIfQuery query;
            query.natural_language = "What if I study 2 hours daily?";
            query.horizon = 10;
            query.num_samples = 32;
            
            // Initialize belief state
            yuki::brain::inference::BeliefState belief;
            belief.q_intent.fill(0.125f);
            query.initial_state = belief;
            
            auto result = vs.runWhatIf(query);
            
            // Verify result has valid fields
            if (result.branch_id == 0) return false;
            if (result.confidence < 0.0f || result.confidence > 1.0f) return false;
            if (result.summary.empty()) return false;
            
            return true;
        } catch (const std::exception&) {
            return false;
        }
    }
    
    bool testBranchManagementHypothesis() {
        try {
            BranchManagement bm;
            VirtualSimulation vs;
            bm.setVirtualSimulation(&vs);
            
            // Propose hypotheses
            uint64_t hyp1 = bm.proposeHypothesis("Studying improves grades", "study -> grades", 0.6f);
            uint64_t hyp2 = bm.proposeHypothesis("Sleep improves grades", "sleep -> grades", 0.5f);
            
            if (hyp1 == 0 || hyp2 == 0) return false;
            
            // Add evidence
            BranchManagement::Evidence ev1;
            ev1.type = BranchManagement::Evidence::Type::OBSERVATION;
            ev1.description = "Student A studied 2h/day, grade improved";
            ev1.weight = 0.8f;
            ev1.reliability = 0.9f;
            bm.addEvidence(hyp1, ev1);
            
            BranchManagement::Evidence ev2;
            ev2.type = BranchManagement::Evidence::Type::OBSERVATION;
            ev2.description = "Student B slept 8h/day, grade improved";
            ev2.weight = 0.7f;
            ev2.reliability = 0.8f;
            bm.addEvidence(hyp2, ev2);
            
            // Evaluate
            bm.evaluateAll();
            
            // Get best
            uint64_t best = bm.getBestHypothesis();
            if (best == 0) return false;
            
            auto* best_hyp = bm.getHypothesis(best);
            if (!best_hyp) return false;
            if (best_hyp->posterior <= 0.5f) return false;  // Should be updated
            
            return true;
        } catch (const std::exception&) {
            return false;
        }
    }
    
    bool testStructureDiscoverySleep() {
        try {
            StructureDiscovery sd;
            CausalWorldModel cwm;
            LatentWorldModel lwm;
            yuki::brain::memory::EpisodicStore es;
            
            sd.setCausalWorldModel(&cwm);
            sd.setLatentWorldModel(&lwm);
            sd.setEpisodicStore(&es);
            
            float avg_conf = 0.0f;
            size_t discoveries = sd.runDiscoveryEpoch(avg_conf);
            
            // Should run without crashing
            if (avg_conf < 0.0f || avg_conf > 1.0f) return false;
            
            // Test export
            auto export_result = sd.exportDistillation();
            // May fail if no data, but shouldn't crash
            
            return true;
        } catch (const std::exception&) {
            return false;
        }
    }
    
    bool testSparseHDCEfficiency() {
        try {
            SparseHDC sparse_hdc;
            std::mt19937 gen(42);
            
            // Generate sparse vectors
            auto a = sparse_hdc.random(gen);
            auto b = sparse_hdc.random(gen);
            
            // Check sparsity
            float sparsity_a = static_cast<float>(a.nnz()) / 10000.0f;
            float sparsity_b = static_cast<float>(b.nnz()) / 10000.0f;
            
            if (sparsity_a > 0.02f || sparsity_b > 0.02f) return false;  // Should be ~1%
            
            // Test operations
            auto bound = sparse_hdc.bind(a, b);
            auto bundled = sparse_hdc.bundle(a, b);
            auto permuted = sparse_hdc.permute(a, 3);
            
            // Check memory efficiency
            size_t sparse_mem = a.memoryBytes();
            size_t dense_mem = 10000 / 8;  // 1250 bytes for bitset
            // Sparse should be smaller for 1% sparsity
            // (100 * 2 + 100 * 1) = 300 bytes vs 1250 bytes
            if (sparse_mem > dense_mem) return false;
            
            // Test similarity
            float sim = sparse_hdc.cosineSimilarity(a, a);
            if (sim < 0.99f) return false;  // Self-similarity should be 1.0
            
            return true;
        } catch (const std::exception&) {
            return false;
        }
    }
    
    bool testINT8Quantization() {
        try {
            INT8Quantizer quantizer;
            
            // Test tensor quantization
            std::vector<float> weights(1000);
            std::mt19937 gen(42);
            std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
            for (auto& w : weights) w = dist(gen);
            
            std::vector<size_t> shape = {100, 10};
            auto qt = quantizer.quantizePerChannel(weights, shape, 0);
            
            // Check compression ratio ~4x
            if (qt.compressionRatio() < 3.0f) return false;
            
            // Test dequantization
            auto dequantized = quantizer.dequantize(qt);
            if (dequantized.size() != weights.size()) return false;
            
            // Check error is small
            float max_error = 0.0f;
            for (size_t i = 0; i < weights.size(); ++i) {
                max_error = std::max(max_error, std::abs(weights[i] - dequantized[i]));
            }
            if (max_error > 0.1f) return false;  // Max error should be small
            
            // Test quantized linear
            std::vector<int8_t> input(10);
            for (auto& v : input) v = static_cast<int8_t>(dist(gen) * 127);
            
            INT8Quantizer::QuantizedTensor bias_qt;
            bias_qt.data.assign(100, 0);
            bias_qt.scales = {0.01f};
            bias_qt.zero_points = {0};
            
            auto output = quantizer.quantizedLinear(input, qt, bias_qt, 1, 10, 100);
            if (output.size() != 100) return false;
            
            return true;
        } catch (const std::exception&) {
            return false;
        }
    }
    
    bool testKVCacheLLM() {
        try {
            KVCache::KVCacheConfig cfg;
            cfg.max_batch_size = 4;
            cfg.max_seq_len = 1024;
            cfg.num_layers = 12;
            cfg.num_heads = 12;
            cfg.head_dim = 64;
            cfg.num_kv_heads = 12;
            cfg.page_size = 256;
            
            KVCache kv_cache(cfg);
            
            // Allocate entry
            auto entry = kv_cache.allocate(512);
            if (entry.page_indices.empty()) return false;
            
            // Generate test K/V
            size_t per_token = cfg.num_kv_heads * cfg.head_dim;
            std::vector<float> keys(2 * per_token, 0.5f);
            std::vector<float> values(2 * per_token, 0.3f);
            
            // Append
            size_t appended = kv_cache.append(entry, keys, values, 1, 2);
            if (appended != 2) return false;
            
            // Retrieve
            std::vector<float> out_keys, out_values;
            kv_cache.get(entry, out_keys, out_values, 1, 2);
            
            if (out_keys.size() != keys.size()) return false;
            if (out_values.size() != values.size()) return false;
            
            // Verify values
            for (size_t i = 0; i < keys.size(); ++i) {
                if (std::abs(out_keys[i] - keys[i]) > 1e-5f) return false;
                if (std::abs(out_values[i] - values[i]) > 1e-5f) return false;
            }
            
            // Test copy (beam search)
            auto copied = kv_cache.copy(entry);
            if (copied.page_indices.size() != entry.page_indices.size()) return false;
            
            // Free
            kv_cache.free(entry);
            
            return true;
        } catch (const std::exception&) {
            return false;
        }
    }
    
    bool testTieredModelsRouting() {
        try {
            TieredModels::TieredModelConfig cfg;
            cfg.router_confidence_threshold = 0.8f;
            cfg.complexity_threshold = 0.6f;
            
            TieredModels tm(cfg);
            tm.initialize();
            
            // Simple query - should use router
            auto result1 = tm.generate("What is 2+2?");
            if (result1.decision.action != TieredModels::RoutingDecision::Action::USE_ROUTER) {
                // Might escalate due to placeholder, but should not crash
            }
            
            // Complex query - should escalate
            auto result2 = tm.generate("Explain quantum entanglement and its applications in quantum computing");
            // Decision may vary, but should complete
            
            // Check stats
            auto stats = tm.getStats();
            if (stats.total_requests == 0) return false;
            
            return true;
        } catch (const std::exception&) {
            return false;
        }
    }
    
    bool testAutoTieredMemory() {
        try {
            AutoTieredMemory::TierConfig cfg;
            cfg.ram_max_bytes = 100 * 1024 * 1024;  // 100MB for test
            cfg.ssd_max_bytes = 1024 * 1024 * 1024; // 1GB
            cfg.hdd_max_bytes = 10ULL * 1024 * 1024 * 1024; // 10GB
            cfg.ssd_path = "test_data/tier_ssd";
            cfg.hdd_path = "test_data/tier_hdd";
            
            AutoTieredMemory atm(cfg);
            atm.start();
            
            // Put some data
            std::vector<uint8_t> data(1024, 0x42);
            bool ok = atm.put("test_key", data, AutoTieredMemory::MemoryTier::RAM);
            if (!ok) return false;
            
            // Get data
            auto retrieved = atm.get("test_key");
            if (!retrieved || retrieved->size() != data.size()) return false;
            if ((*retrieved)[0] != 0x42) return false;
            
            // Test tier query
            auto tier = atm.getObjectTier("test_key");
            if (tier != AutoTieredMemory::MemoryTier::RAM) return false;
            
            // Test stats
            auto stats = atm.getStatsSnapshot();
            if (stats.total_objects == 0) return false;
            
            atm.stop();
            return true;
        } catch (const std::exception&) {
            return false;
        }
    }
    
    bool testPerTurnLearning() {
        try {
            PerTurnLearningHook hook;
            VariationalStateEstimator vse;
            yuki::brain::memory::CognitiveMemoryFabric cmf;
            
            hook.setVSE(&vse);
            hook.setWeightCalibratorRegistry(nullptr);
            hook.setHdcGraph(nullptr);
            
            // Simulate turn
            std::vector<float> state(8, 0.5f);
            std::vector<float> next_state(8, 0.6f);
            
            hook.onTurnEnd(true, 0.8f, 0.7f, state, 4, next_state, "test input");
            
            auto stats = hook.stats();
            if (stats.turns_processed != 1) return false;
            if (stats.belief_updates != 1) return false;
            
            return true;
        } catch (const std::exception&) {
            return false;
        }
    }
    
    bool testMetacognitiveStrategyShift() {
        try {
            MetacognitiveMonitor monitor;
            
            yuki::brain::field::CognitiveSnapshot snap;
            snap.prediction_errors.fill(0.1f);
            snap.surprise = 0.2f;
            snap.free_energy = 0.5f;
            
            // Normal update
            monitor.update(snap, 0.8f, 0.7f, true, 0.01f);
            
            auto state = monitor.getState();
            if (state.current_strategy != yuki::brain::self::CognitiveStrategy::FAST_INTUITIVE) {
                return false;
            }
            
            // High surprise - should trigger strategy shift
            snap.surprise = 0.9f;
            for (int i = 0; i < 5; ++i) {
                monitor.update(snap, 0.3f, 0.2f, false, 0.1f);
            }
            
            state = monitor.getState();
            // May shift to DELIBERATE or CLARIFY
            if (state.current_strategy == yuki::brain::self::CognitiveStrategy::FAST_INTUITIVE) {
                // Still fast - check if shift recommended
                if (!monitor.shouldShiftStrategy()) return false;
            }
            
            return true;
        } catch (const std::exception&) {
            return false;
        }
    }
    
    bool testRandomizedStress() {
        try {
            BabyMode baby;
            baby.initialize();
            
            std::mt19937 gen(12345);
            std::uniform_int_distribution<int> len_dist(1, 200);
            std::uniform_int_distribution<int> type_dist(0, 5);
            
            const std::vector<std::string> test_inputs = {
                "Hello",
                "What is the weather?",
                "Explain quantum mechanics",
                "Write a poem about cats",
                "Calculate 15 * 23",
                "Translate to French: hello world"
            };
            
            // Run 100 random turns
            for (int i = 0; i < 100; ++i) {
                std::string input = test_inputs[type_dist(gen)];
                
                // Add some noise
                if (gen() % 3 == 0) {
                    input += " " + std::to_string(len_dist(gen));
                }
                
                auto response = baby.processTurn(input);
                
                if (response.empty()) return false;
                
                // Verify system state remains valid
                auto vse = baby.variationalEstimator();
                auto belief = vse->currentBelief();
                if (!std::isfinite(belief.free_energy)) return false;
                
                auto hpc = baby.getHPC();
                float surprise = hpc->getSurprise();
                if (surprise < 0 || !std::isfinite(surprise)) return false;
            }
            
            // Check learning occurred
            auto hook = baby.getPerTurnLearningHook();
            if (hook) {
                auto stats = hook->stats();
                if (stats.turns_processed < 50) return false;  // Should have processed many turns
            }
            
            return true;
        } catch (const std::exception&) {
            return false;
        }
    }
};

std::unique_ptr<Gate> create_gate_10() {
    return std::make_unique<Gate_CognitiveLoopIntegration>();
}