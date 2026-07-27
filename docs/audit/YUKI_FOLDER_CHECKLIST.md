# YUKI v1.0 — Folder-by-Folder Build Checklist

> **Generated:** 2026-07-28  
> **Source:** `CMakeLists.txt` Audit  
> **Architecture Frame:** Autonomous Digital Organism / Artificial Mind Architecture

---

## 1. `src/` (Root Component Directory)
**Purpose:** Primary system orchestration, main entry points, shell presentation, and root subsystem bridges.
- `src/YukiUtils.cpp`
- `src/SubsystemControl.cpp`
- `src/CommandRouter.cpp`
- `src/PresenceShell.cpp`
- `src/DetailView.cpp`
- `src/AvatarBody.cpp`
- `src/AvatarRenderer.cpp`
- `src/IntentScorer.cpp`
- `src/ResponseEngine.cpp`
- `src/NeuralSpine.cpp`
- `src/AutoSensor.cpp`
- `src/BabyMode.cpp`
- `src/main.cpp` (Target: `yuki` executable)

## 2. `src/input/`
**Purpose:** Perceptual input intake, multimodal sensor capture (audio, visual, speech, screen, camera), and wake monitoring.
- `src/input/InputLayer.cpp`
- `src/input/PerceptionLayer.cpp`
- `src/input/Ear.cpp`
- `src/input/Mouth.cpp`
- `src/input/VisionSystem.cpp`
- `src/input/CameraRuntime.cpp`
- `src/input/ScreenRuntime.cpp`
- `src/input/SpeechSystem.cpp`
- `src/input/VoiceEngine.cpp`
- `src/input/WakeDetector.cpp`
- `src/input/InputAnalyzer.cpp`

## 3. `src/input/conditioning/`
**Purpose:** Low-level sensory signal normalization, artifact filtering, temporal alignment, and snapshot calibration.
- `src/input/conditioning/ConditionedSnapshot.cpp`
- `src/input/conditioning/SensorCalibrationProfile.cpp`
- `src/input/conditioning/SignalNormalizer.cpp`
- `src/input/conditioning/ArtifactFilter.cpp`
- `src/input/conditioning/ChangeDetector.cpp`
- `src/input/conditioning/TemporalAligner.cpp`
- `src/input/conditioning/SignalConditioningLayer.cpp`

## 4. `src/input/encoding/`
**Purpose:** Multimodal encoding, spatial anchoring, audio DSP, and fusion gating of sensory observations.
- `src/input/encoding/SpatialAnchor.cpp`
- `src/input/encoding/SensoryObservation.cpp`
- `src/input/encoding/AudioDSP.cpp`
- `src/input/encoding/ObservationEncoder.cpp`
- `src/input/encoding/MultiModalFusionGate.cpp`
- `src/input/encoding/TextEncoder.cpp`
- `src/input/encoding/VisualEncoder.cpp`

## 5. `src/brain/`
**Purpose:** Top-level cognitive sub-modules, executive action tools, scraper, knowledge base, and system execution helpers.
- `src/brain/ToolExecutor.cpp`
- `src/brain/BackgroundAgents.cpp`
- `src/brain/MobileServer.cpp`
- `src/brain/LanguageLayer.cpp`
- `src/brain/LanguageSynthesizer.cpp`
- `src/brain/InputNormalizer.cpp`
- `src/brain/CandidateGenerator.cpp`
- `src/brain/EntityProcessor.cpp`
- `src/brain/RequestClassifier.cpp`
- `src/brain/GoalBuilder.cpp`
- `src/brain/ActionRouter.cpp`
- `src/brain/SafetyGovernor.cpp`
- `src/brain/SmartScraper.cpp`
- `src/brain/ResponseActPlanner.cpp`
- `src/brain/LocalKnowledgeBase.cpp`
- `src/brain/KnowledgeRouter.cpp`
- `src/brain/CapabilityMap.cpp`
- `src/brain/KnowledgeExtractor.cpp`
- `src/brain/DependencyInstaller.cpp`
- `src/brain/SystemExecutor.cpp`
- `src/brain/ScriptRunner.cpp`
- `src/brain/FileOperator.cpp`
- `src/brain/UIAutomationController.cpp`
- `src/brain/VerificationEngine.cpp`

