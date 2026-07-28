# YUKI Autonomy Test Migration & Absorption Matrix

**Migration Date**: 2026-07-28  
**Autonomy Wave**: `AUTONOMY_PLAN_WAVE_1`  
**Active Integration Test Target**: `tests/testautonomyfullrandomized.cpp`

---

## 1. Migration Overview

To ensure single-target deterministic test execution while preserving all historical unit tests, all 29 pre-autonomy test sources have been archived into `not_in_use/test_files/legacy_pre_autonomy_wave/`. Their verification coverage has been absorbed into the unified deterministic-randomized full-system integration test `tests/testautonomyfullrandomized.cpp`.

---

## 2. Test File Inventory & Absorption Status

| Index | Original Path | Archived Location | Regression Status | Absorbed Coverage Target |
|:---|:---|:---|:---|:---|
| 1 | `tests/test_analogical_reasoning.cpp` | `not_in_use/test_files/legacy_pre_autonomy_wave/test_analogical_reasoning.cpp` | Archived | `testautonomyfullrandomized` (Phase 2: Intent & Reason) |
| 2 | `tests/test_autonomous_ingestor.cpp` | `not_in_use/test_files/legacy_pre_autonomy_wave/test_autonomous_ingestor.cpp` | Archived | `testautonomyfullrandomized` (Phase 4: Ingestion & Knowledge) |
| 3 | `tests/test_concept_blender.cpp` | `not_in_use/test_files/legacy_pre_autonomy_wave/test_concept_blender.cpp` | Archived | `testautonomyfullrandomized` (Phase 2: Intent & Reason) |
| 4 | `tests/test_conceptnet_adapter.cpp` | `not_in_use/test_files/legacy_pre_autonomy_wave/test_conceptnet_adapter.cpp` | Archived | `testautonomyfullrandomized` (Phase 4: Knowledge Subsystem) |
| 5 | `tests/test_counterfactual_replay_enhanced.cpp` | `not_in_use/test_files/legacy_pre_autonomy_wave/test_counterfactual_replay_enhanced.cpp` | Archived | `testautonomyfullrandomized` (Phase 3: Belief & Replay) |
| 6 | `tests/test_counterfactual_simulator.cpp` | `not_in_use/test_files/legacy_pre_autonomy_wave/test_counterfactual_simulator.cpp` | Archived | `testautonomyfullrandomized` (Phase 3: Hypothesis Engine) |
| 7 | `tests/test_creative_search.cpp` | `not_in_use/test_files/legacy_pre_autonomy_wave/test_creative_search.cpp` | Archived | `testautonomyfullrandomized` (Phase 5: Research & Tools) |
| 8 | `tests/test_dream_engine.cpp` | `not_in_use/test_files/legacy_pre_autonomy_wave/test_dream_engine.cpp` | Archived | `testautonomyfullrandomized` (Phase 6: Sleep & Consolidation) |
| 9 | `tests/test_grammar_extractor.cpp` | `not_in_use/test_files/legacy_pre_autonomy_wave/test_grammar_extractor.cpp` | Archived | `testautonomyfullrandomized` (Phase 1: Input Analysis) |
| 10 | `tests/test_hdc_batch_encoder.cpp` | `not_in_use/test_files/legacy_pre_autonomy_wave/test_hdc_batch_encoder.cpp` | Archived | `testautonomyfullrandomized` (Phase 4: HDC Memory) |
| 11 | `tests/test_identity_persistence.cpp` | `not_in_use/test_files/legacy_pre_autonomy_wave/test_identity_persistence.cpp` | Archived | `testautonomyfullrandomized` (Phase 7: Evolution & Identity) |
| 12 | `tests/test_integration_orchestrator.cpp` | `not_in_use/test_files/legacy_pre_autonomy_wave/test_integration_orchestrator.cpp` | Archived | `testautonomyfullrandomized` (Phase 1: Turn Coordinator) |
| 13 | `tests/test_knowledge_filter.cpp` | `not_in_use/test_files/legacy_pre_autonomy_wave/test_knowledge_filter.cpp` | Archived | `testautonomyfullrandomized` (Phase 4: Knowledge Filtering) |
| 14 | `tests/test_knowledge_integration.cpp` | `not_in_use/test_files/legacy_pre_autonomy_wave/test_knowledge_integration.cpp` | Archived | `testautonomyfullrandomized` (Phase 4: Knowledge Memory) |
| 15 | `tests/test_m10_integration.cpp` | `not_in_use/test_files/legacy_pre_autonomy_wave/test_m10_integration.cpp` | Archived | `testautonomyfullrandomized` (Full System Pipeline) |
| 16 | `tests/test_m11_integration.cpp` | `not_in_use/test_files/legacy_pre_autonomy_wave/test_m11_integration.cpp` | Archived | `testautonomyfullrandomized` (Full System Pipeline) |
| 17 | `tests/test_m12_full_integration.cpp` | `not_in_use/test_files/legacy_pre_autonomy_wave/test_m12_full_integration.cpp` | Archived | `testautonomyfullrandomized` (Full System Pipeline) |
| 18 | `tests/test_metaphor_engine.cpp` | `not_in_use/test_files/legacy_pre_autonomy_wave/test_metaphor_engine.cpp` | Archived | `testautonomyfullrandomized` (Phase 2: Cognitive Intent) |
| 19 | `tests/test_multimodal_encoder.cpp` | `not_in_use/test_files/legacy_pre_autonomy_wave/test_multimodal_encoder.cpp` | Archived | `testautonomyfullrandomized` (Phase 1: Perception Encoding) |
| 20 | `tests/test_p1_p2_integration.cpp` | `not_in_use/test_files/legacy_pre_autonomy_wave/test_p1_p2_integration.cpp` | Archived | `testautonomyfullrandomized` (Phase 1: Turn Pipeline) |
| 21 | `tests/test_physics_knowledge_base.cpp` | `not_in_use/test_files/legacy_pre_autonomy_wave/test_physics_knowledge_base.cpp` | Archived | `testautonomyfullrandomized` (Phase 4: Physics Knowledge) |
| 22 | `tests/test_physics_world.cpp` | `not_in_use/test_files/legacy_pre_autonomy_wave/test_physics_world.cpp` | Archived | `testautonomyfullrandomized` (Phase 4: Physics Sim) |
| 23 | `tests/test_self_play_engine.cpp` | `not_in_use/test_files/legacy_pre_autonomy_wave/test_self_play_engine.cpp` | Archived | `testautonomyfullrandomized` (Phase 5: Self Play) |
| 24 | `tests/test_structural_causal_model.cpp` | `not_in_use/test_files/legacy_pre_autonomy_wave/test_structural_causal_model.cpp` | Archived | `testautonomyfullrandomized` (Phase 3: Causal Model) |
| 25 | `tests/test_system_benchmark.cpp` | `not_in_use/test_files/legacy_pre_autonomy_wave/test_system_benchmark.cpp` | Archived | `testautonomyfullrandomized` (Phase 7: Performance Benchmark) |
| 26 | `tests/test_vae.cpp` | `not_in_use/test_files/legacy_pre_autonomy_wave/test_vae.cpp` | Archived | `testautonomyfullrandomized` (Phase 2: VAE Generator) |
| 27 | `tests/test_vae_response_generator.cpp` | `not_in_use/test_files/legacy_pre_autonomy_wave/test_vae_response_generator.cpp` | Archived | `testautonomyfullrandomized` (Phase 2: VAE Generation) |
| 28 | `tests/test_value_constitution.cpp` | `not_in_use/test_files/legacy_pre_autonomy_wave/test_value_constitution.cpp` | Archived | `testautonomyfullrandomized` (Phase 8: Safety & Values) |
| 29 | `tests/test_zero_hardcoding.cpp` | `not_in_use/test_files/legacy_pre_autonomy_wave/test_zero_hardcoding.cpp` | Archived | `testautonomyfullrandomized` (Phase 8: Config Verification) |
