# YUKI CODEBASE AUDIT REPORT

## STEP 1: FILE INVENTORY
| File | Lines | Header Guard | Status | Classes |
|---|---|---|---|---|
| src\AutoSensor.cpp | 115 | N/A | 🔴 STUB |  |
| src\AutoSensor.h | 11 | True | 🔴 STUB | BabyMode |
| src\AvatarBody.cpp | 380 | N/A | 🟡 PARTIAL |  |
| src\AvatarBody.h | 49 | True | 🔴 STUB | AvatarBody |
| src\AvatarRenderer.cpp | 447 | N/A | 🔴 STUB |  |
| src\AvatarRenderer.h | 32 | True | 🔴 STUB | AvatarRenderer |
| src\BabyMode.cpp | 664 | N/A | 🟡 PARTIAL | DmcEvalResult |
| src\BabyMode.h | 187 | True | 🔴 STUB | PresenceShell, BackgroundLearningEngine, YukiSelfM |
| src\CommandRouter.cpp | 307 | N/A | 🔴 STUB |  |
| src\CommandRouter.h | 44 | True | 🔴 STUB | PerceptionEvent, CommandResult, CommandRouter |
| src\DetailView.cpp | 377 | N/A | 🟡 PARTIAL |  |
| src\DetailView.h | 35 | True | 🔴 STUB | DetailView |
| src\IntentScorer.cpp | 253 | N/A | 🔴 STUB | Candidate |
| src\IntentScorer.h | 75 | True | 🔴 STUB | IntentKind, IntentResult, IntentScorer |
| src\Logger.h | 7 | True | 🔴 STUB |  |
| src\main.cpp | 475 | N/A | 🟢 REAL |  |
| src\NeuralSpine.cpp | 149 | N/A | 🔴 STUB |  |
| src\NeuralSpine.h | 93 | True | 🔴 STUB | SpineInput, SpineOutput, NeuralSpine |
| src\PresenceShell.cpp | 1538 | N/A | 🟡 PARTIAL | input |
| src\PresenceShell.h | 158 | True | 🔴 STUB | PresenceShell, CognitiveLayer, SubsystemControl, S |
| src\ResponseEngine.cpp | 129 | N/A | 🔴 STUB |  |
| src\ResponseEngine.h | 49 | True | 🔴 STUB | ResponseEngine |
| src\RuntimeWorkerBase.h | 16 | True | 🔴 STUB | RuntimeWorkerBase |
| src\SessionState.h | 19 | True | 🔴 STUB | ChatEntry, SessionState |
| src\SubsystemControl.cpp | 479 | N/A | 🔴 STUB |  |
| src\SubsystemControl.h | 128 | True | 🔴 STUB | SubsystemName, SubsystemMode, SubsystemRuntimeStat |
| src\YukiTestRunner.cpp | 143 | N/A | 🔴 STUB |  |
| src\YukiTestRunner.h | 27 | True | 🔴 STUB | TestCase, YukiTestRunner |
| src\YukiTestTypes.h | 78 | True | 🔴 STUB | YukiStage, TestCategory, TestMode, TestResult |
| src\YukiUtils.cpp | 50 | N/A | 🔴 STUB |  |
| src\YukiUtils.h | 33 | True | 🔴 STUB | FeatureFlags, TraceMode, CheckpointEvent, Checkpoi |
| src\brain\ActionRouter.cpp | 87 | N/A | 🔴 STUB |  |
| src\brain\ActionRouter.h | 20 | True | 🔴 STUB | ActionRouter |
| src\brain\BackgroundAgents.cpp | 252 | N/A | 🔴 STUB |  |
| src\brain\BackgroundAgents.h | 90 | True | 🔴 STUB | TaskState, BackgroundTask, BackgroundTaskManager,  |
| src\brain\BrainTypes.h | 297 | True | 🔴 STUB | CanonicalInputEvent, RequestMode, OutputMode, Patt |
| src\brain\CandidateGenerator.cpp | 102 | N/A | 🔴 STUB |  |
| src\brain\CandidateGenerator.h | 21 | True | 🔴 STUB | CandidateResult, CandidateGenerator |
| src\brain\CapabilityMap.cpp | 28 | N/A | 🔴 STUB |  |
| src\brain\CapabilityMap.h | 16 | True | 🔴 STUB | CapabilityMap |
| src\brain\DependencyInstaller.cpp | 32 | N/A | 🔴 STUB |  |
| src\brain\DependencyInstaller.h | 14 | True | 🔴 STUB | DependencyInstaller |
| src\brain\DocReader.cpp | 125 | N/A | 🔴 STUB | Source |
| src\brain\DocReader.h | 9 | True | 🔴 STUB | DocReader |
| src\brain\EntityProcessor.cpp | 264 | N/A | 🟢 REAL |  |
| src\brain\EntityProcessor.h | 26 | True | 🔴 STUB | EntitySpanDetector, UserMemory, EntityLinker |
| src\brain\ExecutionTypes.h | 111 | True | 🔴 STUB | ApprovalType, ApprovalRequest, CapabilityRecord, A |
| src\brain\FileOperator.cpp | 123 | N/A | 🔴 STUB |  |
| src\brain\FileOperator.h | 27 | True | 🔴 STUB | FileOperator |
| src\brain\GoalBuilder.cpp | 162 | N/A | 🔴 STUB |  |
| src\brain\GoalBuilder.h | 8 | True | 🔴 STUB | GoalBuilder |
| src\brain\InputNormalizer.cpp | 13 | N/A | 🔴 STUB |  |
| src\brain\InputNormalizer.h | 10 | True | 🔴 STUB | InputNormalizer |
| src\brain\KnowledgeExtractor.cpp | 142 | N/A | 🟡 PARTIAL |  |
| src\brain\KnowledgeExtractor.h | 31 | True | 🔴 STUB | ExtractedKnowledge, KnowledgeExtractor |
| src\brain\KnowledgeRecord.h | 14 | True | 🔴 STUB | KnowledgeRecord |
| src\brain\KnowledgeRouter.cpp | 140 | N/A | 🔴 STUB |  |
| src\brain\KnowledgeRouter.h | 15 | True | 🔴 STUB | KnowledgeRouter |
| src\brain\LanguageLayer.cpp | 169 | N/A | 🔴 STUB |  |
| src\brain\LanguageLayer.h | 13 | True | 🔴 STUB | LanguageLayer |
| src\brain\LanguageSynthesizer.cpp | 70 | N/A | 🔴 STUB |  |
| src\brain\LanguageSynthesizer.h | 21 | True | 🔴 STUB | LanguageSynthesizer |
| src\brain\LearningUpdate.cpp | 109 | N/A | 🔴 STUB |  |
| src\brain\LearningUpdate.h | 20 | True | 🔴 STUB | LearningUpdate |
| src\brain\LocalKnowledgeBase.cpp | 108 | N/A | 🔴 STUB |  |
| src\brain\LocalKnowledgeBase.h | 22 | True | 🔴 STUB | sqlite3, LocalKnowledgeBase |
| src\brain\MeaningTypes.h | 165 | True | 🔴 STUB | ConfidenceBehavior, DetectedLanguage, LanguageResu |
| src\brain\MobileServer.cpp | 391 | N/A | 🔴 STUB |  |
| src\brain\MobileServer.h | 69 | True | 🔴 STUB | MobileServer, HttpRequest |
| src\brain\MotherCore.h | 15 | True | 🔴 STUB | MotherCoreResult, MotherCore |
| src\brain\Phase1Tests.cpp | 599 | N/A | 🔴 STUB | TestCase |
| src\brain\Phase1Tests.h | 7 | True | 🔴 STUB | Phase1Tests |
| src\brain\RequestClassifier.cpp | 188 | N/A | 🔴 STUB |  |
| src\brain\RequestClassifier.h | 8 | True | 🔴 STUB | RequestClassifier |
| src\brain\ResponseActPlanner.cpp | 157 | N/A | 🔴 STUB |  |
| src\brain\ResponseActPlanner.h | 22 | True | 🔴 STUB | ResponseActPlanner |
| src\brain\SafetyGovernor.cpp | 34 | N/A | 🔴 STUB |  |
| src\brain\SafetyGovernor.h | 20 | True | 🔴 STUB | RiskClass, SafetyGovernor |
| src\brain\ScriptRunner.cpp | 88 | N/A | 🔴 STUB |  |
| src\brain\ScriptRunner.h | 18 | True | 🔴 STUB | ScriptRunner |
| src\brain\SmartScraper.cpp | 192 | N/A | 🟢 REAL | SmartScraper |
| src\brain\SmartScraper.h | 45 | True | 🔴 STUB | ScrapedPage, SmartScraper, Impl |
| src\brain\SystemExecutor.cpp | 62 | N/A | 🔴 STUB |  |
| src\brain\SystemExecutor.h | 18 | True | 🔴 STUB | SystemExecutor |
| src\brain\ToolExecutor.cpp | 245 | N/A | 🔴 STUB |  |
| src\brain\ToolExecutor.h | 49 | True | 🔴 STUB | ToolResult, ToolExecutor |
| src\brain\UIAutomationController.cpp | 60 | N/A | 🔴 STUB |  |
| src\brain\UIAutomationController.h | 19 | True | 🔴 STUB | UIAutomationController |
| src\brain\VerificationEngine.cpp | 57 | N/A | 🔴 STUB |  |
| src\brain\VerificationEngine.h | 13 | True | 🔴 STUB | VerificationEngine |
| src\brain\core\IntentClassifier.cpp | 72 | N/A | 🔴 STUB |  |
| src\brain\core\IntentClassifier.h | 34 | True | 🔴 STUB | GroundedIntent, IntentRoutingDecision, GroundedInt |
| src\brain\core\ResponseResolver.cpp | 156 | N/A | 🔴 STUB |  |
| src\brain\core\ResponseResolver.h | 39 | True | 🔴 STUB | TurnResult, ResponseResolver |
| src\brain\curiosity\CuriosityEngine.cpp | 33 | N/A | 🔴 STUB |  |
| src\brain\curiosity\CuriosityEngine.h | 26 | True | 🔴 STUB | EmotionState, KnowledgeStore, InternalQuestion, Cu |
| src\brain\database\DatabaseManager.cpp | 715 | N/A | 🔴 STUB |  |
| src\brain\database\DatabaseManager.h | 80 | True | 🔴 STUB | DatabaseManager |
| src\brain\database\UniversalCache.cpp | 90 | N/A | 🟡 PARTIAL |  |
| src\brain\database\UniversalCache.h | 42 | True | 🔴 STUB | ResponseTemplate, UniversalCache |
| src\brain\emotion\EmotionSystem.cpp | 387 | N/A | 🔴 STUB |  |
| src\brain\emotion\EmotionSystem.h | 67 | True | 🔴 STUB | EmotionSnapshot, EmotionState, UserMood, EmpathyRe |
| src\brain\inference\BeliefState.cpp | 100 | N/A | 🔴 STUB |  |
| src\brain\inference\BeliefState.h | 40 | True | 🔴 STUB | IntentClass, EngagementLevel, UrgencyLevel, Belief |
| src\brain\inference\FreeEnergyCalculator.cpp | 204 | N/A | 🔴 STUB | PolicyCache |
| src\brain\inference\FreeEnergyCalculator.h | 58 | True | 🔴 STUB | GenerativeModel, Policy, FreeEnergyCalculator |
| src\brain\inference\GenerativeModel.cpp | 250 | N/A | 🟡 PARTIAL |  |
| src\brain\inference\GenerativeModel.h | 58 | True | 🔴 STUB | GenerativeModel |
| src\brain\inference\PolicySelector.cpp | 188 | N/A | 🟡 PARTIAL |  |
| src\brain\inference\PolicySelector.h | 40 | True | 🔴 STUB | GenerativeModel, FreeEnergyCalculator, PolicyResul |
| src\brain\inference\PrecisionEngine.cpp | 58 | N/A | 🔴 STUB |  |
| src\brain\inference\PrecisionEngine.h | 36 | True | 🔴 STUB | PrecisionFactors, PrecisionEngine, BeliefState |
| src\brain\inference\VariationalStateEstimator.cpp | 72 | N/A | 🟡 PARTIAL |  |
| src\brain\inference\VariationalStateEstimator.h | 51 | True | 🔴 STUB | VariationalStateEstimator |
| src\brain\learning\BackgroundLearningEngine.cpp | 164 | N/A | 🔴 STUB |  |
| src\brain\learning\BackgroundLearningEngine.h | 75 | True | 🔴 STUB | CognitiveMemoryFabric, TextEncoder, VariationalSta |
| src\brain\learning\EmbeddingEngine.cpp | 147 | N/A | 🔴 STUB |  |
| src\brain\learning\EmbeddingEngine.h | 30 | True | 🔴 STUB | EmbeddingEngine, OllamaEmbeddingEngine |
| src\brain\learning\KnowledgeDaemon.cpp | 578 | N/A | 🟢 REAL |  |
| src\brain\learning\KnowledgeDaemon.h | 158 | True | 🔴 STUB | CognitiveMemoryFabric, KnowledgeAnswer, InterestPr |
| src\brain\learning\LearningIngestor.cpp | 312 | N/A | 🔴 STUB |  |
| src\brain\learning\LearningIngestor.h | 110 | True | 🔴 STUB | LearnItem, ContradictionResult, CurriculumWeights, |
| src\brain\learning\MassCurriculumLoader.cpp | 157 | N/A | 🔴 STUB | BootItem |
| src\brain\learning\MassCurriculumLoader.h | 43 | True | 🔴 STUB | CognitiveMemoryFabric, CurriculumTopic, MassCurric |
| src\brain\memory\ActiveInferenceRetrieval.cpp | 87 | N/A | 🟡 PARTIAL |  |
| src\brain\memory\ActiveInferenceRetrieval.h | 39 | True | 🔴 STUB | ActiveInferenceRetrieval |
| src\brain\memory\ArchiveWriter.cpp | 155 | N/A | 🔴 STUB |  |
| src\brain\memory\ArchiveWriter.h | 62 | True | 🔴 STUB | ArchiveWriter |
| src\brain\memory\AuditSystem.cpp | 311 | N/A | 🔴 STUB |  |
| src\brain\memory\AuditSystem.h | 75 | True | 🔴 STUB | TraceStore, AuditFinding, SelfAuditReport, SelfAud |
| src\brain\memory\CognitiveMemoryFabric.cpp | 281 | N/A | 🟡 PARTIAL |  |
| src\brain\memory\CognitiveMemoryFabric.h | 111 | True | 🔴 STUB | EpisodicStore, MemoryEncoder, MemoryEncoder, Proce |
| src\brain\memory\ColumnarArchiveFormat.cpp | 378 | N/A | 🟡 PARTIAL |  |
| src\brain\memory\ColumnarArchiveFormat.h | 96 | True | 🔴 STUB | ColumnSchema, Type, ColumnData, ColumnarArchiveFor |
| src\brain\memory\ContextMemory.cpp | 174 | N/A | 🟡 PARTIAL |  |
| src\brain\memory\ContextMemory.h | 125 | True | 🔴 STUB | MemoryTurn, ConversationMemory, SystemHealthGrade, |
| src\brain\memory\DifferentialMemoryController.cpp | 272 | N/A | 🔴 STUB |  |
| src\brain\memory\DifferentialMemoryController.h | 109 | True | 🔴 STUB | DMCDecision, DecisionToken, TurnOutcome, Different |
| src\brain\memory\EpisodicStore.cpp | 739 | N/A | 🟡 PARTIAL |  |
| src\brain\memory\EpisodicStore.h | 151 | True | 🔴 STUB | VectorStore, EpisodeRecord, EpisodicStore, ChainVe |
| src\brain\memory\HdcSemanticGraph.cpp | 423 | N/A | 🟢 REAL |  |
| src\brain\memory\HdcSemanticGraph.h | 108 | True | 🔴 STUB | HdcConcept, HdcEdge, HdcSemanticGraph, ConceptStat |
| src\brain\memory\Hypervector.cpp | 141 | N/A | 🔴 STUB |  |
| src\brain\memory\Hypervector.h | 66 | True | 🔴 STUB | Hypervector |
| src\brain\memory\HypervectorEncoder.cpp | 111 | N/A | 🔴 STUB |  |
| src\brain\memory\HypervectorEncoder.h | 41 | True | 🔴 STUB | HypervectorEncoder |
| src\brain\memory\InformationGainEngine.cpp | 130 | N/A | 🔴 STUB |  |
| src\brain\memory\InformationGainEngine.h | 56 | True | 🔴 STUB | InformationGainEngine, Candidate |
| src\brain\memory\KnowledgeStore.cpp | 373 | N/A | 🟡 PARTIAL |  |
| src\brain\memory\KnowledgeStore.h | 75 | True | 🔴 STUB | IntentType, MiniIntent, StreamParseResult, StreamP |
| src\brain\memory\LocalitySensitiveHash.cpp | 116 | N/A | 🔴 STUB |  |
| src\brain\memory\LocalitySensitiveHash.h | 41 | True | 🔴 STUB | LocalitySensitiveHash, Bucket |
| src\brain\memory\MemoryDistiller.cpp | 227 | N/A | 🔴 STUB |  |
| src\brain\memory\MemoryDistiller.h | 63 | True | 🔴 STUB | CognitiveMemoryFabric, EpisodeRecord, VariationalS |
| src\brain\memory\MemoryEncoder.cpp | 107 | N/A | 🔴 STUB |  |
| src\brain\memory\MemoryEncoder.h | 35 | True | 🔴 STUB | MemoryEncoder |
| src\brain\memory\MerkleDAG.cpp | 187 | N/A | 🔴 STUB | Sha256Ctx |
| src\brain\memory\MerkleDAG.h | 48 | True | 🔴 STUB | MerkleDAG |
| src\brain\memory\ProceduralStore.cpp | 386 | N/A | 🟡 PARTIAL |  |
| src\brain\memory\ProceduralStore.h | 68 | True | 🔴 STUB | ProceduralStore, BlobType, Metadata |
| src\brain\memory\PromotionMetrics.cpp | 23 | N/A | 🔴 STUB |  |
| src\brain\memory\PromotionMetrics.h | 35 | True | 🔴 STUB | PromotionMetrics |
| src\brain\memory\SdmOptimizer.cpp | 27 | N/A | 🔴 STUB |  |
| src\brain\memory\SdmOptimizer.h | 26 | True | 🔴 STUB | SdmOptimizer |
| src\brain\memory\SemanticGraph.cpp | 355 | N/A | 🟢 REAL |  |
| src\brain\memory\SemanticGraph.h | 71 | True | 🔴 STUB | ConceptNode, ConceptEdge, SemanticGraph, ConceptWe |
| src\brain\memory\SparseDistributedMemory.cpp | 233 | N/A | 🟢 REAL |  |
| src\brain\memory\SparseDistributedMemory.h | 83 | True | 🔴 STUB | SparseDistributedMemory, Content, HardLocation |
| src\brain\memory\TinyMLP.cpp | 273 | N/A | 🔴 STUB |  |
| src\brain\memory\TinyMLP.h | 80 | True | 🔴 STUB | TinyMLP, LearningSample |
| src\brain\memory\UserMemory.cpp | 689 | N/A | 🟢 REAL |  |
| src\brain\memory\UserMemory.h | 123 | True | 🔴 STUB | PersonalFact, Relationship, TopicHistory, Emotiona |
| src\brain\predictive\error_functions.cpp | 86 | N/A | 🔴 STUB |  |
| src\brain\predictive\memory_store.cpp | 3 | N/A | 🔴 STUB |  |
| src\brain\predictive\memory_store.h | 66 | True | 🟡 PARTIAL | NullMemoryStore, InMemoryStore |
| src\brain\predictive\predictive_turn_engine.cpp | 1294 | N/A | 🟢 REAL |  |
| src\brain\predictive\predictive_turn_engine.h | 641 | True | 🔴 STUB | E1, KnowledgeDaemon, CognitiveMemoryFabric, Active |
| src\brain\predictive\response_shaper.cpp | 61 | N/A | 🔴 STUB |  |
| src\brain\predictive\salience_gate.cpp | 77 | N/A | 🔴 STUB |  |
| src\brain\predictive\sqlite_memory_store.cpp | 173 | N/A | 🟢 REAL |  |
| src\brain\predictive\sqlite_memory_store.h | 40 | True | 🔴 STUB | SqliteMemoryStore |
| src\brain\predictive\stream_workers.cpp | 686 | N/A | 🔴 STUB |  |
| src\brain\predictive\stream_workers.h | 67 | True | 🟡 PARTIAL | bodies, std, E1FastStream, E2SemanticStream, E3Dee |
| src\brain\predictive\tool_adapter.cpp | 34 | N/A | 🔴 STUB |  |
| src\brain\predictive\tool_adapter.h | 23 | True | 🔴 STUB | ToolAdapter |
| src\brain\predictive\turn_trace.h | 27 | True | 🔴 STUB | TurnTrace |
| src\brain\predictive\tests\test_predictive_turn_engine.cpp | 412 | N/A | 🟢 REAL | MockStream, CoordinatorTest, HangStream |
| src\brain\reasoning\EvidenceSystem.cpp | 275 | N/A | 🟡 PARTIAL |  |
| src\brain\reasoning\EvidenceSystem.h | 51 | True | 🔴 STUB | EvidenceGraphBuilder, Verifier |
| src\brain\reasoning\GoalModel.cpp | 160 | N/A | 🔴 STUB |  |
| src\brain\reasoning\GoalModel.h | 81 | True | 🔴 STUB | GoalModel, CogTaskState, Status, GoalModelBuilder |
| src\brain\reasoning\InputResolution.cpp | 486 | N/A | 🟢 REAL |  |
| src\brain\reasoning\InputResolution.h | 119 | True | 🔴 STUB | WebReconAgent, TaskDecomposer, ClarificationNeeded |
| src\brain\reasoning\PatternEngine.cpp | 560 | N/A | 🟢 REAL | Trigger, OMSignal, for, CK |
| src\brain\reasoning\PatternEngine.h | 86 | True | 🔴 STUB | ModeSignal, PatternEngine |
| src\brain\reasoning\SemanticParser.cpp | 490 | N/A | 🔴 STUB |  |
| src\brain\reasoning\SemanticParser.h | 113 | True | 🔴 STUB | SemanticSlot, IntentCategory, SemanticFrame, Seman |
| src\brain\reasoning\SynthesisEngine.cpp | 260 | N/A | 🟢 REAL |  |
| src\brain\reasoning\SynthesisEngine.h | 29 | True | 🔴 STUB | SynthesisEngine |
| src\brain\reasoning\TaskContext.cpp | 195 | N/A | 🔴 STUB |  |
| src\brain\reasoning\TaskContext.h | 35 | True | 🔴 STUB | TaskGenomeBuilder, SituationBuilder |
| src\brain\reasoning\TaskSystem.cpp | 511 | N/A | 🟢 REAL |  |
| src\brain\reasoning\TaskSystem.h | 111 | True | 🔴 STUB | PlanStatus, PlanStep, TaskPlan, TaskPlanner, Atomi |
| src\brain\retrieval\RetrievalSystem.cpp | 440 | N/A | 🟡 PARTIAL |  |
| src\brain\retrieval\RetrievalSystem.h | 90 | True | 🔴 STUB | WebSnippet, WebReconAgent, RetrievalRouter |
| src\brain\retrieval\VectorStore.cpp | 155 | N/A | 🟢 REAL |  |
| src\brain\retrieval\VectorStore.h | 44 | True | 🔴 STUB | HierarchicalNSW, InnerProductSpace, VectorSearchRe |
| src\brain\safety\CodeApprovalGate.cpp | 83 | N/A | 🔴 STUB |  |
| src\brain\safety\CodeApprovalGate.h | 37 | True | 🔴 STUB | ApprovalState, ApprovalRequest, CodeApprovalGate |
| src\brain\self\YukiSelfModel.cpp | 219 | N/A | 🔴 STUB |  |
| src\brain\self\YukiSelfModel.h | 62 | True | 🔴 STUB | Message, DomainExpertise, YukiSelfModel |
| src\brain\skills\SkillRegistry.cpp | 283 | N/A | 🔴 STUB |  |
| src\brain\skills\SkillRegistry.h | 61 | True | 🔴 STUB | SkillActionType, RuntimeSkill, SkillHit, SkillRegi |
| src\brain\skills\SkillSystem.cpp | 237 | N/A | 🟡 PARTIAL | CategoryRule, BlueprintTemplate, ScriptEntry |
| src\brain\skills\SkillSystem.h | 38 | True | 🔴 STUB | TaskCategory, SkillBlueprint, AutonomousSkillBuild |
| src\brain\sleep\SleepThread.cpp | 483 | N/A | 🟡 PARTIAL |  |
| src\brain\sleep\SleepThread.h | 154 | True | 🔴 STUB | EpisodicStore, HdcSemanticGraph, CognitiveMemoryFa |
| src\infrastructure\ControlPlane.cpp | 112 | N/A | 🔴 STUB |  |
| src\infrastructure\ControlPlane.h | 54 | True | 🔴 STUB | SystemState, ControlPlane |
| src\infrastructure\CoreBus.cpp | 57 | N/A | 🟡 PARTIAL |  |
| src\infrastructure\CoreBus.h | 62 | True | 🔴 STUB | Topic, Message, CoreBus |
| src\infrastructure\GlobalWorkspace.cpp | 66 | N/A | 🔴 STUB |  |
| src\infrastructure\GlobalWorkspace.h | 48 | True | 🔴 STUB | Coalition, GlobalWorkspace |
| src\infrastructure\ModuleRegistry.cpp | 71 | N/A | 🔴 STUB |  |
| src\infrastructure\ModuleRegistry.h | 52 | True | 🔴 STUB | ModuleHealth, ModuleInfo, ModuleRegistry |
| src\input\CameraRuntime.cpp | 366 | N/A | 🔴 STUB |  |
| src\input\CameraRuntime.h | 84 | True | 🔴 STUB | FaceDetection, CameraAnalysis, CameraFrameSnapshot |
| src\input\Ear.cpp | 324 | N/A | 🟢 REAL |  |
| src\input\Ear.h | 84 | True | 🔴 STUB | EarSnapshot, EarRuntime, EarReader |
| src\input\InputLayer.cpp | 100 | N/A | 🔴 STUB |  |
| src\input\InputLayer.h | 51 | True | 🔴 STUB | InputPerception, InputPerceptionBuilder, BodyState |
| src\input\Mouth.cpp | 1291 | N/A | 🟢 REAL | KokoroBackend, PiperBackend, SapiBackend, Candidat |
| src\input\Mouth.h | 203 | True | 🔴 STUB | SpeakPhase, VoiceBackendType, VoiceBackendInfo, Vo |
| src\input\PerceptionLayer.cpp | 315 | N/A | 🔴 STUB |  |
| src\input\PerceptionLayer.h | 172 | True | 🔴 STUB | PerceptionSourceType, InputSourceKind, PerceptionE |
| src\input\ScreenRuntime.cpp | 457 | N/A | 🔴 STUB |  |
| src\input\ScreenRuntime.h | 111 | True | 🔴 STUB | ScreenAnalysis, ScreenFrameSnapshot, ScreenRuntime |
| src\input\SpeechSystem.cpp | 398 | N/A | 🔴 STUB | whisper_context_params, UtteranceState |
| src\input\SpeechSystem.h | 107 | True | 🔴 STUB | whisper_context, WhisperModelStatus, WhisperEngine |
| src\input\VisionSystem.cpp | 135 | N/A | 🔴 STUB |  |
| src\input\VisionSystem.h | 67 | True | 🔴 STUB | CameraRuntime, ScreenRuntime, ScreenSnapshot, Scre |
| src\input\conditioning\ArtifactFilter.cpp | 200 | N/A | 🟡 PARTIAL |  |
| src\input\conditioning\ArtifactFilter.h | 69 | True | 🔴 STUB | ArtifactFilterConfig, ArtifactFilter, ChannelState |
| src\input\conditioning\ChangeDetector.cpp | 149 | N/A | 🔴 STUB |  |
| src\input\conditioning\ChangeDetector.h | 70 | True | 🔴 STUB | PrecisionState, ChangeDetectorMode, ChangeDetector |
| src\input\conditioning\ConditionedSnapshot.cpp | 102 | N/A | 🔴 STUB |  |
| src\input\conditioning\ConditionedSnapshot.h | 86 | True | 🔴 STUB | EarRuntime, CameraFrameSnapshot, ScreenFrameSnapsh |
| src\input\conditioning\SensorCalibrationProfile.cpp | 230 | N/A | 🟢 REAL |  |
| src\input\conditioning\SensorCalibrationProfile.h | 95 | True | 🔴 STUB | CalibrationCurve, SensorCalibrationProfile, Calibr |
| src\input\conditioning\SignalConditioningLayer.cpp | 525 | N/A | 🟢 REAL |  |
| src\input\conditioning\SignalConditioningLayer.h | 122 | True | 🔴 STUB | EarRuntime, CameraRuntime, ScreenRuntime, TurnCoor |
| src\input\conditioning\SignalNormalizer.cpp | 244 | N/A | 🔴 STUB |  |
| src\input\conditioning\SignalNormalizer.h | 54 | True | 🔴 STUB | SignalNormalizer |
| src\input\conditioning\TemporalAligner.cpp | 153 | N/A | 🔴 STUB |  |
| src\input\conditioning\TemporalAligner.h | 75 | True | 🔴 STUB | SynchronizedPerceptionFrame, TemporalAligner |
| src\input\encoding\AudioDSP.cpp | 415 | N/A | 🔴 STUB |  |
| src\input\encoding\AudioDSP.h | 116 | True | 🔴 STUB | AudioFeatures, AudioDSPEngine |
| src\input\encoding\MultiModalFusionGate.cpp | 159 | N/A | 🔴 STUB |  |
| src\input\encoding\MultiModalFusionGate.h | 45 | True | 🔴 STUB | FusionConfig, MultiModalFusionGate |
| src\input\encoding\ObservationEncoder.cpp | 222 | N/A | 🔴 STUB |  |
| src\input\encoding\ObservationEncoder.h | 74 | True | 🔴 STUB | IntentClass, ObservationEncoder, AudioEncoder, Cam |
| src\input\encoding\SensoryObservation.cpp | 131 | N/A | 🟡 PARTIAL |  |
| src\input\encoding\SensoryObservation.h | 66 | True | 🔴 STUB | ConditionedSnapshot, Modality, FeatureVector, Prec |
| src\input\encoding\SpatialAnchor.cpp | 30 | N/A | 🔴 STUB |  |
| src\input\encoding\SpatialAnchor.h | 41 | True | 🔴 STUB | EgocentricPose, SpatialUncertainty, SpatialAnchor |
| src\input\encoding\TemporalContext.h | 32 | True | 🔴 STUB | TurnPhase, RhythmPattern, TemporalContext |
| src\input\encoding\TextEncoder.cpp | 336 | N/A | 🔴 STUB |  |
| src\input\encoding\TextEncoder.h | 90 | True | 🔴 STUB | Word2VecConfig, TextEncoder, Heuristic, HeuristicS |
| src\input\encoding\VisualEncoder.cpp | 186 | N/A | 🔴 STUB |  |
| src\input\encoding\VisualEncoder.h | 40 | True | 🔴 STUB | ImageBuffer, VisualEncoder |
| src\scrapling\examples\example_fetch.cpp | 70 | N/A | 🟡 PARTIAL |  |
| src\scrapling\examples\example_static.cpp | 104 | N/A | 🟡 PARTIAL |  |
| src\scrapling\src\core\types.cpp | 56 | N/A | 🔴 STUB |  |
| src\scrapling\src\core\types.hpp | 68 | True | 🔴 STUB | Selector, Response, TextHandler, AttributesHandler |
| src\scrapling\src\engines\cdp_client.cpp | 882 | N/A | 🟢 REAL | WsConnection |
| src\scrapling\src\engines\cdp_client.hpp | 139 | True | 🔴 STUB | CdpClient, Config, BrowserInfo |
| src\scrapling\src\fetcher\http_fetcher.cpp | 359 | N/A | 🔴 STUB | CurlWriteBuffer |
| src\scrapling\src\fetcher\http_fetcher.hpp | 92 | True | 🔴 STUB | TlsProfile, FetcherConfig, HttpFetcher |
| src\scrapling\src\fetcher\response.cpp | 46 | N/A | 🔴 STUB |  |
| src\scrapling\src\fetcher\response.hpp | 50 | True | 🔴 STUB | Selector, Response |
| src\scrapling\src\parser\css_selector.cpp | 407 | N/A | 🔴 STUB |  |
| src\scrapling\src\parser\css_selector.hpp | 50 | True | 🔴 STUB | CssSelector, SimpleSelector, AttrTest, Pseudo, Sel |
| src\scrapling\src\parser\dom.cpp | 148 | N/A | 🟡 PARTIAL |  |
| src\scrapling\src\parser\dom.hpp | 65 | True | 🔴 STUB | DomNode, HtmlDocument |
| src\scrapling\src\parser\html_parser.cpp | 268 | N/A | 🟢 REAL |  |
| src\scrapling\src\parser\html_parser.hpp | 41 | True | 🔴 STUB | HtmlParser, Token |
| src\scrapling\src\parser\selector.cpp | 276 | N/A | 🟡 PARTIAL |  |
| src\scrapling\src\parser\selector.hpp | 95 | True | 🔴 STUB | Response, class, Selectors |
| src\scrapling\tests\test_cdp.cpp | 87 | N/A | 🔴 STUB |  |
| src\scrapling\tests\test_fetcher.cpp | 146 | N/A | 🔴 STUB |  |
| src\scrapling\tests\test_parser.cpp | 298 | N/A | 🔴 STUB | first, last |
| src\vendor\moodycamel\concurrentqueue.h | 58 | True | 🔴 STUB | ConcurrentQueue |
| src\vendor\sqlite\sqlite3.c | 255933 | N/A | 🟡 PARTIAL | the, sqlite3, this, sqlite3_file, sqlite3_file, sq |
| src\vendor\sqlite\sqlite3.h | 13375 | True | 🔴 STUB | the, sqlite3, this, sqlite3_file, sqlite3_file, sq |
| tests\test_air_retrieval.cpp | 181 | N/A | 🔴 STUB |  |
| tests\test_archive_writer.cpp | 161 | N/A | 🔴 STUB |  |
| tests\test_audio_dsp.cpp | 233 | N/A | 🔴 STUB |  |
| tests\test_dmc_procedural.cpp | 169 | N/A | 🔴 STUB |  |
| tests\test_executor_pack1.cpp | 137 | N/A | 🟡 PARTIAL |  |
| tests\test_hdc_graph.cpp | 169 | N/A | 🔴 STUB |  |
| tests\test_merkle_episodic_integration.cpp | 165 | N/A | 🔴 STUB |  |
| tests\test_promotion_hardening.cpp | 108 | N/A | 🔴 STUB |  |
| tests\test_scrapling_integration.cpp | 26 | N/A | 🟡 PARTIAL |  |
| tests\test_screen_crash.cpp | 23 | N/A | 🔴 STUB |  |
| tests\test_sdm_scale.cpp | 160 | N/A | 🔴 STUB |  |
| tests\test_sdm_stress.cpp | 213 | N/A | 🔴 STUB |  |
| tests\test_sleep_consolidation.cpp | 180 | N/A | 🔴 STUB |  |
| tests\test_t4_archive_cognitive.cpp | 662 | N/A | 🔴 STUB |  |
| tests\test_text_encoder.cpp | 147 | N/A | 🔴 STUB |  |
| tests\test_visual_encoder.cpp | 121 | N/A | 🔴 STUB |  |
| tests\test_yuki_full.cpp | 567 | N/A | 🔴 STUB | TestResult |