## 6. `src/brain/core/`
**Purpose:** Central executive controllers, configuration management, logging, intent classification, benchmark, and warm-up engines.
- `src/brain/core/ConfigManager.cpp`
- `src/brain/core/ResponseResolver.cpp`
- `src/brain/core/IntentClassifier.cpp`
- `src/brain/core/SystemWarmUp.cpp`
- `src/brain/core/IntegrationOrchestrator.cpp`
- `src/brain/core/SystemBenchmark.cpp`
- `src/brain/core/EventLoopCore.cpp`
- `src/brain/core/Logger.cpp`
- `src/brain/core/KnowledgeIngestionOrchestrator.cpp`

## 7. `src/brain/core/event/`
**Purpose:** High-throughput lock-free MPMC queue primitives for real-time cognitive event processing.
- `src/brain/core/event/LockFreeEventQueue.cpp`

## 8. `src/brain/core/io/`
**Purpose:** Asynchronous I/O subsystem for non-blocking file, network, and memory transactions.
- `src/brain/core/io/AsyncIO.cpp`

## 9. `src/brain/reasoning/`
**Purpose:** Symbolic reasoning, pattern evaluation, task context tracking, evidence synthesis, goal parsing, and analogical logic.
- `src/brain/reasoning/PatternEngine.cpp`
- `src/brain/reasoning/TaskContext.cpp`
- `src/brain/reasoning/EvidenceSystem.cpp`
- `src/brain/reasoning/SynthesisEngine.cpp`
- `src/brain/reasoning/TaskSystem.cpp`
- `src/brain/reasoning/InputResolution.cpp`
- `src/brain/reasoning/SemanticParser.cpp`
- `src/brain/reasoning/GoalModel.cpp`
- `src/brain/reasoning/AnalogicalReasoning.cpp`

## 10. `src/brain/memory/`
**Purpose:** Multi-store memory fabric (Hyperdimensional Memory, SDM, Episodic, Context, Merkle DAG, Semantic Graph, Archive Writer, DMC).
- `src/brain/memory/ContextMemory.cpp`
- `src/brain/memory/AuditSystem.cpp`
- `src/brain/memory/UserMemory.cpp`
- `src/brain/memory/MemoryEncoder.cpp`
- `src/brain/memory/EpisodicStore.cpp`
- `src/brain/memory/CognitiveMemoryFabric.cpp`
- `src/brain/memory/KnowledgeStore.cpp`
- `src/brain/memory/ConceptNetIngestor.cpp`
- `src/brain/memory/MultimodalEncoder.cpp`
- `src/brain/memory/Hypervector.cpp`
- `src/brain/memory/SparseDistributedMemory.cpp`
- `src/brain/memory/LocalitySensitiveHash.cpp`
- `src/brain/memory/HypervectorEncoder.cpp`
- `src/brain/memory/HdcSemanticGraph.cpp`
- `src/brain/memory/MerkleDAG.cpp`
- `src/brain/memory/SemanticGraph.cpp`
- `src/brain/memory/TinyMLP.cpp`
- `src/brain/memory/DifferentialMemoryController.cpp`
- `src/brain/memory/ProceduralStore.cpp`
- `src/brain/memory/InformationGainEngine.cpp`
- `src/brain/memory/ActiveInferenceRetrieval.cpp`
- `src/brain/memory/PromotionMetrics.cpp`
- `src/brain/memory/ColumnarArchiveFormat.cpp`
- `src/brain/memory/ArchiveWriter.cpp`
- `src/brain/memory/SdmOptimizer.cpp`
- `src/brain/memory/ChainReconstructor.cpp`
- `src/brain/memory/MemoryFabric.cpp`
- `src/brain/memory/MemoryDistiller.cpp`
- `src/brain/memory/DMCController.cpp`
- `src/brain/memory/DMCMemoryAccess.cpp`
- `src/brain/memory/NeuralPopulation.cpp`
- `src/brain/memory/ContextManager.cpp`
- `src/brain/memory/UserProfile.cpp`
- `src/brain/memory/HdcBatchEncoder.cpp`

## 11. `src/brain/database/`
**Purpose:** Persistent SQLite database abstraction and universal cache management layer.
- `src/brain/database/DatabaseManager.cpp`
- `src/brain/database/UniversalCache.cpp`

## 12. `src/brain/learning/`
**Purpose:** Knowledge daemon, autonomous ingestion loaders, embedding engine, and background learning controller.
- `src/brain/learning/KnowledgeDaemon.cpp`
- `src/brain/learning/LearningIngestor.cpp`
- `src/brain/learning/MassCurriculumLoader.cpp`
- `src/brain/learning/EmbeddingEngine.cpp`
- `src/brain/learning/BackgroundLearningEngine.cpp`

