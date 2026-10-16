/**
 * @file test_brain_advanced_coverage.cpp
 * @brief Advanced coverage tests for nimcp_brain.c to achieve 95%+ coverage
 *
 * WHAT: Comprehensive tests for uncovered functions in brain.c (1,207 lines)
 * WHY:  Increase coverage from 52.18% to 95%+ by testing all task strategies,
 *       batch operations, optimization, distributed features, pretrained models
 * HOW:  Unit, integration, and regression tests for all uncovered code paths
 *
 * TARGET AREAS:
 * - Task strategy functions (classification, regression, pattern, association, sequence, custom)
 * - Batch decision making (brain_decide_batch with various sizes)
 * - Model optimization (brain_optimize_for_inference, brain_prune, brain_recommend_pruning_threshold)
 * - Distributed brain functions (brain_create_distributed, brain_enable_distributed, brain_sync_neuromodulators)
 * - Pretrained models (brain_create_pretrained, brain_finetune, brain_download_model)
 * - Global workspace integration with cognitive modules
 * - Advanced configuration options and error paths
 *
 * @author NIMCP Coverage Team
 * @date 2025-11-11
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>
#include <sys/stat.h>

#include "core/brain/nimcp_brain.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/cache/nimcp_cache.h"
#include "utils/time/nimcp_time.h"
#include "nimcp.h"

//=============================================================================
// Test Fixture
//=============================================================================

class BrainAdvancedCoverageTest : public ::testing::Test {
protected:
    brain_t brain;

    void SetUp() override {
        brain = nullptr;
        // Initialize NIMCP systems
        nimcp_memory_init();
        nimcp_cache_init();
        nimcp_init();
        brain_clear_error();
    }

    void TearDown() override {
        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }
        // Cleanup NIMCP systems
        nimcp_shutdown();
        nimcp_cache_cleanup();
        nimcp_memory_cleanup();
    }

    // Helper: Create test features with specific base value
    float* create_features(uint32_t size, float base = 0.5f) {
        float* features = new float[size];
        for (uint32_t i = 0; i < size; i++) {
            features[i] = base + (float)i * 0.01f;
        }
        return features;
    }

    // Helper: Create random features
    float* create_random_features(uint32_t size) {
        float* features = new float[size];
        for (uint32_t i = 0; i < size; i++) {
            features[i] = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;
        }
        return features;
    }

    // Helper: Create trained brain for testing
    brain_t create_trained_brain(const char* name, brain_size_t size,
                                 brain_task_t task, uint32_t inputs, uint32_t outputs) {
        brain_t b = brain_create(name, size, task, inputs, outputs);
        if (!b) return nullptr;

        // Train with multiple examples
        for (int i = 0; i < 20; i++) {
            float* features = create_features(inputs, 0.1f * i);
            char label[32];
            snprintf(label, sizeof(label), "class_%d", i % outputs);
            brain_learn_example(b, features, inputs, label, 0.85f);
            delete[] features;
        }
        return b;
    }

    // Helper: Check file exists
    bool file_exists(const char* path) {
        struct stat buffer;
        return (stat(path, &buffer) == 0);
    }
};

//=============================================================================
// 1. Task-Specific Learning Tests (Classification Loss)
//=============================================================================

TEST_F(BrainAdvancedCoverageTest, Classification_LearningWithLabels) {
    // Create classification brain
    brain = brain_create("classification_test", BRAIN_SIZE_SMALL,
                        BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);

    // Prepare training data
    float features[10] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f};

    // Learn multiple examples to trigger loss calculation
    for (int i = 0; i < 10; i++) {
        float loss = brain_learn_example(brain, features, 10, "class_a", 1.0f);
        EXPECT_GE(loss, 0.0f) << "Loss should be non-negative";
    }

    // Verify brain learned something
    brain_decision_t* decision = brain_decide(brain, features, 10);
    ASSERT_NE(decision, nullptr);
    EXPECT_GE(decision->confidence, 0.0f);
    EXPECT_LE(decision->confidence, 1.0f);
    brain_free_decision(decision);
}

TEST_F(BrainAdvancedCoverageTest, Classification_MultipleLabels) {
    brain = brain_create("multi_class", BRAIN_SIZE_SMALL,
                        BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);

    float features1[10] = {1,2,3,4,5,6,7,8,9,10};
    float features2[10] = {10,9,8,7,6,5,4,3,2,1};
    float features3[10] = {5,5,5,5,5,5,5,5,5,5};

    // Learn different classes
    brain_learn_example(brain, features1, 10, "class_a", 1.0f);
    brain_learn_example(brain, features2, 10, "class_b", 1.0f);
    brain_learn_example(brain, features3, 10, "class_c", 1.0f);

    // Make predictions
    brain_decision_t* dec1 = brain_decide(brain, features1, 10);
    ASSERT_NE(dec1, nullptr);
    brain_free_decision(dec1);
}

//=============================================================================
// 2. Regression Task Tests (Regression Loss)
//=============================================================================

TEST_F(BrainAdvancedCoverageTest, Regression_BasicLearning) {
    brain = brain_create("regression_test", BRAIN_SIZE_SMALL,
                        BRAIN_TASK_REGRESSION, 10, 1);
    ASSERT_NE(brain, nullptr);

    float features[10] = {1,2,3,4,5,6,7,8,9,10};

    // Learn multiple examples to trigger MSE loss calculation
    for (int i = 0; i < 10; i++) {
        float loss = brain_learn_example(brain, features, 10, "regression_target", 1.0f);
        EXPECT_GE(loss, 0.0f);
    }

    brain_decision_t* decision = brain_decide(brain, features, 10);
    ASSERT_NE(decision, nullptr);
    brain_free_decision(decision);
}

//=============================================================================
// 3. Pattern Matching Task Tests (Pattern Loss)
//=============================================================================

TEST_F(BrainAdvancedCoverageTest, PatternMatching_BasicLearning) {
    brain = brain_create("pattern_test", BRAIN_SIZE_SMALL,
                        BRAIN_TASK_PATTERN_MATCHING, 10, 10);
    ASSERT_NE(brain, nullptr);

    float pattern[10] = {1,0,1,0,1,0,1,0,1,0};

    // Learn pattern multiple times to trigger binary cross-entropy loss
    for (int i = 0; i < 10; i++) {
        float loss = brain_learn_example(brain, pattern, 10, "binary_pattern", 1.0f);
        EXPECT_GE(loss, 0.0f);
    }

    brain_decision_t* decision = brain_decide(brain, pattern, 10);
    ASSERT_NE(decision, nullptr);
    brain_free_decision(decision);
}

//=============================================================================
// 4. Association Task Tests (Cosine Loss)
//=============================================================================

TEST_F(BrainAdvancedCoverageTest, Association_BasicLearning) {
    brain = brain_create("association_test", BRAIN_SIZE_SMALL,
                        BRAIN_TASK_ASSOCIATION, 10, 10);
    ASSERT_NE(brain, nullptr);

    float features[10] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f};

    // Learn associations to trigger cosine similarity loss
    for (int i = 0; i < 10; i++) {
        float loss = brain_learn_example(brain, features, 10, "association_target", 1.0f);
        EXPECT_GE(loss, 0.0f);
    }

    brain_decision_t* decision = brain_decide(brain, features, 10);
    ASSERT_NE(decision, nullptr);
    brain_free_decision(decision);
}

//=============================================================================
// 5. Symbolic Logic Integration Tests
//=============================================================================

TEST_F(BrainAdvancedCoverageTest, SymbolicLogic_EnabledConfiguration) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 10;
    config.num_outputs = 3;
    strncpy(config.task_name, "logic_test", sizeof(config.task_name) - 1);

    // Enable symbolic logic
    config.enable_logic = true;

    brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    // Verify brain was created successfully with symbolic logic
    brain_stats_t stats;
    bool result = brain_get_stats(brain, &stats);
    EXPECT_TRUE(result);
}

//=============================================================================
// 6. Global Workspace Configuration Tests
//=============================================================================

TEST_F(BrainAdvancedCoverageTest, GlobalWorkspace_CustomConfiguration) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 10;
    config.num_outputs = 3;
    strncpy(config.task_name, "workspace_test", sizeof(config.task_name) - 1);

    // Configure global workspace parameters
    config.enable_global_workspace = true;
    config.workspace_capacity_dim = 128;
    config.workspace_ignition_threshold = 0.7f;
    config.workspace_refractory_ms = 100;
    config.workspace_history_depth = 10;
    config.workspace_enable_history = true;

    brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    // Verify brain created with custom workspace config
    brain_stats_t stats;
    bool result = brain_get_stats(brain, &stats);
    EXPECT_TRUE(result);
}

TEST_F(BrainAdvancedCoverageTest, GlobalWorkspace_WithCognitiveModules) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 10;
    config.num_outputs = 3;
    strncpy(config.task_name, "modules_workspace", sizeof(config.task_name) - 1);

    // Enable global workspace
    config.enable_global_workspace = true;

    // Enable cognitive modules that subscribe to workspace
    config.enable_working_memory = true;
    config.enable_executive_control = true;
    config.enable_ethics = true;
    config.enable_introspection = true;
    config.enable_salience = true;
    config.enable_theory_of_mind = true;

    brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    // Perform operations to ensure modules are active
    float features[10] = {1,2,3,4,5,6,7,8,9,10};
    brain_learn_example(brain, features, 10, "test_label", 1.0f);

    brain_decision_t* decision = brain_decide(brain, features, 10);
    if (decision) {
        brain_free_decision(decision);
    }
}

//=============================================================================
// 7. Comprehensive Configuration Tests
//=============================================================================

TEST_F(BrainAdvancedCoverageTest, AllCognitiveFeatures_Enabled) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_MEDIUM;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 20;
    config.num_outputs = 5;
    strncpy(config.task_name, "full_features", sizeof(config.task_name) - 1);

    // Enable all cognitive features
    config.enable_working_memory = true;
    config.enable_executive_control = true;
    config.enable_ethics = true;
    config.enable_introspection = true;
    config.enable_salience = true;
    config.enable_theory_of_mind = true;
    config.enable_logic = true;
    config.enable_global_workspace = true;
    config.enable_mental_health_monitoring = true;
    config.enable_curiosity = true;
    config.enable_wellbeing = true;

    // Configure workspace
    config.workspace_capacity_dim = 256;
    config.workspace_ignition_threshold = 0.65f;
    config.workspace_refractory_ms = 50;
    config.workspace_history_depth = 15;
    config.workspace_enable_history = true;

    brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    // Exercise the brain with all features
    float features[20];
    for (int i = 0; i < 20; i++) {
        features[i] = static_cast<float>(i) / 20.0f;
    }

    // Learn
    float loss = brain_learn_example(brain, features, 20, "full_test", 1.0f);
    EXPECT_GE(loss, 0.0f);

    // Decide
    brain_decision_t* decision = brain_decide(brain, features, 20);
    ASSERT_NE(decision, nullptr);
    brain_free_decision(decision);

    // Get stats
    brain_stats_t stats;
    bool result = brain_get_stats(brain, &stats);
    EXPECT_TRUE(result);
}

//=============================================================================
// 8. Batch Learning Tests
//=============================================================================

TEST_F(BrainAdvancedCoverageTest, BatchLearning_MultipleExamples) {
    brain = brain_create("batch_test", BRAIN_SIZE_SMALL,
                        BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);

    // Create batch of examples
    std::vector<brain_example_t> examples;
    for (int i = 0; i < 5; i++) {
        brain_example_t ex = {};
        ex.features = new float[10];
        for (int j = 0; j < 10; j++) {
            ex.features[j] = static_cast<float>(i + j);
        }
        ex.num_features = 10;
        strncpy(ex.label, (i % 2 == 0) ? "class_a" : "class_b", sizeof(ex.label) - 1);
        ex.confidence = 1.0f;
        examples.push_back(ex);
    }

    // Learn batch
    float avg_loss = brain_learn_batch(brain, examples.data(), examples.size());
    EXPECT_GE(avg_loss, 0.0f);

    // Cleanup
    for (auto& ex : examples) {
        delete[] ex.features;
    }
}

//=============================================================================
// 9. Sequence Task Tests
//=============================================================================

TEST_F(BrainAdvancedCoverageTest, Sequence_BasicLearning) {
    brain = brain_create("sequence_test", BRAIN_SIZE_SMALL,
                        BRAIN_TASK_SEQUENCE, 10, 10);
    ASSERT_NE(brain, nullptr);

    float sequence[10] = {1,2,3,4,5,6,7,8,9,10};

    // Learn sequence patterns
    for (int i = 0; i < 10; i++) {
        float loss = brain_learn_example(brain, sequence, 10, "sequence_label", 1.0f);
        EXPECT_GE(loss, 0.0f);
    }

    brain_decision_t* decision = brain_decide(brain, sequence, 10);
    ASSERT_NE(decision, nullptr);
    brain_free_decision(decision);
}

//=============================================================================
// 10. Custom Task Tests
//=============================================================================

TEST_F(BrainAdvancedCoverageTest, CustomTask_BasicOperation) {
    brain = brain_create("custom_task", BRAIN_SIZE_SMALL,
                        BRAIN_TASK_CUSTOM, 10, 3);
    ASSERT_NE(brain, nullptr);

    float features[10] = {1,2,3,4,5,6,7,8,9,10};

    // Custom task should work with default behavior
    float loss = brain_learn_example(brain, features, 10, "custom_label", 1.0f);
    EXPECT_GE(loss, 0.0f);

    brain_decision_t* decision = brain_decide(brain, features, 10);
    ASSERT_NE(decision, nullptr);
    brain_free_decision(decision);
}

//=============================================================================
// 11. Error Path Tests for Configuration
//=============================================================================

TEST_F(BrainAdvancedCoverageTest, Configuration_InvalidWorkspaceSettings) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 10;
    config.num_outputs = 3;
    strncpy(config.task_name, "invalid_ws", sizeof(config.task_name) - 1);

    // Enable workspace with invalid settings
    config.enable_global_workspace = true;
    config.workspace_capacity_dim = 0;  // Invalid
    config.workspace_ignition_threshold = -1.0f;  // Invalid

    brain = brain_create_custom(&config);
    // Brain should handle invalid config gracefully
    // Either create with defaults or return nullptr
}

//=============================================================================
// 12. Learning Rate Variations
//=============================================================================

TEST_F(BrainAdvancedCoverageTest, Learning_VariousLearningRates) {
    brain = brain_create("lr_test", BRAIN_SIZE_SMALL,
                        BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);

    float features[10] = {1,2,3,4,5,6,7,8,9,10};

    // Test different learning rates
    float learning_rates[] = {0.001f, 0.01f, 0.1f, 0.5f};

    for (float lr : learning_rates) {
        float loss = brain_learn_example(brain, features, 10, "lr_test", lr);
        EXPECT_GE(loss, 0.0f);
    }

    brain_decision_t* decision = brain_decide(brain, features, 10);
    ASSERT_NE(decision, nullptr);
    brain_free_decision(decision);
}

//=============================================================================
// 13. Batch Decision Tests (brain_decide_batch)
//=============================================================================

TEST_F(BrainAdvancedCoverageTest, DecideBatch_SmallBatch) {
    brain = create_trained_brain("batch_small", BRAIN_SIZE_SMALL,
                                  BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);

    const int batch_size = 5;
    const float* inputs[batch_size];
    float* feature_arrays[batch_size];
    brain_decision_t decisions[batch_size];

    // Prepare batch
    for (int i = 0; i < batch_size; i++) {
        feature_arrays[i] = create_features(10, 0.2f * i);
        inputs[i] = feature_arrays[i];
    }

    // Batch inference
    bool success = brain_decide_batch(brain, inputs, batch_size, 10, decisions);
    EXPECT_TRUE(success);

    // Verify decisions
    for (int i = 0; i < batch_size; i++) {
        EXPECT_GE(decisions[i].confidence, 0.0f);
        EXPECT_LE(decisions[i].confidence, 1.0f);
        if (decisions[i].output_vector) {
            nimcp_free(decisions[i].output_vector);
        }
        if (decisions[i].active_neuron_ids) {
            nimcp_free(decisions[i].active_neuron_ids);
        }
    }

    // Cleanup
    for (int i = 0; i < batch_size; i++) {
        delete[] feature_arrays[i];
    }
}

TEST_F(BrainAdvancedCoverageTest, DecideBatch_LargeBatch) {
    brain = create_trained_brain("batch_large", BRAIN_SIZE_SMALL,
                                  BRAIN_TASK_CLASSIFICATION, 15, 5);
    ASSERT_NE(brain, nullptr);

    const int batch_size = 100;
    std::vector<const float*> inputs(batch_size);
    std::vector<float*> feature_arrays(batch_size);
    std::vector<brain_decision_t> decisions(batch_size);

    // Prepare large batch
    for (int i = 0; i < batch_size; i++) {
        feature_arrays[i] = create_features(15, 0.01f * i);
        inputs[i] = feature_arrays[i];
    }

    // Large batch inference
    bool success = brain_decide_batch(brain, inputs.data(), batch_size, 15, decisions.data());
    EXPECT_TRUE(success);

    // Cleanup
    for (int i = 0; i < batch_size; i++) {
        delete[] feature_arrays[i];
        if (decisions[i].output_vector) {
            nimcp_free(decisions[i].output_vector);
        }
        if (decisions[i].active_neuron_ids) {
            nimcp_free(decisions[i].active_neuron_ids);
        }
    }
}

TEST_F(BrainAdvancedCoverageTest, DecideBatch_ErrorHandling) {
    brain = create_trained_brain("batch_errors", BRAIN_SIZE_SMALL,
                                  BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);

    float* features = create_features(10);
    const float* inputs[] = {features};
    brain_decision_t decisions[1];

    // Test NULL brain
    bool result1 = brain_decide_batch(nullptr, inputs, 1, 10, decisions);
    EXPECT_FALSE(result1);
    brain_clear_error();

    // Test NULL inputs
    bool result2 = brain_decide_batch(brain, nullptr, 1, 10, decisions);
    EXPECT_FALSE(result2);
    brain_clear_error();

    // Test zero batch size
    bool result3 = brain_decide_batch(brain, inputs, 0, 10, decisions);
    EXPECT_FALSE(result3);
    brain_clear_error();

    // Test NULL decisions buffer
    bool result4 = brain_decide_batch(brain, inputs, 1, 10, nullptr);
    EXPECT_FALSE(result4);
    brain_clear_error();

    delete[] features;
}

//=============================================================================
// 14. Model Optimization Tests
//=============================================================================

TEST_F(BrainAdvancedCoverageTest, Prune_Basic) {
    brain = create_trained_brain("prune_test", BRAIN_SIZE_SMALL,
                                  BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);

    brain_stats_t stats_before;
    brain_get_stats(brain, &stats_before);

    // Prune weak connections
    uint32_t pruned = brain_prune(brain, 0.01f);

    brain_stats_t stats_after;
    brain_get_stats(brain, &stats_after);

    // Should have pruned some synapses
    EXPECT_LE(stats_after.num_active_synapses, stats_before.num_active_synapses);

    // Verify brain still works
    float* features = create_features(10);
    brain_decision_t* decision = brain_decide(brain, features, 10);
    ASSERT_NE(decision, nullptr);
    brain_free_decision(decision);
    delete[] features;
}

TEST_F(BrainAdvancedCoverageTest, Prune_ProgressivePruning) {
    brain = create_trained_brain("prune_progressive", BRAIN_SIZE_SMALL,
                                  BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);

    // Progressive pruning with increasing thresholds
    float thresholds[] = {0.001f, 0.005f, 0.01f, 0.02f};
    for (float threshold : thresholds) {
        brain_prune(brain, threshold);
    }

    // Verify brain still works
    float* features = create_features(10);
    brain_decision_t* decision = brain_decide(brain, features, 10);
    EXPECT_NE(decision, nullptr);
    if (decision) brain_free_decision(decision);
    delete[] features;
}

TEST_F(BrainAdvancedCoverageTest, OptimizeForInference_Complete) {
    brain = create_trained_brain("optimize_complete", BRAIN_SIZE_MEDIUM,
                                  BRAIN_TASK_CLASSIFICATION, 20, 5);
    ASSERT_NE(brain, nullptr);

    brain_stats_t stats_before;
    brain_get_stats(brain, &stats_before);

    // Optimize for inference
    bool result = brain_optimize_for_inference(brain);
    EXPECT_TRUE(result);

    brain_stats_t stats_after;
    brain_get_stats(brain, &stats_after);

    // Should have fewer active synapses
    EXPECT_LE(stats_after.num_active_synapses, stats_before.num_active_synapses);

    // Verify inference still works
    float* features = create_features(20);
    brain_decision_t* decision = brain_decide(brain, features, 20);
    ASSERT_NE(decision, nullptr);
    brain_free_decision(decision);
    delete[] features;
}

TEST_F(BrainAdvancedCoverageTest, RecommendPruningThreshold_VariousSparsity) {
    brain = create_trained_brain("recommend_threshold", BRAIN_SIZE_SMALL,
                                  BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);

    // Test various sparsity targets
    float sparsity_targets[] = {0.5f, 0.7f, 0.85f, 0.90f, 0.95f};
    for (float sparsity : sparsity_targets) {
        float threshold = brain_recommend_pruning_threshold(brain, sparsity);
        EXPECT_GE(threshold, 0.0f);
    }
}

TEST_F(BrainAdvancedCoverageTest, RecommendPruningThreshold_ApplyRecommendation) {
    brain = create_trained_brain("recommend_apply", BRAIN_SIZE_SMALL,
                                  BRAIN_TASK_CLASSIFICATION, 12, 4);
    ASSERT_NE(brain, nullptr);

    // Get recommendation
    float target_sparsity = 0.90f;
    float threshold = brain_recommend_pruning_threshold(brain, target_sparsity);
    EXPECT_GE(threshold, 0.0f);

    // Apply recommended threshold
    uint32_t pruned = brain_prune(brain, threshold);

    // Verify inference still works
    float* features = create_features(12);
    brain_decision_t* decision = brain_decide(brain, features, 12);
    EXPECT_NE(decision, nullptr);
    if (decision) brain_free_decision(decision);
    delete[] features;
}

TEST_F(BrainAdvancedCoverageTest, Optimization_ErrorHandling) {
    // Test NULL brain for prune
    uint32_t pruned = brain_prune(nullptr, 0.01f);
    EXPECT_EQ(pruned, 0u);
    brain_clear_error();

    // Test NULL brain for optimize
    bool result = brain_optimize_for_inference(nullptr);
    EXPECT_FALSE(result);
    brain_clear_error();

    // Test NULL brain for recommend
    float threshold = brain_recommend_pruning_threshold(nullptr, 0.9f);
    EXPECT_EQ(threshold, -1.0f);
    brain_clear_error();
}

//=============================================================================
// 15. Distributed Brain Tests
//=============================================================================

TEST_F(BrainAdvancedCoverageTest, Distributed_IsDistributedCheck) {
    brain = brain_create("standalone", BRAIN_SIZE_SMALL,
                        BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);

    bool is_distributed = brain_is_distributed(brain);
    EXPECT_FALSE(is_distributed);
}

TEST_F(BrainAdvancedCoverageTest, Distributed_CreateWithNullP2P) {
    brain = brain_create_distributed("distributed_null", BRAIN_SIZE_SMALL,
                                     BRAIN_TASK_CLASSIFICATION, 10, 3, nullptr);
    EXPECT_EQ(brain, nullptr);
    brain_clear_error();
}

TEST_F(BrainAdvancedCoverageTest, Distributed_EnableWithNullBrain) {
    bool result = brain_enable_distributed(nullptr, nullptr);
    EXPECT_FALSE(result);
    brain_clear_error();
}

TEST_F(BrainAdvancedCoverageTest, Distributed_EnableWithNullP2P) {
    brain = brain_create("enable_dist", BRAIN_SIZE_SMALL,
                        BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);

    bool result = brain_enable_distributed(brain, nullptr);
    EXPECT_FALSE(result);
    brain_clear_error();
}

TEST_F(BrainAdvancedCoverageTest, Distributed_SyncNeuromodulatorsNull) {
    bool result = brain_sync_neuromodulators(nullptr);
    EXPECT_FALSE(result);
    brain_clear_error();
}

TEST_F(BrainAdvancedCoverageTest, Distributed_SyncNonDistributed) {
    brain = brain_create("non_distributed", BRAIN_SIZE_SMALL,
                        BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);

    bool result = brain_sync_neuromodulators(brain);
    EXPECT_FALSE(result);
    brain_clear_error();
}

TEST_F(BrainAdvancedCoverageTest, Distributed_GetStatsNull) {
    distrib_cognition_stats_t stats;
    bool result = brain_get_distributed_stats(nullptr, &stats);
    EXPECT_FALSE(result);
    brain_clear_error();
}

TEST_F(BrainAdvancedCoverageTest, Distributed_GetStatsNullBuffer) {
    brain = brain_create("stats_test", BRAIN_SIZE_SMALL,
                        BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);

    bool result = brain_get_distributed_stats(brain, nullptr);
    EXPECT_FALSE(result);
    brain_clear_error();
}

//=============================================================================
// 16. Pretrained Model Tests
//=============================================================================

TEST_F(BrainAdvancedCoverageTest, PretrainedModel_ExistsCheck) {
    bool exists = brain_model_exists("nonexistent_model_xyz");
    EXPECT_FALSE(exists);
}

TEST_F(BrainAdvancedCoverageTest, PretrainedModel_ExistsNull) {
    bool exists = brain_model_exists(nullptr);
    EXPECT_FALSE(exists);
}

TEST_F(BrainAdvancedCoverageTest, PretrainedModel_DownloadInvalid) {
    bool result = brain_download_model("invalid_model_name");
    EXPECT_FALSE(result);
    brain_clear_error();
}

TEST_F(BrainAdvancedCoverageTest, PretrainedModel_DownloadNull) {
    bool result = brain_download_model(nullptr);
    EXPECT_FALSE(result);
    brain_clear_error();
}

TEST_F(BrainAdvancedCoverageTest, PretrainedModel_GetInfoInvalid) {
    brain_model_info_t info;
    bool result = brain_get_model_info("invalid_model", &info);
    EXPECT_FALSE(result);
    brain_clear_error();
}

TEST_F(BrainAdvancedCoverageTest, PretrainedModel_GetInfoNull) {
    brain_model_info_t info;
    bool result = brain_get_model_info(nullptr, &info);
    EXPECT_FALSE(result);
    brain_clear_error();
}

TEST_F(BrainAdvancedCoverageTest, PretrainedModel_GetInfoNullBuffer) {
    bool result = brain_get_model_info("some_model", nullptr);
    EXPECT_FALSE(result);
    brain_clear_error();
}

TEST_F(BrainAdvancedCoverageTest, PretrainedModel_CreateNull) {
    brain = brain_create_pretrained(nullptr, BRAIN_TASK_CLASSIFICATION);
    EXPECT_EQ(brain, nullptr);
    brain_clear_error();
}

TEST_F(BrainAdvancedCoverageTest, PretrainedModel_CreateInvalid) {
    brain = brain_create_pretrained("nonexistent_model", BRAIN_TASK_CLASSIFICATION);
    EXPECT_EQ(brain, nullptr);
    brain_clear_error();
}

TEST_F(BrainAdvancedCoverageTest, Finetune_NullBrain) {
    float training_data[30];
    float labels[9];
    bool result = brain_finetune(nullptr, training_data, labels, 3, nullptr);
    EXPECT_FALSE(result);
    brain_clear_error();
}

TEST_F(BrainAdvancedCoverageTest, Finetune_NullData) {
    brain = brain_create("finetune_test", BRAIN_SIZE_SMALL,
                        BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);

    float labels[9];
    bool result = brain_finetune(brain, nullptr, labels, 3, nullptr);
    EXPECT_FALSE(result);
    brain_clear_error();
}

TEST_F(BrainAdvancedCoverageTest, Finetune_NullLabels) {
    brain = brain_create("finetune_test", BRAIN_SIZE_SMALL,
                        BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);

    float training_data[30];
    bool result = brain_finetune(brain, training_data, nullptr, 3, nullptr);
    EXPECT_FALSE(result);
    brain_clear_error();
}

TEST_F(BrainAdvancedCoverageTest, Finetune_ZeroSamples) {
    brain = brain_create("finetune_test", BRAIN_SIZE_SMALL,
                        BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);

    float training_data[30];
    float labels[9];
    bool result = brain_finetune(brain, training_data, labels, 0, nullptr);
    EXPECT_FALSE(result);
    brain_clear_error();
}

TEST_F(BrainAdvancedCoverageTest, Finetune_WithCustomConfig) {
    brain = brain_create("finetune_config", BRAIN_SIZE_SMALL,
                        BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);

    const int num_samples = 10;
    const int input_dim = 10;
    const int output_dim = 3;
    std::vector<float> training_data(num_samples * input_dim);
    std::vector<float> labels(num_samples * output_dim);

    for (int i = 0; i < num_samples * input_dim; i++) {
        training_data[i] = ((float)rand() / RAND_MAX);
    }
    for (int i = 0; i < num_samples * output_dim; i++) {
        labels[i] = ((float)rand() / RAND_MAX);
    }

    brain_finetune_config_t config = {
        .learning_rate = 0.001f,
        .num_epochs = 3,
        .freeze_sensory = true,
        .freeze_cognitive = true,
        .finetune_classifier = true,
        .batch_size = 5,
        .verbose = false
    };

    brain_finetune(brain, training_data.data(), labels.data(), num_samples, &config);
    brain_clear_error();
}

//=============================================================================
// 17. Integration Tests
//=============================================================================

TEST_F(BrainAdvancedCoverageTest, Integration_ClassificationWithOptimization) {
    brain = brain_create("class_optim", BRAIN_SIZE_MEDIUM,
                        BRAIN_TASK_CLASSIFICATION, 30, 10);
    ASSERT_NE(brain, nullptr);

    // Train extensively
    for (int epoch = 0; epoch < 3; epoch++) {
        for (int cls = 0; cls < 10; cls++) {
            for (int sample = 0; sample < 5; sample++) {
                float* features = create_features(30, 0.1f * cls + 0.01f * sample);
                char label[32];
                snprintf(label, sizeof(label), "class_%d", cls);
                brain_learn_example(brain, features, 30, label, 0.9f);
                delete[] features;
            }
        }
    }

    // Optimize
    bool optimized = brain_optimize_for_inference(brain);
    EXPECT_TRUE(optimized);

    // Verify it still works
    float* test_features = create_features(30, 0.25f);
    brain_decision_t* decision = brain_decide(brain, test_features, 30);
    ASSERT_NE(decision, nullptr);
    brain_free_decision(decision);
    delete[] test_features;
}

TEST_F(BrainAdvancedCoverageTest, Integration_AllTaskTypesSequential) {
    brain_task_t tasks[] = {
        BRAIN_TASK_CLASSIFICATION,
        BRAIN_TASK_REGRESSION,
        BRAIN_TASK_PATTERN_MATCHING,
        BRAIN_TASK_SEQUENCE,
        BRAIN_TASK_ASSOCIATION,
        BRAIN_TASK_CUSTOM
    };

    for (int i = 0; i < 6; i++) {
        char name[32];
        snprintf(name, sizeof(name), "task_%d", i);

        brain_t b = brain_create(name, BRAIN_SIZE_SMALL, tasks[i], 10, 3);
        ASSERT_NE(b, nullptr);

        // Train
        for (int j = 0; j < 10; j++) {
            float* features = create_features(10, 0.1f * j);
            brain_learn_example(b, features, 10, "test", 0.85f);
            delete[] features;
        }

        // Inference
        float* test = create_features(10, 0.35f);
        brain_decision_t* decision = brain_decide(b, test, 10);
        EXPECT_NE(decision, nullptr);
        if (decision) brain_free_decision(decision);
        delete[] test;
        brain_destroy(b);
    }
}

//=============================================================================
// 18. Stress Tests
//=============================================================================

TEST_F(BrainAdvancedCoverageTest, Stress_LargeBatchInference) {
    brain = create_trained_brain("stress_batch", BRAIN_SIZE_SMALL,
                                  BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);

    const int batch_size = 200;
    std::vector<const float*> inputs(batch_size);
    std::vector<float*> feature_arrays(batch_size);
    std::vector<brain_decision_t> decisions(batch_size);

    for (int i = 0; i < batch_size; i++) {
        feature_arrays[i] = create_features(10, 0.005f * i);
        inputs[i] = feature_arrays[i];
    }

    bool success = brain_decide_batch(brain, inputs.data(), batch_size, 10, decisions.data());
    EXPECT_TRUE(success);

    // Cleanup
    for (int i = 0; i < batch_size; i++) {
        delete[] feature_arrays[i];
        if (decisions[i].output_vector) {
            nimcp_free(decisions[i].output_vector);
        }
        if (decisions[i].active_neuron_ids) {
            nimcp_free(decisions[i].active_neuron_ids);
        }
    }
}

TEST_F(BrainAdvancedCoverageTest, Stress_RepeatedOptimization) {
    brain = create_trained_brain("stress_optim", BRAIN_SIZE_SMALL,
                                  BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);

    // Optimize many times
    for (int i = 0; i < 5; i++) {
        bool result = brain_optimize_for_inference(brain);
        EXPECT_TRUE(result);
    }

    // Should still work
    float* features = create_features(10);
    brain_decision_t* decision = brain_decide(brain, features, 10);
    EXPECT_NE(decision, nullptr);
    if (decision) brain_free_decision(decision);
    delete[] features;
}

//=============================================================================
// Run Tests
//=============================================================================

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