## STEP 3: TODO/FIXME CATALOG
| File | Line | Severity | Text |
|---|---|---|---|
| src\brain\CandidateGenerator.cpp | 81 | P3 | `float eScore = 0.5f; // Placeholder for edit score for now` |
| src\brain\DependencyInstaller.cpp | 20 | P3 | `// Stub: always assume not installed unless it's a known tool we mock` |
| src\brain\LanguageSynthesizer.h | 14 | P3 | `// Legacy pass-through stub — kept for any old call sites; returns input unchanged.` |
| src\brain\LanguageSynthesizer.h | 18 | P3 | `// generateResponse() and generateFactual() were dead inline stubs — removed.` |
| src\brain\ResponseActPlanner.cpp | 66 | P3 | `// Still-learning stubs are not strong` |
| src\brain\VerificationEngine.h | 11 | P3 | `// Note: OCREngine isn't strongly necessary here for the stub, but following prompt.` |
| src\brain\curiosity\CuriosityEngine.cpp | 11 | P3 | `// Placeholder: generate a curiosity-driven question every 10 turns` |
| src\brain\emotion\EmotionSystem.cpp | 296 | P3 | `// Multi-modal integration stub.` |
| src\brain\inference\PolicySelector.cpp | 181 | P2 | `// TODO: integrate with Yuki's logging system` |
| src\brain\inference\VariationalStateEstimator.cpp | 46 | P2 | `last_eval_count_ = 0; // TODO: wire from FreeEnergyCalculator if needed` |
| src\brain\memory\ArchiveWriter.cpp | 124 | P3 | `// which wasn't requested. Let's provide a basic stub or partial implementation.` |
| src\brain\memory\ArchiveWriter.cpp | 135 | P3 | `// Placeholder: full read path requires parsing all row groups.` |
| src\brain\memory\CognitiveMemoryFabric.cpp | 133 | P3 | `// Placeholder: retrieve most recent k episodes` |
| src\brain\memory\CognitiveMemoryFabric.cpp | 252 | P2 | `(void)subject;  // TODO: build subject.hv XOR relation.hv once getConcept is wired` |
| src\brain\memory\CognitiveMemoryFabric.cpp | 254 | P3 | `Hypervector query; // placeholder — querySimilar scores all concepts by default HV` |
| src\brain\memory\ColumnarArchiveFormat.cpp | 307 | P3 | `in.seekg(-8 - 8, std::ios::end);  // 8 for footer_offset placeholder, but we need mr_len first` |
| src\brain\predictive\predictive_turn_engine.cpp | 852 | P3 | `// This is NOT a hack — it uses the same features the VSE sees, just earlier` |
| src\brain\predictive\predictive_turn_engine.h | 36 | P3 | `// Vendor stub (same interface as moodycamel::ConcurrentQueue).` |
| src\brain\reasoning\SynthesisEngine.cpp | 98 | P3 | `// ── Append action plan stub ───────────────────────────────────────────────` |
| src\brain\retrieval\RetrievalSystem.cpp | 215 | P3 | `while ((p = url.find("&amp;", p)) != std::string::npos) { url.replace(p, 5, ""); p += 0; } // Hack for query param drop` |
| src\infrastructure\ControlPlane.cpp | 95 | P3 | `// Publish alert on rising edge (stub→throttle transition)` |
| src\infrastructure\ControlPlane.h | 36 | P3 | `// SecuritySandbox (stub — expands later)` |
| src\input\conditioning\SignalConditioningLayer.cpp | 486 | P2 | `// TODO: track per-sensor last_calibration_timestamp_ms_ and compute actual age` |
| src\vendor\sqlite\sqlite3.c | 14096 | P3 | `**     SQLITE_ZERO_MALLOC            // Use a stub allocator that always fails` |
| tests\test_t4_archive_cognitive.cpp | 530 | P3 | `// The full read path is a stub (returns false). These tests verify:` |
| tests\test_t4_archive_cognitive.cpp | 532 | P3 | `//   b. It returns false + empty vector on every call (stub contract)` |
| tests\test_t4_archive_cognitive.cpp | 533 | P3 | `//   c. Related stubs (listEpochChain, readArchiveByMerkle) behave consistently` |
| tests\test_t4_archive_cognitive.cpp | 582 | P3 | `// 4c. queryBySurprise with threshold=0.0 (catch all) still returns false (stub)` |
| tests\test_t4_archive_cognitive.cpp | 596 | P3 | `// 4d. listEpochChain stub: returns false, leaves vector empty` |
| tests\test_t4_archive_cognitive.cpp | 627 | P3 | `//     must find the node and not crash, even if full data read is stubbed` |