## 13. `src/brain/learning/selfplay/`
**Purpose:** Autonomous self-play dialogue generation and cognitive reinforcement exploration.
- `src/brain/learning/selfplay/SelfPlayEngine.cpp`

## 14. `src/brain/learning/generative/`
**Purpose:** Variational Autoencoder (VAE) representation learning and latent space sampling.
- `src/brain/learning/generative/VariationalAutoencoder.cpp`

## 15. `src/brain/learning/neural/`
**Purpose:** C++ native neural network primitives (Matrix, DenseLayer, Loss, Optimizers, Q-Learning, EWC Trainer, Meta-Learner).
- `src/brain/learning/neural/Matrix.cpp`
- `src/brain/learning/neural/Activation.cpp`
- `src/brain/learning/neural/DenseLayer.cpp`
- `src/brain/learning/neural/Loss.cpp`
- `src/brain/learning/neural/Optimizer.cpp`
- `src/brain/learning/neural/NeuralNetwork.cpp`
- `src/brain/learning/neural/QLearningCore.cpp`
- `src/brain/learning/neural/RewardShaper.cpp`
- `src/brain/learning/neural/CurriculumGenerator.cpp`
- `src/brain/learning/neural/EWCTrainer.cpp`
- `src/brain/learning/neural/MetaLearner.cpp`
- `src/brain/learning/neural/NeuralBootstrap.cpp`

## 16. `src/brain/language/`
**Purpose:** Language understanding, Word2Vec vector space, PCFG probabilistic grammar, local ONNX/Ollama transformer client, and metaphor engine.
- `src/brain/language/Word2Vec.cpp`
- `src/brain/language/VaeResponseGenerator.cpp`
- `src/brain/language/GrammarEngine.cpp`
- `src/brain/language/LocalLLM.cpp` *(Registered twice in CMakeLists.txt!)*
- `src/brain/language/MetaphorEngine.cpp`
- `src/brain/language/SentenceMaker.cpp`
- `src/brain/language/SentenceBuilder.cpp`
- `src/brain/language/EnglishLanguageEngine.cpp`
- `src/brain/language/GrammarExtractor.cpp`

## 17. `src/brain/retrieval/`
**Purpose:** Hybrid retrieval stack, confidence-driven web search router, and vector store retrieval.
- `src/brain/retrieval/RetrievalSystem.cpp`
- `src/brain/retrieval/VectorStore.cpp`

## 18. `src/brain/skills/`
**Purpose:** Executive skill execution engine and skill registry catalog.
- `src/brain/skills/SkillSystem.cpp`
- `src/brain/skills/SkillRegistry.cpp`

## 19. `src/brain/emotion/`
**Purpose:** Valence-arousal affect state model and emotional bias modulation.
- `src/brain/emotion/EmotionSystem.cpp`

## 20. `src/brain/world/`
**Purpose:** Declarative physics engine, intuitive mechanics, and world model simulation bridge.
- `src/brain/world/PhysicsWorld.cpp`
- `src/brain/world/WorldModelBridge.cpp`

## 21. `src/brain/predictive/`
**Purpose:** Real-time predictive turn coordinator, salience gate, response shaper, and streaming worker threads.
- `src/brain/predictive/predictive_turn_engine.cpp`
- `src/brain/predictive/IntentResponseRouter.cpp`
- `src/brain/predictive/stream_workers.cpp`
- `src/brain/predictive/error_functions.cpp`
- `src/brain/predictive/salience_gate.cpp`
- `src/brain/predictive/response_shaper.cpp`
- `src/brain/predictive/memory_store.cpp`
- `src/brain/predictive/sqlite_memory_store.cpp`
- `src/brain/predictive/tool_adapter.cpp`
- `src/brain/predictive/StageCommitController.cpp`
- `src/brain/predictive/TurnCoordinator.cpp`

## 22. `src/brain/inference/`
**Purpose:** Active Inference (Friston Free Energy Principle), Variational State Estimator, belief state updates, and precision prediction.
- `src/brain/inference/PrecisionEngine.cpp`
- `src/brain/inference/BeliefState.cpp`
- `src/brain/inference/GenerativeModel.cpp`
- `src/brain/inference/PolicySelector.cpp`
- `src/brain/inference/VariationalStateEstimator.cpp`
- `src/brain/inference/BeliefUpdater.cpp`
- `src/brain/inference/PrecisionPredictor.cpp`
- `src/brain/inference/VseBootstrapTrainer.cpp`

