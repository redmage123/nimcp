# NIMCP Skipped Tests Analysis

This document catalogs all tests that are currently skipped or disabled in the NIMCP test suite, along with the reasons for skipping and potential remediation plans.

## Summary Statistics

- **Total Skipped Tests**: ~166 tests
- **Categories**:
  - DISABLED_ prefix tests: ~35
  - GTEST_SKIP() runtime skips: ~131

---

## Category 1: DISABLED_ Prefix Tests

These tests are explicitly disabled at compile time using GTest's `DISABLED_` prefix.

### Ethics B-Tree Regression Tests
**File**: `test/regression/btree/test_ethics_btree_regression.cpp`
**Tests**: 5 disabled tests
- `DISABLED_TimestampOrdering_TimeRangeQuery_CorrectOrder`
- `DISABLED_StressTest_ManyIncidents_NoCorruption`
- `DISABLED_RapidInsertions_UniqueTimestamps_NoCollisions`
- `DISABLED_PartialRetrieval_LimitedCount_CorrectSubset`
- `DISABLED_MixedViolationTypes_AllTypesStored_CorrectRetrieval`

**Reason**: Ethics B-tree implementation not yet complete
**Fix Priority**: Medium
**Remediation**: Complete ethics B-tree implementation

### Bio-Async Training E2E Tests
**File**: `test/e2e/e2e_test_bio_async_training_pipeline.cpp`
**Tests**: 3 disabled tests
- `DISABLED_BatchTrainingPipeline`
- `DISABLED_AsyncCheckpointPipeline`
- `DISABLED_TrainingWithPlasticityPipeline`

**Reason**: Training pipeline not fully integrated with bio-async system
**Fix Priority**: High
**Remediation**: Complete bio-async training integration

### Brain Working Memory Serialization Tests
**File**: `test/unit/utils/memory/test_brain_working_memory_serialization.cpp`
**Tests**: 8 disabled tests
- `DISABLED_SaveLoadWithSingleWorkingMemoryItem`
- `DISABLED_SaveLoadWithMultipleWorkingMemoryItems`
- `DISABLED_SaveLoadWithWorkingMemoryAndLongSequence`
- `DISABLED_SaveLoadWithEmotionallyTaggedWorkingMemory`
- `DISABLED_SaveLoadCycleStressTest`
- `DISABLED_SaveLoadWithMaxWorkingMemoryCapacity`
- `DISABLED_LoadCorruptedWorkingMemoryFile`
- `DISABLED_SaveLoadWorkingMemoryWithAllCognitiveSystems`

**Reason**: Working memory serialization API not yet stable
**Fix Priority**: High
**Remediation**: Stabilize working memory serialization API

### Utils Integration Tests
**File**: `test/integration/utils/test_utils_integration.cpp`
**Tests**: 1 disabled test
- `DISABLED_ThreadPoolQueueManager`

**Reason**: Thread pool queue manager integration incomplete
**Fix Priority**: Low
**Remediation**: Complete thread pool queue integration

### Learning Stability Tests
**File**: `test/regression/core/brain/learning/test_learning_stability.cpp`
**Tests**: 6 disabled tests
- `DISABLED_ConvergesOver20Iterations`
- `DISABLED_AssociationStrengthStable`
- `DISABLED_LearningRateAdaptationStable`
- `DISABLED_NoMemoryLeaksInExtendedLearning`
- `DISABLED_BatchLearningConsistent`
- `DISABLED_RewardLearningStable`

**Reason**: Learning stability tests require heavyweight brain setup
**Fix Priority**: Medium
**Remediation**: Optimize brain creation for test environment

### Code Immune Integration Tests
**File**: `test/integration/cognitive/immune/test_code_immune_integration.cpp`
**Tests**: 1 disabled test
- `DISABLED_GenerateCandidatesReturnsMultipleOptions`

**Reason**: Candidate generation API not stable
**Fix Priority**: Low
**Remediation**: Stabilize code immune candidate generation

### Pretrained Bio-Async Tests
**File**: `test/disabled/test_pretrained_bio_async.cpp`
**Tests**: 5 disabled tests
- `DISABLED_LoadingPublishesStateChange`
- `DISABLED_SuccessfulLoadPublishesDopamine`
- `DISABLED_FinetuningPublishesTrainingEvents`
- `DISABLED_LoadingGeneratesLogs`
- `DISABLED_GetModelInfoLogsActivity`

**Reason**: Pretrained model loading not yet implemented
**Fix Priority**: Medium
**Remediation**: Implement pretrained model loading

### Brain Persistence Tests
**File**: `test/unit/api/test_api_brain_persistence.cpp`
**Tests**: 1 disabled test
- `DISABLED_LoadedBrainIsIndependent`

**Reason**: Brain independence verification incomplete
**Fix Priority**: Medium
**Remediation**: Implement brain copy-on-write isolation verification

---

## Category 2: GTEST_SKIP() Runtime Skips

These tests skip at runtime based on conditions, typically feature availability.

### Feature Not Available Skips

#### FEP Bridge Tests
**Files**: `test/e2e/e2e_test_parietal_pipeline.cpp`
**Skip Reason**: "FEP bridge not available"
**Count**: ~7 tests
**Remediation**: Ensure FEP bridge initialization in test setup

#### Visual/Audio Cortex Tests
**Files**: `test/e2e/e2e_test_cnn_cortex_bridge.cpp`
**Skip Reasons**:
- "Visual cortex not available"
- "Audio cortex not available"
- "Audio cortex processing failed (FFT may not be available)"
**Count**: ~9 tests
**Remediation**: Add FFT dependency check, ensure cortex initialization