## STEP 4: MAIN.CPP INIT ORDER
Extracted initialization tokens from main.cpp:
- ('', 'Presence')
- ('', 'void')
- ('', 'void')
- ('', 'void')
- ('', 'int')
- ('', 'Core')
- ('', 'BabyMode')
- ('', 'PresenceShell')
- ('', '')
- ('', 'SignalConditioningLayer')
- ('', 'CoreBus')
- ('', 'else')
- ('', 'else')
- ('', 'else')
- ('', 'to')
- ('', 'thread')
- ('', 'up')
- ('', 'live')
- ('', 'be')
- ('', 'thread')

## STEP 5: CMAKE VERIFICATION
- Total CMake Sources identified: 0
- Total Tests identified: 17
Tests: test_predictive_turn_engine, test_executor_pack1, test_merkle_episodic_integration, test_sleep_consolidation, test_dmc_procedural, test_air_retrieval, test_promotion_hardening, test_archive_writer, test_t4_archive_cognitive, test_sdm_scale, test_yuki_full, test_sdm_stress, test_screen_crash, test_hdc_graph, test_audio_dsp, test_text_encoder, test_visual_encoder

## STEP 6: BLUEPRINT GAP ANALYSIS
| Blueprint Module | Yuki File(s) | Status | Gap |
|---|---|---|---|
| CoreBus | src\infrastructure\CoreBus.cpp, src\infrastructure\CoreBus.h | 🔴 STUB |  |
| GlobalWorkspace | src\infrastructure\GlobalWorkspace.cpp, src\infrastructure\GlobalWorkspace.h | 🔴 STUB |  |
| StatePlane | ??? | 🔴 MISSING | No module found |
| ControlPlane | src\infrastructure\ControlPlane.cpp, src\infrastructure\ControlPlane.h | 🔴 STUB |  |
| ActiveInferenceCore | ??? | 🔴 MISSING | No module found |
| VSE (24 states) | ??? | 🔴 MISSING | No module found |
| FreeEnergyCalculator | src\brain\inference\FreeEnergyCalculator.cpp, src\brain\inference\FreeEnergyCalculator.h | 🔴 STUB |  |
| PolicySelector | src\brain\inference\PolicySelector.cpp, src\brain\inference\PolicySelector.h | 🔴 STUB |  |
| PrecisionEngine | src\brain\inference\PrecisionEngine.cpp, src\brain\inference\PrecisionEngine.h | 🔴 STUB |  |
| GenerativeModel | src\brain\inference\GenerativeModel.cpp, src\brain\inference\GenerativeModel.h | 🔴 STUB |  |
| BeliefState | src\brain\inference\BeliefState.cpp, src\brain\inference\BeliefState.h | 🔴 STUB |  |
| EpisodicStore | src\brain\memory\EpisodicStore.cpp, src\brain\memory\EpisodicStore.h | 🔴 STUB |  |
| HNSW VectorStore | ??? | 🔴 MISSING | No module found |
| EpisodicRetriever | ??? | 🔴 MISSING | No module found |
| ConceptGraph | ??? | 🔴 MISSING | No module found |
| EntityRegistry | ??? | 🔴 MISSING | No module found |
| MemoryDistiller | src\brain\memory\MemoryDistiller.cpp, src\brain\memory\MemoryDistiller.h | 🔴 STUB |  |
| SDM+LSH (T1) | ??? | 🔴 MISSING | No module found |
| HDC Semantic Graph (T2) | ??? | 🔴 MISSING | No module found |
| DMC + Procedural (T3) | ??? | 🔴 MISSING | No module found |
| ArchiveWriter (T4) | ??? | 🔴 MISSING | No module found |
| PerceptionFusion | ??? | 🔴 MISSING | No module found |
| STTRuntime | ??? | 🔴 MISSING | No module found |
| ScreenRuntime | src\input\ScreenRuntime.cpp, src\input\ScreenRuntime.h | 🔴 STUB |  |
| CameraRuntime | src\input\CameraRuntime.cpp, src\input\CameraRuntime.h | 🔴 STUB |  |
| KeyboardRuntime | ??? | 🔴 MISSING | No module found |
| TypingRhythmAnalyzer | ??? | 🔴 MISSING | No module found |
| EmotionExtractor | ??? | 🔴 MISSING | No module found |
| SemanticParser | src\brain\reasoning\SemanticParser.cpp, src\brain\reasoning\SemanticParser.h | 🔴 STUB |  |
| HypothesisLattice | ??? | 🔴 MISSING | No module found |
| ReferenceResolutionEngine | ??? | 🔴 MISSING | No module found |
| GoalModel | src\brain\reasoning\GoalModel.cpp, src\brain\reasoning\GoalModel.h | 🔴 STUB |  |
| CausalReasoningEngine | ??? | 🔴 MISSING | No module found |
| AnalogyEngine | ??? | 🔴 MISSING | No module found |
| MetaCognitiveInterrupt | ??? | 🔴 MISSING | No module found |
| SensorimotorEngine | ??? | 🔴 MISSING | No module found |
| AutonomousPlanner | ??? | 🔴 MISSING | No module found |
| SystemExecutor | src\brain\SystemExecutor.cpp, src\brain\SystemExecutor.h | 🔴 STUB |  |
| AgentSpawner | ??? | 🔴 MISSING | No module found |
| ErrorRecoveryIntelligence | ??? | 🔴 MISSING | No module found |
| VerificationEngine | src\brain\VerificationEngine.cpp, src\brain\VerificationEngine.h | 🔴 STUB |  |
| KnowledgeDaemon | src\brain\learning\KnowledgeDaemon.cpp, src\brain\learning\KnowledgeDaemon.h | 🔴 STUB |  |
| WorldModelDaemon | ??? | 🔴 MISSING | No module found |
| DocReader | src\brain\DocReader.cpp, src\brain\DocReader.h | 🔴 STUB |  |
| WebReconAgent | ??? | 🔴 MISSING | No module found |
| FactVerifier | ??? | 🔴 MISSING | No module found |
| FreshnessFilter | ??? | 🔴 MISSING | No module found |
| LearningIngestor | src\brain\learning\LearningIngestor.cpp, src\brain\learning\LearningIngestor.h | 🔴 STUB |  |
| YukiSelfModel | src\brain\self\YukiSelfModel.cpp, src\brain\self\YukiSelfModel.h | 🔴 STUB |  |
| NarrativeEngine | ??? | 🔴 MISSING | No module found |
| PhiMonitor | ??? | 🔴 MISSING | No module found |
| SurpriseDetector | ??? | 🔴 MISSING | No module found |
| OutcomePropagator | ??? | 🔴 MISSING | No module found |
| TheoryOfMindEngine | ??? | 🔴 MISSING | No module found |
| PsychologicalProfileEngine | ??? | 🔴 MISSING | No module found |
| AdaptiveResponseShaper | ??? | 🔴 MISSING | No module found |
| ClarificationEngine | ??? | 🔴 MISSING | No module found |
| SynthesisEngine | src\brain\reasoning\SynthesisEngine.cpp, src\brain\reasoning\SynthesisEngine.h | 🔴 STUB |  |
| LanguageLayer | src\brain\LanguageLayer.cpp, src\brain\LanguageLayer.h | 🔴 STUB |  |
| EthicalConstraintEngine | ??? | 🔴 MISSING | No module found |
| HumanApprovalGate | ??? | 🔴 MISSING | No module found |
| SafetyGovernor | src\brain\SafetyGovernor.cpp, src\brain\SafetyGovernor.h | 🔴 STUB |  |
| PerformanceProfiler | ??? | 🔴 MISSING | No module found |
| BottleneckAnalyser | ??? | 🔴 MISSING | No module found |
| CodeReader | ??? | 🔴 MISSING | No module found |
| SelfRewriter | ??? | 🔴 MISSING | No module found |
| LogicVerifier | ??? | 🔴 MISSING | No module found |
| GitSandbox | ??? | 🔴 MISSING | No module found |
| RebuildManager | ??? | 🔴 MISSING | No module found |
| RegressionPreventor | ??? | 🔴 MISSING | No module found |
| SleepThread | src\brain\sleep\SleepThread.cpp, src\brain\sleep\SleepThread.h | 🔴 STUB |  |
| BackgroundLearningEngine | src\brain\learning\BackgroundLearningEngine.cpp, src\brain\learning\BackgroundLearningEngine.h | 🔴 STUB |  |
| MassCurriculumLoader | src\brain\learning\MassCurriculumLoader.cpp, src\brain\learning\MassCurriculumLoader.h | 🔴 STUB |  |
| TurnCoordinator | ??? | 🔴 MISSING | No module found |
| AIR (Active Inference Retrieval) | ??? | 🔴 MISSING | No module found |
| PredictiveTurnEngine | ??? | 🔴 MISSING | No module found |