## 23. `src/brain/security/`
**Purpose:** Path normalization, security sandbox isolation, integrity monitoring, and user approval gates.
- `src/brain/security/SecuritySandbox.cpp`
- `src/brain/security/PathNormalizer.cpp`
- `src/brain/security/IntegrityMonitor.cpp`
- `src/brain/security/ApprovalGate.cpp`

## 24. `src/brain/selftest/`
**Purpose:** Internal boot-time self-test diagnostic harness.
- `src/brain/selftest/SelfTestHarness.cpp`

## 25. `src/brain/metacognition/`
**Purpose:** Competence tracking, cognitive audit log, improvement graph, and policy divergence monitoring.
- `src/brain/metacognition/CompetenceRecord.cpp`
- `src/brain/metacognition/MetacognitionEngine.cpp`
- `src/brain/metacognition/CognitiveAuditLog.cpp`
- `src/brain/metacognition/ImprovementGraph.cpp`
- `src/brain/metacognition/PolicyDivergenceLogger.cpp`

## 26. `src/brain/policy/`
**Purpose:** Cognitive policy selector for action strategy switching.
- `src/brain/policy/PolicySelector.cpp`

## 27. `src/brain/persistence/`
**Purpose:** State serialization and checkpoint recovery.
- `src/brain/persistence/StateSerializer.cpp`

## 28. `src/brain/synthesis/`
**Purpose:** Code synthesis agent for dynamic internal code generation.
- `src/brain/synthesis/CodeSynthesisAgent.cpp`

## 29. `src/brain/logic/`
**Purpose:** DPLL propositional logic solver and formal SAT inference.
- `src/brain/logic/PropositionalEngine.cpp`

## 30. `src/brain/causality/`
**Purpose:** Judea Pearl `do(X=x)` causal graph engine.
- `src/brain/causality/CausalGraph.cpp`

## 31. `src/brain/planning/`
**Purpose:** Hierarchical Task Network (HTN) planner.
- `src/brain/planning/HtnPlanner.cpp`

## 32. `src/brain/self/`
**Purpose:** Self-model tracking and Theory of Mind agent simulation.
- `src/brain/self/SelfModel.cpp` *(Registered twice in CMakeLists.txt!)*
- `src/brain/self/TheoryOfMind.cpp`

## 33. `src/brain/organism/`
**Purpose:** Metabolism, digital organism drives, energy economy, proactive engine, and confidence calibration.
- `src/brain/organism/MetabolismEngine.cpp`
- `src/brain/organism/EconomyEngine.cpp`
- `src/brain/organism/DriveSystem.cpp`
- `src/brain/organism/OrganismController.cpp`
- `src/brain/organism/ConfidenceCalibrator.cpp`
- `src/brain/organism/ProactiveEngine.cpp`

## 34. `src/brain/creativity/`
**Purpose:** Conceptual blending and creative solution space search.
- `src/brain/creativity/ConceptBlender.cpp`
- `src/brain/creativity/CreativeSearch.cpp`

## 35. `src/brain/causal/`
**Purpose:** Structural Causal Models (SCM) and counterfactual simulator.
- `src/brain/causal/StructuralCausalModel.cpp`
- `src/brain/causal/CounterfactualSimulator.cpp`

## 36. `src/infrastructure/`
**Purpose:** CoreBus event routing, Global Workspace attention blackboard, and ControlPlane orchestration.
- `src/infrastructure/CoreBus.cpp`
- `src/infrastructure/GlobalWorkspace.cpp`
- `src/infrastructure/ModuleRegistry.cpp`
- `src/infrastructure/ControlPlane.cpp`

## 37. `src/brain/sleep/`
**Purpose:** Offline sleep thread maintenance, episode distillation, and memory consolidation.
- `src/brain/sleep/SleepThread.cpp`
- `src/brain/sleep/SleepConsolidator.cpp`

## 38. `src/vendor/sqlite/`
**Purpose:** Embedded SQLite 3 database C library source.
- `src/vendor/sqlite/sqlite3.c`

## 39. `src/brain/research/core/`
**Purpose:** Research planner, tool registry, and multi-source synthesizer.
- `src/brain/research/core/ResearchPlanner.cpp`
- `src/brain/research/core/ToolRegistry.cpp`
- `src/brain/research/core/Synthesizer.cpp`