#### Encryption Tests
**Files**: `test/regression/io/test_encryption_regression.cpp`
**Skip Reason**: "Encryption not available"
**Count**: ~10 tests
**Remediation**: Ensure OpenSSL/crypto library available in test environment

#### Stream Tests
**Files**: `test/regression/io/test_stream_regression.cpp`
**Skip Reasons**:
- "Stream creation not implemented"
- "Stats not implemented"
- "Pause/resume not implemented"
- "Flush not implemented"
- "Clear not implemented"
**Count**: ~20 tests
**Remediation**: Complete stream implementation

#### DataIO Tests
**Files**: `test/regression/io/test_dataio_regression.cpp`
**Skip Reasons**:
- "CSV loading not implemented"
- "Batch reading not implemented"
- "Custom delimiter not implemented"
**Count**: ~12 tests
**Remediation**: Complete data I/O implementation

### Bridge/Router Not Initialized Skips

#### Cognitive Training Bridge Tests
**Files**:
- `test/e2e/e2e_test_cognitive_training_pipeline.cpp`
- `test/regression/middleware/training/test_cognitive_training_bridge_regression.cpp`
**Skip Reason**: "Bridge not yet implemented"
**Count**: ~50 tests
**Remediation**: Complete cognitive training bridge implementation

#### Thalamic Routing Tests
**Files**: `test/regression/cognitive/test_cognitive_thalamic_routing.cpp`
**Skip Reason**: "Bridge creation failed - may require real router"
**Count**: ~25 tests
**Remediation**: Fix thalamic routing bridge initialization

#### Middleware Controller Tests
**Files**: `test/regression/middleware/integration/test_middleware_controller_regression.cpp`
**Skip Reason**: "Setup failed"
**Count**: ~12 tests
**Remediation**: Fix middleware controller test setup

### Environment/Configuration Skips

#### Brain Test Environment Skips
**Files**: `test/regression/core/brain/learning/test_learning_stability.cpp`
**Skip Reason**: "Skipping brain-dependent test (NIMCP_SKIP_BRAIN_TESTS=1)"
**Remediation**: These are intentionally skippable via environment variable

#### Checkpoint Recovery Tests
**Files**: `test/integration/utils/fault_tolerance/test_checkpoint_recovery.cpp`
**Skip Reasons**:
- "Checkpoint load not yet fully implemented"
- "Auto-restore not yet fully implemented"
- "Partial recovery not yet implemented"
**Count**: ~5 tests
**Remediation**: Complete checkpoint/recovery implementation

#### Portia Integration Tests
**Files**: `test/integration/cognitive/executive/test_executive_portia_integration.cpp`
**Skip Reason**: "Portia not initialized, skipping integration test"
**Count**: ~10 tests
**Remediation**: Ensure Portia initialization in test setup

### Logic System Skips

#### Reasoning Regression Tests
**Files**: `test/regression/cognitive/reasoning/test_reasoning_regression.cpp`
**Skip Reason**: "Logic system not available"
**Count**: ~4 tests
**Remediation**: Ensure symbolic logic system initialization

### Bio-Async Module API Tests
**Files**: `test/regression/cognitive/bio_async/test_cognitive_modules_api.cpp`
**Skip Reasons**:
- "Requires complex brain setup"
- "Internally creates brain - has init issues in test environment"
- "predictive_default_config() has stack allocation issues"
**Count**: ~5 tests
**Remediation**: Fix brain initialization in test environment

### JSON Export Tests
**Files**: `test/regression/core/brain/test_brain_json_regression.cpp`
**Skip Reason**: "JSON export API not yet implemented in new brain API"
**Count**: ~6 tests
**Remediation**: Implement JSON export/import API

---

## Remediation Priority Matrix

| Priority | Category | Count | Effort | Impact |
|----------|----------|-------|--------|--------|
| High | Cognitive Training Bridge | ~50 | Large | High |
| High | Working Memory Serialization | 8 | Medium | High |
| Medium | Bio-Async Training | 3 | Medium | High |
| Medium | Stream Implementation | ~20 | Large | Medium |
| Medium | Thalamic Routing | ~25 | Medium | Medium |
| Medium | Learning Stability | 6 | Medium | Medium |
| Medium | JSON Export | 6 | Small | Medium |
| Low | Ethics B-Tree | 5 | Medium | Low |
| Low | DataIO | ~12 | Medium | Low |
| Low | Encryption | ~10 | Small | Low |

---

## Quick Fix Candidates

These tests can be fixed with minimal effort:

1. **FEP Bridge Tests** - Add proper initialization in test fixture
2. **Encryption Tests** - Add conditional compile based on OpenSSL availability
3. **JSON Export Tests** - Implement basic JSON export/import
4. **Portia Integration Tests** - Fix Portia initialization sequence

---

## Environment Variables

Some tests can be controlled via environment variables:

- `NIMCP_SKIP_BRAIN_TESTS=1` - Skip brain-dependent tests
- `NIMCP_TEST_TIMEOUT=<seconds>` - Override default test timeout

---

## Action Items

1. [ ] Create tracking issues for each high-priority category
2. [ ] Add CI job to track skip count over time
3. [ ] Set target: Reduce skipped tests by 50% in next quarter
4. [ ] Weekly review of newly skipped tests