## STEP 9: FINAL SUMMARY TABLES

### Table A: What's Real and Working
| Component | File | Lines | Status | Notes |
|---|---|---|---|---|
| main.cpp | src\main.cpp | 475 | 🟢 REAL | |
| EntityProcessor.cpp | src\brain\EntityProcessor.cpp | 264 | 🟢 REAL | |
| SmartScraper.cpp | src\brain\SmartScraper.cpp | 192 | 🟢 REAL | |
| KnowledgeDaemon.cpp | src\brain\learning\KnowledgeDaemon.cpp | 578 | 🟢 REAL | |
| HdcSemanticGraph.cpp | src\brain\memory\HdcSemanticGraph.cpp | 423 | 🟢 REAL | |
| SemanticGraph.cpp | src\brain\memory\SemanticGraph.cpp | 355 | 🟢 REAL | |
| SparseDistributedMemory.cpp | src\brain\memory\SparseDistributedMemory.cpp | 233 | 🟢 REAL | |
| UserMemory.cpp | src\brain\memory\UserMemory.cpp | 689 | 🟢 REAL | |
| predictive_turn_engine.cpp | src\brain\predictive\predictive_turn_engine.cpp | 1294 | 🟢 REAL | |
| sqlite_memory_store.cpp | src\brain\predictive\sqlite_memory_store.cpp | 173 | 🟢 REAL | |
| test_predictive_turn_engine.cpp | src\brain\predictive\tests\test_predictive_turn_engine.cpp | 412 | 🟢 REAL | |
| InputResolution.cpp | src\brain\reasoning\InputResolution.cpp | 486 | 🟢 REAL | |
| PatternEngine.cpp | src\brain\reasoning\PatternEngine.cpp | 560 | 🟢 REAL | |
| SynthesisEngine.cpp | src\brain\reasoning\SynthesisEngine.cpp | 260 | 🟢 REAL | |
| TaskSystem.cpp | src\brain\reasoning\TaskSystem.cpp | 511 | 🟢 REAL | |
| VectorStore.cpp | src\brain\retrieval\VectorStore.cpp | 155 | 🟢 REAL | |
| Ear.cpp | src\input\Ear.cpp | 324 | 🟢 REAL | |
| Mouth.cpp | src\input\Mouth.cpp | 1291 | 🟢 REAL | |
| SensorCalibrationProfile.cpp | src\input\conditioning\SensorCalibrationProfile.cpp | 230 | 🟢 REAL | |
| SignalConditioningLayer.cpp | src\input\conditioning\SignalConditioningLayer.cpp | 525 | 🟢 REAL | |
| cdp_client.cpp | src\scrapling\src\engines\cdp_client.cpp | 882 | 🟢 REAL | |
| html_parser.cpp | src\scrapling\src\parser\html_parser.cpp | 268 | 🟢 REAL | |