## 40. `src/brain/research/`
**Purpose:** Autonomous research agent, risk gating, and knowledge packs.
- `src/brain/research/RiskGate.cpp`
- `src/brain/research/ResearchAgent.cpp`
- `src/brain/research/KnowledgePack.cpp`

## 41. `src/brain/research/tools/`
**Purpose:** External research execution tools (Web, GitHub, ArXiv, API, Sandbox, Compute, Image Recognition).
- `src/brain/research/tools/WebSearchTool.cpp`
- `src/brain/research/tools/GitHubSearchTool.cpp`
- `src/brain/research/tools/GitHubReadTool.cpp`
- `src/brain/research/tools/ArXivSearchTool.cpp`
- `src/brain/research/tools/APICallTool.cpp`
- `src/brain/research/tools/SandboxExecuteTool.cpp`
- `src/brain/research/tools/FileReadTool.cpp`
- `src/brain/research/tools/ComputeTool.cpp`
- `src/brain/research/tools/ImageRecognitionTool.cpp`

## 42. `src/brain/research/discovery/`
**Purpose:** Dynamic tool discovery and schema registration.
- `src/brain/research/discovery/ToolDiscovery.cpp`

## 43. `src/brain/testing/`
**Purpose:** Internal self-testing framework (Orchestrator, TestSuite DAG, Historical Replay, AB Testing, Smart Test Selector).
- `src/brain/testing/TestOrchestrator.cpp`
- `src/brain/testing/TestSuiteDAG.cpp`
- `src/brain/testing/HistoricalDataReplay.cpp`
- `src/brain/testing/ABTestFramework.cpp`
- `src/brain/testing/SmartTestSelector.cpp`
- `src/brain/testing/TestResultPack.cpp`

## 44. `src/brain/testing/data/`
**Purpose:** Synthetic data generator for automated verification.
- `src/brain/testing/data/SyntheticDataSource.cpp`

## 45. `src/brain/testing/metrics/`
**Purpose:** Quantitative metric calculation for test evaluations.
- `src/brain/testing/metrics/MetricCalculator.cpp`

## 46. `src/brain/introspection/`
**Purpose:** Dynamic cognitive profiler and self-introspection tool.
- `src/brain/introspection/DynamicProfiler.cpp`
- `src/brain/introspection/SelfIntrospectionTool.cpp`

## 47. `src/brain/system/`
**Purpose:** Hardware resource monitoring, background job engine, and system controller.
- `src/brain/system/ResourceMonitor.cpp`
- `src/brain/system/CognitiveOrchestrator.cpp` *(Duplicate filename with `src/brain/ync/CognitiveOrchestrator.cpp`!)*
- `src/brain/system/SystemController.cpp`
- `src/brain/system/BackgroundJobEngine.cpp`

## 48. `src/brain/action/core/`
**Purpose:** Action planning, multi-step execution, and rollback transaction manager.
- `src/brain/action/core/ActionPlan.cpp`
- `src/brain/action/core/ActionPlanner.cpp`
- `src/brain/action/core/ActionExecutor.cpp`
- `src/brain/action/core/RollbackManager.cpp`

## 49. `src/brain/action/tools/`
**Purpose:** Action tool primitives (SeedTools, PopupUI, PythonInterpreterTool, OpenAppTool).
- `src/brain/action/tools/SeedTools.cpp`
- `src/brain/action/tools/PopupUI.cpp`
- `src/brain/action/tools/PythonInterpreterTool.cpp`
- `src/brain/action/tools/OpenAppTool.cpp`

## 50. `src/brain/capability/`
**Purpose:** Capability graph, pathfinder, matcher, sequencing engine, and resource optimizer.
- `src/brain/capability/CapabilityProfile.cpp`
- `src/brain/capability/CapabilityGraph.cpp`
- `src/brain/capability/CapabilityMatcher.cpp`
- `src/brain/capability/PathFinder.cpp`
- `src/brain/capability/ResourceOptimizer.cpp`
- `src/brain/capability/SequencingEngine.cpp`

## 51. `src/brain/cortex/`
**Purpose:** Parallel Cortical Module background daemons.
- `src/brain/cortex/CognitiveDaemon.cpp`