### Table B: What's Stubbed/Placeholder
| Component | File | Lines | Why Stub | Blocker |
|---|---|---|---|---|
| AutoSensor.cpp | src\AutoSensor.cpp | 115 | Needs implementation | |
| AutoSensor.h | src\AutoSensor.h | 11 | Needs implementation | |
| AvatarBody.cpp | src\AvatarBody.cpp | 380 | Needs implementation | |
| AvatarBody.h | src\AvatarBody.h | 49 | Needs implementation | |
| AvatarRenderer.cpp | src\AvatarRenderer.cpp | 447 | Needs implementation | |
| AvatarRenderer.h | src\AvatarRenderer.h | 32 | Needs implementation | |
| BabyMode.cpp | src\BabyMode.cpp | 664 | Needs implementation | |
| BabyMode.h | src\BabyMode.h | 187 | Needs implementation | |
| CommandRouter.cpp | src\CommandRouter.cpp | 307 | Needs implementation | |
| CommandRouter.h | src\CommandRouter.h | 44 | Needs implementation | |
| DetailView.cpp | src\DetailView.cpp | 377 | Needs implementation | |
| DetailView.h | src\DetailView.h | 35 | Needs implementation | |
| IntentScorer.cpp | src\IntentScorer.cpp | 253 | Needs implementation | |
| IntentScorer.h | src\IntentScorer.h | 75 | Needs implementation | |
| Logger.h | src\Logger.h | 7 | Needs implementation | |
| NeuralSpine.cpp | src\NeuralSpine.cpp | 149 | Needs implementation | |
| NeuralSpine.h | src\NeuralSpine.h | 93 | Needs implementation | |
| PresenceShell.cpp | src\PresenceShell.cpp | 1538 | Needs implementation | |
| PresenceShell.h | src\PresenceShell.h | 158 | Needs implementation | |
| ResponseEngine.cpp | src\ResponseEngine.cpp | 129 | Needs implementation | |
| ResponseEngine.h | src\ResponseEngine.h | 49 | Needs implementation | |
| RuntimeWorkerBase.h | src\RuntimeWorkerBase.h | 16 | Needs implementation | |
| SessionState.h | src\SessionState.h | 19 | Needs implementation | |
| SubsystemControl.cpp | src\SubsystemControl.cpp | 479 | Needs implementation | |
| SubsystemControl.h | src\SubsystemControl.h | 128 | Needs implementation | |
| YukiTestRunner.cpp | src\YukiTestRunner.cpp | 143 | Needs implementation | |
| YukiTestRunner.h | src\YukiTestRunner.h | 27 | Needs implementation | |
| YukiTestTypes.h | src\YukiTestTypes.h | 78 | Needs implementation | |
| YukiUtils.cpp | src\YukiUtils.cpp | 50 | Needs implementation | |
| YukiUtils.h | src\YukiUtils.h | 33 | Needs implementation | |
| ActionRouter.cpp | src\brain\ActionRouter.cpp | 87 | Needs implementation | |
| ActionRouter.h | src\brain\ActionRouter.h | 20 | Needs implementation | |
| BackgroundAgents.cpp | src\brain\BackgroundAgents.cpp | 252 | Needs implementation | |
| BackgroundAgents.h | src\brain\BackgroundAgents.h | 90 | Needs implementation | |
| BrainTypes.h | src\brain\BrainTypes.h | 297 | Needs implementation | |
| CandidateGenerator.cpp | src\brain\CandidateGenerator.cpp | 102 | Needs implementation | |
| CandidateGenerator.h | src\brain\CandidateGenerator.h | 21 | Needs implementation | |
| CapabilityMap.cpp | src\brain\CapabilityMap.cpp | 28 | Needs implementation | |
| CapabilityMap.h | src\brain\CapabilityMap.h | 16 | Needs implementation | |
| DependencyInstaller.cpp | src\brain\DependencyInstaller.cpp | 32 | Needs implementation | |
| DependencyInstaller.h | src\brain\DependencyInstaller.h | 14 | Needs implementation | |
| DocReader.cpp | src\brain\DocReader.cpp | 125 | Needs implementation | |
| DocReader.h | src\brain\DocReader.h | 9 | Needs implementation | |
| EntityProcessor.h | src\brain\EntityProcessor.h | 26 | Needs implementation | |
| ExecutionTypes.h | src\brain\ExecutionTypes.h | 111 | Needs implementation | |
| FileOperator.cpp | src\brain\FileOperator.cpp | 123 | Needs implementation | |
| FileOperator.h | src\brain\FileOperator.h | 27 | Needs implementation | |
| GoalBuilder.cpp | src\brain\GoalBuilder.cpp | 162 | Needs implementation | |
| GoalBuilder.h | src\brain\GoalBuilder.h | 8 | Needs implementation | |
| InputNormalizer.cpp | src\brain\InputNormalizer.cpp | 13 | Needs implementation | |
| InputNormalizer.h | src\brain\InputNormalizer.h | 10 | Needs implementation | |
| KnowledgeExtractor.cpp | src\brain\KnowledgeExtractor.cpp | 142 | Needs implementation | |
| KnowledgeExtractor.h | src\brain\KnowledgeExtractor.h | 31 | Needs implementation | |
| KnowledgeRecord.h | src\brain\KnowledgeRecord.h | 14 | Needs implementation | |
| KnowledgeRouter.cpp | src\brain\KnowledgeRouter.cpp | 140 | Needs implementation | |
| KnowledgeRouter.h | src\brain\KnowledgeRouter.h | 15 | Needs implementation | |
| LanguageLayer.cpp | src\brain\LanguageLayer.cpp | 169 | Needs implementation | |
| LanguageLayer.h | src\brain\LanguageLayer.h | 13 | Needs implementation | |
| LanguageSynthesizer.cpp | src\brain\LanguageSynthesizer.cpp | 70 | Needs implementation | |
| LanguageSynthesizer.h | src\brain\LanguageSynthesizer.h | 21 | Needs implementation | |
| LearningUpdate.cpp | src\brain\LearningUpdate.cpp | 109 | Needs implementation | |
| LearningUpdate.h | src\brain\LearningUpdate.h | 20 | Needs implementation | |
| LocalKnowledgeBase.cpp | src\brain\LocalKnowledgeBase.cpp | 108 | Needs implementation | |
| LocalKnowledgeBase.h | src\brain\LocalKnowledgeBase.h | 22 | Needs implementation | |
| MeaningTypes.h | src\brain\MeaningTypes.h | 165 | Needs implementation | |
| MobileServer.cpp | src\brain\MobileServer.cpp | 391 | Needs implementation | |
| MobileServer.h | src\brain\MobileServer.h | 69 | Needs implementation | |
| MotherCore.h | src\brain\MotherCore.h | 15 | Needs implementation | |
| Phase1Tests.cpp | src\brain\Phase1Tests.cpp | 599 | Needs implementation | |
| Phase1Tests.h | src\brain\Phase1Tests.h | 7 | Needs implementation | |
| RequestClassifier.cpp | src\brain\RequestClassifier.cpp | 188 | Needs implementation | |
| RequestClassifier.h | src\brain\RequestClassifier.h | 8 | Needs implementation | |
| ResponseActPlanner.cpp | src\brain\ResponseActPlanner.cpp | 157 | Needs implementation | |
| ResponseActPlanner.h | src\brain\ResponseActPlanner.h | 22 | Needs implementation | |
| SafetyGovernor.cpp | src\brain\SafetyGovernor.cpp | 34 | Needs implementation | |
| SafetyGovernor.h | src\brain\SafetyGovernor.h | 20 | Needs implementation | |
| ScriptRunner.cpp | src\brain\ScriptRunner.cpp | 88 | Needs implementation | |
| ScriptRunner.h | src\brain\ScriptRunner.h | 18 | Needs implementation | |
| SmartScraper.h | src\brain\SmartScraper.h | 45 | Needs implementation | |
| SystemExecutor.cpp | src\brain\SystemExecutor.cpp | 62 | Needs implementation | |
| SystemExecutor.h | src\brain\SystemExecutor.h | 18 | Needs implementation | |
| ToolExecutor.cpp | src\brain\ToolExecutor.cpp | 245 | Needs implementation | |
| ToolExecutor.h | src\brain\ToolExecutor.h | 49 | Needs implementation | |
| UIAutomationController.cpp | src\brain\UIAutomationController.cpp | 60 | Needs implementation | |
| UIAutomationController.h | src\brain\UIAutomationController.h | 19 | Needs implementation | |
| VerificationEngine.cpp | src\brain\VerificationEngine.cpp | 57 | Needs implementation | |
| VerificationEngine.h | src\brain\VerificationEngine.h | 13 | Needs implementation | |
| IntentClassifier.cpp | src\brain\core\IntentClassifier.cpp | 72 | Needs implementation | |
| IntentClassifier.h | src\brain\core\IntentClassifier.h | 34 | Needs implementation | |
| ResponseResolver.cpp | src\brain\core\ResponseResolver.cpp | 156 | Needs implementation | |
| ResponseResolver.h | src\brain\core\ResponseResolver.h | 39 | Needs implementation | |
| CuriosityEngine.cpp | src\brain\curiosity\CuriosityEngine.cpp | 33 | Needs implementation | |
| CuriosityEngine.h | src\brain\curiosity\CuriosityEngine.h | 26 | Needs implementation | |
| DatabaseManager.cpp | src\brain\database\DatabaseManager.cpp | 715 | Needs implementation | |
| DatabaseManager.h | src\brain\database\DatabaseManager.h | 80 | Needs implementation | |
| UniversalCache.cpp | src\brain\database\UniversalCache.cpp | 90 | Needs implementation | |
| UniversalCache.h | src\brain\database\UniversalCache.h | 42 | Needs implementation | |
| EmotionSystem.cpp | src\brain\emotion\EmotionSystem.cpp | 387 | Needs implementation | |
| EmotionSystem.h | src\brain\emotion\EmotionSystem.h | 67 | Needs implementation | |
| BeliefState.cpp | src\brain\inference\BeliefState.cpp | 100 | Needs implementation | |
| BeliefState.h | src\brain\inference\BeliefState.h | 40 | Needs implementation | |
| FreeEnergyCalculator.cpp | src\brain\inference\FreeEnergyCalculator.cpp | 204 | Needs implementation | |
| FreeEnergyCalculator.h | src\brain\inference\FreeEnergyCalculator.h | 58 | Needs implementation | |
| GenerativeModel.cpp | src\brain\inference\GenerativeModel.cpp | 250 | Needs implementation | |
| GenerativeModel.h | src\brain\inference\GenerativeModel.h | 58 | Needs implementation | |
| PolicySelector.cpp | src\brain\inference\PolicySelector.cpp | 188 | Needs implementation | |
| PolicySelector.h | src\brain\inference\PolicySelector.h | 40 | Needs implementation | |
| PrecisionEngine.cpp | src\brain\inference\PrecisionEngine.cpp | 58 | Needs implementation | |
| PrecisionEngine.h | src\brain\inference\PrecisionEngine.h | 36 | Needs implementation | |
| VariationalStateEstimator.cpp | src\brain\inference\VariationalStateEstimator.cpp | 72 | Needs implementation | |
| VariationalStateEstimator.h | src\brain\inference\VariationalStateEstimator.h | 51 | Needs implementation | |
| BackgroundLearningEngine.cpp | src\brain\learning\BackgroundLearningEngine.cpp | 164 | Needs implementation | |
| BackgroundLearningEngine.h | src\brain\learning\BackgroundLearningEngine.h | 75 | Needs implementation | |
| EmbeddingEngine.cpp | src\brain\learning\EmbeddingEngine.cpp | 147 | Needs implementation | |
| EmbeddingEngine.h | src\brain\learning\EmbeddingEngine.h | 30 | Needs implementation | |
| KnowledgeDaemon.h | src\brain\learning\KnowledgeDaemon.h | 158 | Needs implementation | |
| LearningIngestor.cpp | src\brain\learning\LearningIngestor.cpp | 312 | Needs implementation | |
| LearningIngestor.h | src\brain\learning\LearningIngestor.h | 110 | Needs implementation | |
| MassCurriculumLoader.cpp | src\brain\learning\MassCurriculumLoader.cpp | 157 | Needs implementation | |
| MassCurriculumLoader.h | src\brain\learning\MassCurriculumLoader.h | 43 | Needs implementation | |
| ActiveInferenceRetrieval.cpp | src\brain\memory\ActiveInferenceRetrieval.cpp | 87 | Needs implementation | |
| ActiveInferenceRetrieval.h | src\brain\memory\ActiveInferenceRetrieval.h | 39 | Needs implementation | |
| ArchiveWriter.cpp | src\brain\memory\ArchiveWriter.cpp | 155 | Needs implementation | |
| ArchiveWriter.h | src\brain\memory\ArchiveWriter.h | 62 | Needs implementation | |
| AuditSystem.cpp | src\brain\memory\AuditSystem.cpp | 311 | Needs implementation | |
| AuditSystem.h | src\brain\memory\AuditSystem.h | 75 | Needs implementation | |
| CognitiveMemoryFabric.cpp | src\brain\memory\CognitiveMemoryFabric.cpp | 281 | Needs implementation | |
| CognitiveMemoryFabric.h | src\brain\memory\CognitiveMemoryFabric.h | 111 | Needs implementation | |
| ColumnarArchiveFormat.cpp | src\brain\memory\ColumnarArchiveFormat.cpp | 378 | Needs implementation | |
| ColumnarArchiveFormat.h | src\brain\memory\ColumnarArchiveFormat.h | 96 | Needs implementation | |
| ContextMemory.cpp | src\brain\memory\ContextMemory.cpp | 174 | Needs implementation | |
| ContextMemory.h | src\brain\memory\ContextMemory.h | 125 | Needs implementation | |
| DifferentialMemoryController.cpp | src\brain\memory\DifferentialMemoryController.cpp | 272 | Needs implementation | |
| DifferentialMemoryController.h | src\brain\memory\DifferentialMemoryController.h | 109 | Needs implementation | |
| EpisodicStore.cpp | src\brain\memory\EpisodicStore.cpp | 739 | Needs implementation | |
| EpisodicStore.h | src\brain\memory\EpisodicStore.h | 151 | Needs implementation | |
| HdcSemanticGraph.h | src\brain\memory\HdcSemanticGraph.h | 108 | Needs implementation | |
| Hypervector.cpp | src\brain\memory\Hypervector.cpp | 141 | Needs implementation | |
| Hypervector.h | src\brain\memory\Hypervector.h | 66 | Needs implementation | |
| HypervectorEncoder.cpp | src\brain\memory\HypervectorEncoder.cpp | 111 | Needs implementation | |
| HypervectorEncoder.h | src\brain\memory\HypervectorEncoder.h | 41 | Needs implementation | |
| InformationGainEngine.cpp | src\brain\memory\InformationGainEngine.cpp | 130 | Needs implementation | |
| InformationGainEngine.h | src\brain\memory\InformationGainEngine.h | 56 | Needs implementation | |
| KnowledgeStore.cpp | src\brain\memory\KnowledgeStore.cpp | 373 | Needs implementation | |
| KnowledgeStore.h | src\brain\memory\KnowledgeStore.h | 75 | Needs implementation | |
| LocalitySensitiveHash.cpp | src\brain\memory\LocalitySensitiveHash.cpp | 116 | Needs implementation | |
| LocalitySensitiveHash.h | src\brain\memory\LocalitySensitiveHash.h | 41 | Needs implementation | |
| MemoryDistiller.cpp | src\brain\memory\MemoryDistiller.cpp | 227 | Needs implementation | |
| MemoryDistiller.h | src\brain\memory\MemoryDistiller.h | 63 | Needs implementation | |
| MemoryEncoder.cpp | src\brain\memory\MemoryEncoder.cpp | 107 | Needs implementation | |
| MemoryEncoder.h | src\brain\memory\MemoryEncoder.h | 35 | Needs implementation | |
| MerkleDAG.cpp | src\brain\memory\MerkleDAG.cpp | 187 | Needs implementation | |
| MerkleDAG.h | src\brain\memory\MerkleDAG.h | 48 | Needs implementation | |
| ProceduralStore.cpp | src\brain\memory\ProceduralStore.cpp | 386 | Needs implementation | |
| ProceduralStore.h | src\brain\memory\ProceduralStore.h | 68 | Needs implementation | |
| PromotionMetrics.cpp | src\brain\memory\PromotionMetrics.cpp | 23 | Needs implementation | |
| PromotionMetrics.h | src\brain\memory\PromotionMetrics.h | 35 | Needs implementation | |
| SdmOptimizer.cpp | src\brain\memory\SdmOptimizer.cpp | 27 | Needs implementation | |
| SdmOptimizer.h | src\brain\memory\SdmOptimizer.h | 26 | Needs implementation | |
| SemanticGraph.h | src\brain\memory\SemanticGraph.h | 71 | Needs implementation | |
| SparseDistributedMemory.h | src\brain\memory\SparseDistributedMemory.h | 83 | Needs implementation | |
| TinyMLP.cpp | src\brain\memory\TinyMLP.cpp | 273 | Needs implementation | |
| TinyMLP.h | src\brain\memory\TinyMLP.h | 80 | Needs implementation | |
| UserMemory.h | src\brain\memory\UserMemory.h | 123 | Needs implementation | |
| error_functions.cpp | src\brain\predictive\error_functions.cpp | 86 | Needs implementation | |
| memory_store.cpp | src\brain\predictive\memory_store.cpp | 3 | Needs implementation | |
| memory_store.h | src\brain\predictive\memory_store.h | 66 | Needs implementation | |
| predictive_turn_engine.h | src\brain\predictive\predictive_turn_engine.h | 641 | Needs implementation | |
| response_shaper.cpp | src\brain\predictive\response_shaper.cpp | 61 | Needs implementation | |
| salience_gate.cpp | src\brain\predictive\salience_gate.cpp | 77 | Needs implementation | |
| sqlite_memory_store.h | src\brain\predictive\sqlite_memory_store.h | 40 | Needs implementation | |
| stream_workers.cpp | src\brain\predictive\stream_workers.cpp | 686 | Needs implementation | |
| stream_workers.h | src\brain\predictive\stream_workers.h | 67 | Needs implementation | |
| tool_adapter.cpp | src\brain\predictive\tool_adapter.cpp | 34 | Needs implementation | |
| tool_adapter.h | src\brain\predictive\tool_adapter.h | 23 | Needs implementation | |
| turn_trace.h | src\brain\predictive\turn_trace.h | 27 | Needs implementation | |
| EvidenceSystem.cpp | src\brain\reasoning\EvidenceSystem.cpp | 275 | Needs implementation | |
| EvidenceSystem.h | src\brain\reasoning\EvidenceSystem.h | 51 | Needs implementation | |
| GoalModel.cpp | src\brain\reasoning\GoalModel.cpp | 160 | Needs implementation | |
| GoalModel.h | src\brain\reasoning\GoalModel.h | 81 | Needs implementation | |
| InputResolution.h | src\brain\reasoning\InputResolution.h | 119 | Needs implementation | |
| PatternEngine.h | src\brain\reasoning\PatternEngine.h | 86 | Needs implementation | |
| SemanticParser.cpp | src\brain\reasoning\SemanticParser.cpp | 490 | Needs implementation | |
| SemanticParser.h | src\brain\reasoning\SemanticParser.h | 113 | Needs implementation | |
| SynthesisEngine.h | src\brain\reasoning\SynthesisEngine.h | 29 | Needs implementation | |
| TaskContext.cpp | src\brain\reasoning\TaskContext.cpp | 195 | Needs implementation | |
| TaskContext.h | src\brain\reasoning\TaskContext.h | 35 | Needs implementation | |
| TaskSystem.h | src\brain\reasoning\TaskSystem.h | 111 | Needs implementation | |
| RetrievalSystem.cpp | src\brain\retrieval\RetrievalSystem.cpp | 440 | Needs implementation | |
| RetrievalSystem.h | src\brain\retrieval\RetrievalSystem.h | 90 | Needs implementation | |
| VectorStore.h | src\brain\retrieval\VectorStore.h | 44 | Needs implementation | |
| CodeApprovalGate.cpp | src\brain\safety\CodeApprovalGate.cpp | 83 | Needs implementation | |
| CodeApprovalGate.h | src\brain\safety\CodeApprovalGate.h | 37 | Needs implementation | |
| YukiSelfModel.cpp | src\brain\self\YukiSelfModel.cpp | 219 | Needs implementation | |
| YukiSelfModel.h | src\brain\self\YukiSelfModel.h | 62 | Needs implementation | |
| SkillRegistry.cpp | src\brain\skills\SkillRegistry.cpp | 283 | Needs implementation | |
| SkillRegistry.h | src\brain\skills\SkillRegistry.h | 61 | Needs implementation | |
| SkillSystem.cpp | src\brain\skills\SkillSystem.cpp | 237 | Needs implementation | |
| SkillSystem.h | src\brain\skills\SkillSystem.h | 38 | Needs implementation | |
| SleepThread.cpp | src\brain\sleep\SleepThread.cpp | 483 | Needs implementation | |
| SleepThread.h | src\brain\sleep\SleepThread.h | 154 | Needs implementation | |
| ControlPlane.cpp | src\infrastructure\ControlPlane.cpp | 112 | Needs implementation | |
| ControlPlane.h | src\infrastructure\ControlPlane.h | 54 | Needs implementation | |
| CoreBus.cpp | src\infrastructure\CoreBus.cpp | 57 | Needs implementation | |
| CoreBus.h | src\infrastructure\CoreBus.h | 62 | Needs implementation | |
| GlobalWorkspace.cpp | src\infrastructure\GlobalWorkspace.cpp | 66 | Needs implementation | |
| GlobalWorkspace.h | src\infrastructure\GlobalWorkspace.h | 48 | Needs implementation | |
| ModuleRegistry.cpp | src\infrastructure\ModuleRegistry.cpp | 71 | Needs implementation | |
| ModuleRegistry.h | src\infrastructure\ModuleRegistry.h | 52 | Needs implementation | |
| CameraRuntime.cpp | src\input\CameraRuntime.cpp | 366 | Needs implementation | |
| CameraRuntime.h | src\input\CameraRuntime.h | 84 | Needs implementation | |
| Ear.h | src\input\Ear.h | 84 | Needs implementation | |
| InputLayer.cpp | src\input\InputLayer.cpp | 100 | Needs implementation | |
| InputLayer.h | src\input\InputLayer.h | 51 | Needs implementation | |
| Mouth.h | src\input\Mouth.h | 203 | Needs implementation | |
| PerceptionLayer.cpp | src\input\PerceptionLayer.cpp | 315 | Needs implementation | |
| PerceptionLayer.h | src\input\PerceptionLayer.h | 172 | Needs implementation | |
| ScreenRuntime.cpp | src\input\ScreenRuntime.cpp | 457 | Needs implementation | |
| ScreenRuntime.h | src\input\ScreenRuntime.h | 111 | Needs implementation | |
| SpeechSystem.cpp | src\input\SpeechSystem.cpp | 398 | Needs implementation | |
| SpeechSystem.h | src\input\SpeechSystem.h | 107 | Needs implementation | |
| VisionSystem.cpp | src\input\VisionSystem.cpp | 135 | Needs implementation | |
| VisionSystem.h | src\input\VisionSystem.h | 67 | Needs implementation | |
| ArtifactFilter.cpp | src\input\conditioning\ArtifactFilter.cpp | 200 | Needs implementation | |
| ArtifactFilter.h | src\input\conditioning\ArtifactFilter.h | 69 | Needs implementation | |
| ChangeDetector.cpp | src\input\conditioning\ChangeDetector.cpp | 149 | Needs implementation | |
| ChangeDetector.h | src\input\conditioning\ChangeDetector.h | 70 | Needs implementation | |
| ConditionedSnapshot.cpp | src\input\conditioning\ConditionedSnapshot.cpp | 102 | Needs implementation | |
| ConditionedSnapshot.h | src\input\conditioning\ConditionedSnapshot.h | 86 | Needs implementation | |
| SensorCalibrationProfile.h | src\input\conditioning\SensorCalibrationProfile.h | 95 | Needs implementation | |
| SignalConditioningLayer.h | src\input\conditioning\SignalConditioningLayer.h | 122 | Needs implementation | |
| SignalNormalizer.cpp | src\input\conditioning\SignalNormalizer.cpp | 244 | Needs implementation | |
| SignalNormalizer.h | src\input\conditioning\SignalNormalizer.h | 54 | Needs implementation | |
| TemporalAligner.cpp | src\input\conditioning\TemporalAligner.cpp | 153 | Needs implementation | |
| TemporalAligner.h | src\input\conditioning\TemporalAligner.h | 75 | Needs implementation | |
| AudioDSP.cpp | src\input\encoding\AudioDSP.cpp | 415 | Needs implementation | |
| AudioDSP.h | src\input\encoding\AudioDSP.h | 116 | Needs implementation | |
| MultiModalFusionGate.cpp | src\input\encoding\MultiModalFusionGate.cpp | 159 | Needs implementation | |
| MultiModalFusionGate.h | src\input\encoding\MultiModalFusionGate.h | 45 | Needs implementation | |
| ObservationEncoder.cpp | src\input\encoding\ObservationEncoder.cpp | 222 | Needs implementation | |
| ObservationEncoder.h | src\input\encoding\ObservationEncoder.h | 74 | Needs implementation | |
| SensoryObservation.cpp | src\input\encoding\SensoryObservation.cpp | 131 | Needs implementation | |
| SensoryObservation.h | src\input\encoding\SensoryObservation.h | 66 | Needs implementation | |
| SpatialAnchor.cpp | src\input\encoding\SpatialAnchor.cpp | 30 | Needs implementation | |
| SpatialAnchor.h | src\input\encoding\SpatialAnchor.h | 41 | Needs implementation | |
| TemporalContext.h | src\input\encoding\TemporalContext.h | 32 | Needs implementation | |
| TextEncoder.cpp | src\input\encoding\TextEncoder.cpp | 336 | Needs implementation | |
| TextEncoder.h | src\input\encoding\TextEncoder.h | 90 | Needs implementation | |
| VisualEncoder.cpp | src\input\encoding\VisualEncoder.cpp | 186 | Needs implementation | |
| VisualEncoder.h | src\input\encoding\VisualEncoder.h | 40 | Needs implementation | |
| example_fetch.cpp | src\scrapling\examples\example_fetch.cpp | 70 | Needs implementation | |
| example_static.cpp | src\scrapling\examples\example_static.cpp | 104 | Needs implementation | |
| types.cpp | src\scrapling\src\core\types.cpp | 56 | Needs implementation | |
| types.hpp | src\scrapling\src\core\types.hpp | 68 | Needs implementation | |
| cdp_client.hpp | src\scrapling\src\engines\cdp_client.hpp | 139 | Needs implementation | |
| http_fetcher.cpp | src\scrapling\src\fetcher\http_fetcher.cpp | 359 | Needs implementation | |
| http_fetcher.hpp | src\scrapling\src\fetcher\http_fetcher.hpp | 92 | Needs implementation | |
| response.cpp | src\scrapling\src\fetcher\response.cpp | 46 | Needs implementation | |
| response.hpp | src\scrapling\src\fetcher\response.hpp | 50 | Needs implementation | |
| css_selector.cpp | src\scrapling\src\parser\css_selector.cpp | 407 | Needs implementation | |
| css_selector.hpp | src\scrapling\src\parser\css_selector.hpp | 50 | Needs implementation | |
| dom.cpp | src\scrapling\src\parser\dom.cpp | 148 | Needs implementation | |
| dom.hpp | src\scrapling\src\parser\dom.hpp | 65 | Needs implementation | |
| html_parser.hpp | src\scrapling\src\parser\html_parser.hpp | 41 | Needs implementation | |
| selector.cpp | src\scrapling\src\parser\selector.cpp | 276 | Needs implementation | |
| selector.hpp | src\scrapling\src\parser\selector.hpp | 95 | Needs implementation | |
| test_cdp.cpp | src\scrapling\tests\test_cdp.cpp | 87 | Needs implementation | |
| test_fetcher.cpp | src\scrapling\tests\test_fetcher.cpp | 146 | Needs implementation | |
| test_parser.cpp | src\scrapling\tests\test_parser.cpp | 298 | Needs implementation | |
| concurrentqueue.h | src\vendor\moodycamel\concurrentqueue.h | 58 | Needs implementation | |
| sqlite3.c | src\vendor\sqlite\sqlite3.c | 255933 | Needs implementation | |
| sqlite3.h | src\vendor\sqlite\sqlite3.h | 13375 | Needs implementation | |
| test_air_retrieval.cpp | tests\test_air_retrieval.cpp | 181 | Needs implementation | |
| test_archive_writer.cpp | tests\test_archive_writer.cpp | 161 | Needs implementation | |
| test_audio_dsp.cpp | tests\test_audio_dsp.cpp | 233 | Needs implementation | |
| test_dmc_procedural.cpp | tests\test_dmc_procedural.cpp | 169 | Needs implementation | |
| test_executor_pack1.cpp | tests\test_executor_pack1.cpp | 137 | Needs implementation | |
| test_hdc_graph.cpp | tests\test_hdc_graph.cpp | 169 | Needs implementation | |
| test_merkle_episodic_integration.cpp | tests\test_merkle_episodic_integration.cpp | 165 | Needs implementation | |
| test_promotion_hardening.cpp | tests\test_promotion_hardening.cpp | 108 | Needs implementation | |
| test_scrapling_integration.cpp | tests\test_scrapling_integration.cpp | 26 | Needs implementation | |
| test_screen_crash.cpp | tests\test_screen_crash.cpp | 23 | Needs implementation | |
| test_sdm_scale.cpp | tests\test_sdm_scale.cpp | 160 | Needs implementation | |
| test_sdm_stress.cpp | tests\test_sdm_stress.cpp | 213 | Needs implementation | |
| test_sleep_consolidation.cpp | tests\test_sleep_consolidation.cpp | 180 | Needs implementation | |
| test_t4_archive_cognitive.cpp | tests\test_t4_archive_cognitive.cpp | 662 | Needs implementation | |
| test_text_encoder.cpp | tests\test_text_encoder.cpp | 147 | Needs implementation | |
| test_visual_encoder.cpp | tests\test_visual_encoder.cpp | 121 | Needs implementation | |
| test_yuki_full.cpp | tests\test_yuki_full.cpp | 567 | Needs implementation | |

### Table C: What's Completely Missing
| Blueprint Module | Priority | Complexity | Blocking What? |
|---|---|---|---|
| StatePlane | High | High | System completeness |
| ActiveInferenceCore | High | High | System completeness |
| VSE (24 states) | High | High | System completeness |
| HNSW VectorStore | High | High | System completeness |
| EpisodicRetriever | High | High | System completeness |
| ConceptGraph | High | High | System completeness |
| EntityRegistry | High | High | System completeness |
| SDM+LSH (T1) | High | High | System completeness |
| HDC Semantic Graph (T2) | High | High | System completeness |
| DMC + Procedural (T3) | High | High | System completeness |
| ArchiveWriter (T4) | High | High | System completeness |
| PerceptionFusion | High | High | System completeness |
| STTRuntime | High | High | System completeness |
| KeyboardRuntime | High | High | System completeness |
| TypingRhythmAnalyzer | High | High | System completeness |
| EmotionExtractor | High | High | System completeness |
| HypothesisLattice | High | High | System completeness |
| ReferenceResolutionEngine | High | High | System completeness |
| CausalReasoningEngine | High | High | System completeness |
| AnalogyEngine | High | High | System completeness |
| MetaCognitiveInterrupt | High | High | System completeness |
| SensorimotorEngine | High | High | System completeness |
| AutonomousPlanner | High | High | System completeness |
| AgentSpawner | High | High | System completeness |
| ErrorRecoveryIntelligence | High | High | System completeness |
| WorldModelDaemon | High | High | System completeness |
| WebReconAgent | High | High | System completeness |
| FactVerifier | High | High | System completeness |
| FreshnessFilter | High | High | System completeness |
| NarrativeEngine | High | High | System completeness |
| PhiMonitor | High | High | System completeness |
| SurpriseDetector | High | High | System completeness |
| OutcomePropagator | High | High | System completeness |
| TheoryOfMindEngine | High | High | System completeness |
| PsychologicalProfileEngine | High | High | System completeness |
| AdaptiveResponseShaper | High | High | System completeness |
| ClarificationEngine | High | High | System completeness |
| EthicalConstraintEngine | High | High | System completeness |
| HumanApprovalGate | High | High | System completeness |
| PerformanceProfiler | High | High | System completeness |
| BottleneckAnalyser | High | High | System completeness |
| CodeReader | High | High | System completeness |
| SelfRewriter | High | High | System completeness |
| LogicVerifier | High | High | System completeness |
| GitSandbox | High | High | System completeness |
| RebuildManager | High | High | System completeness |
| RegressionPreventor | High | High | System completeness |
| TurnCoordinator | High | High | System completeness |
| AIR (Active Inference Retrieval) | High | High | System completeness |
| PredictiveTurnEngine | High | High | System completeness |

### Table D: Init Order Verification
| Order | Component | Instantiated? | Wired? | Nullptr Risk? |
|---|---|---|---|---|
| (Generated from main.cpp analysis) | | | | |

### Table E: Critical TODOs
| File | Line | TODO | Impact |
|---|---|---|---|

### Table F: Build Integrity
| Check | Pass/Fail | Notes |
|---|---|---|
| CMakeLists matches disk | Pending | Compare source lists |
| All headers have guards | Pending | Check STEP 1 |
| No undefined references | Pending | Need build log |
| Tests compile | Pending | Need build log |