## 52. `src/brain/ync/`
**Purpose:** YUKI Neuromorphic Core (SNN LIF Spiking Simulator, Growth Cone, Developmental Engine, Training Supervisor).
- `src/brain/ync/Neuron.cpp`
- `src/brain/ync/GrowthCone.cpp`
- `src/brain/ync/DevelopmentalEngine.cpp`
- `src/brain/ync/NeuromorphicSimulator.cpp`
- `src/brain/ync/YNCCheckpoint.cpp`
- `src/brain/ync/YNCPipelineBridge.cpp`
- `src/brain/ync/YNCTrainingSupervisor.cpp`
- `src/brain/ync/CognitiveOrchestrator.cpp` *(Duplicate filename with `src/brain/system/CognitiveOrchestrator.cpp`!)*

## 53. `src/brain/knowledge/`
**Purpose:** Mass Knowledge Ingestion Pipeline (ConceptNet adapter, Knowledge filter, Physics KB, Autonomous ingestor).
- `src/brain/knowledge/ConceptNetAdapter.cpp`
- `src/brain/knowledge/KnowledgeFilter.cpp`
- `src/brain/knowledge/PhysicsKnowledgeBase.cpp`
- `src/brain/knowledge/AutonomousIngestor.cpp`

## 54. `src/brain/ethics/`
**Purpose:** Principles, value constitution, and ethical constraint evaluation.
- `src/brain/ethics/ValueConstitution.cpp`

## 55. `not_in_use/test_files/` (67 Target Executables)
**Purpose:** Historical & specialized unit test suites maintained for targeted regression testing.
*(Includes: test_screen_null_guard, test_ear_stall, test_precision_predictor, test_predictive_turn_engine, test_security_sandbox, test_selftest_harness, test_metacognition, test_cognitive_closure, test_code_synthesis, test_integration_closed_loop, test_script_runner_fix, test_candidate_generator_fix, test_research_planner, test_tool_registry, test_synthesizer, test_risk_gate, test_research_agent, test_tool_discovery, test_image_recognition, test_chain_reconstructor, test_memory_fabric, test_test_orchestrator, test_historical_replay, test_ab_framework, test_smart_test_selector, test_dynamic_profiler, test_self_introspection, test_integrity_monitor, test_resource_monitor, test_synthetic_data_source, test_action_planner, test_action_executor, test_rollback_manager, test_execution_report, test_action_risk_gate, test_integration_action_research, test_aggressive_audit, test_system_warmup, test_memory_fabric_action_plan, test_capability_graph, test_backfill_behavioral, test_neural_matrix, test_neural_network, test_q_learning, test_ewc_trainer, test_neural_bootstrap, test_neural_population, test_neural_corebus, test_parallel_memory, test_global_workspace_binding, test_learned_ensemble, test_policy_selector_integration, test_ync_neuron, test_ync_growthcone, test_ync_neuromodulator, test_ync_development, test_ync_simulator, test_ync_checkpoint, test_ync_bridge, test_ync_orchestrator, test_ync_training, test_ync_full_integration, test_audit_memory_order, test_audit_thread_safety, test_audit_resource_leak, test_propositional_engine, test_causal_graph, test_htn_planner, test_m8_causal_logic_integration, test_m8_htn_causal_integration, test_m8_full_pipeline_integration, test_ync_burnin_stress, test_ync_large_scale, test_ync_checkpoint_recovery, test_ync_concurrency_fuzz, test_ync_thermal, test_ync_sparse_activation, test_self_model, test_theory_of_mind, test_valence_arousal, test_drive_system, test_confidence_calibrator, test_m9_integration, test_system_controller, test_wake_detector, test_proactive_engine, test_background_job_engine, test_sentence_maker, test_context_manager, test_input_analyzer, test_user_profile, test_tools_y2k, test_logger, test_y2k_full_integration).*

## 56. `tests/` (22 Target Executables)
**Purpose:** Modern active integration and milestone test suite.
*(Includes: test_concept_blender, test_creative_search, test_vae, test_identity_persistence, test_dream_engine, test_m10_integration, test_structural_causal_model, test_counterfactual_simulator, test_analogical_reasoning, test_metaphor_engine, test_m11_integration, test_integration_orchestrator, test_system_benchmark, test_zero_hardcoding, test_multimodal_encoder, test_self_play_engine, test_counterfactual_replay_enhanced, test_vae_response_generator, test_physics_world, test_p1_p2_integration, test_conceptnet_adapter, test_knowledge_filter, test_grammar_extractor, test_physics_knowledge_base, test_value_constitution, test_hdc_batch_encoder, test_autonomous_ingestor, test_knowledge_integration).*
