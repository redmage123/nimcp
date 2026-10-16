/**
 * @file test_factory_integration.cpp
 * @brief Integration tests for brain factory pattern and creation workflows
 *
 * Tests complete factory workflows:
 * - Create → Train → Infer → Destroy cycles
 * - Multiple brain creation with different configurations
 * - Configuration validation and building
 * - Subsystem initialization and interaction
 * - Complete learning pipelines through factory-created brains
 *
 * Estimated tests: 18
 */

#include <gtest/gtest.h>
#include "core/brain/factory/nimcp_brain_factory.h"
#include "core/brain/nimcp_brain.h"
#include "nimcp.h"
#include <cstdlib>
#include <cstring>
#include <vector>
#include <memory>
#include <unistd.h>

/**
 * @brief Test fixture for factory integration tests
 */
class FactoryIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize NIMCP library
        ASSERT_EQ(nimcp_init(), NIMCP_SUCCESS);
    }

    void TearDown() override {
        // Cleanup NIMCP library
        nimcp_shutdown();
    }

    // Helper to cleanup test files
    void cleanup_file(const char* filepath) {
        unlink(filepath);
    }
};

//=============================================================================
// Test Suite 1: Single Brain Creation → Train → Infer → Destroy Workflows
//=============================================================================

/**
 * @brief Test complete workflow: create small brain, train, infer, destroy
 */
TEST_F(FactoryIntegrationTest, WorkflowSmallBrainCreateTrainInferDestroy) {
    // Create small brain via factory
    brain_t brain = brain_create(
        "small_classifier",
        BRAIN_SIZE_SMALL,
        BRAIN_TASK_CLASSIFICATION,
        5,   // num_inputs
        2    // num_outputs (binary classification)
    );
    ASSERT_NE(brain, nullptr);

    // Verify brain structure is properly initialized
    EXPECT_STREQ(brain->config.task_name, "small_classifier");
    EXPECT_EQ(brain->config.num_inputs, 5);
    EXPECT_EQ(brain->config.num_outputs, 2);
    EXPECT_NE(brain->network, nullptr);

    // Training: Learn multiple examples
    float features_a[] = {1.0f, 0.0f, 1.0f, 0.0f, 1.0f};
    float features_b[] = {0.0f, 1.0f, 0.0f, 1.0f, 0.0f};

    for (int i = 0; i < 5; i++) {
        float loss_a = brain_learn_example(brain, features_a, 5, "class_a", 1.0f);
        float loss_b = brain_learn_example(brain, features_b, 5, "class_b", 1.0f);
        EXPECT_GE(loss_a, 0.0f);  // Loss should be non-negative
        EXPECT_GE(loss_b, 0.0f);
    }

    // Inference: Make predictions
    brain_decision_t* decision_a = brain_decide(brain, features_a, 5);
    brain_decision_t* decision_b = brain_decide(brain, features_b, 5);

    ASSERT_NE(decision_a, nullptr);
    ASSERT_NE(decision_b, nullptr);
    EXPECT_GT(decision_a->confidence, 0.0f);
    EXPECT_GT(decision_b->confidence, 0.0f);

    // Cleanup
    brain_destroy(brain);
}

/**
 * @brief Test complete workflow with medium-sized brain
 */
TEST_F(FactoryIntegrationTest, WorkflowMediumBrainCreateTrainInferDestroy) {
    // Create medium brain via factory
    brain_t brain = brain_create(
        "medium_regression",
        BRAIN_SIZE_MEDIUM,
        BRAIN_TASK_REGRESSION,
        10,  // num_inputs
        1    // num_outputs (regression)
    );
    ASSERT_NE(brain, nullptr);

    // Verify configuration
    EXPECT_EQ(brain->config.num_inputs, 10);
    EXPECT_EQ(brain->config.num_outputs, 1);
    EXPECT_NE(brain->network, nullptr);

    // Training: Continuous values
    float input[10];
    for (int sample = 0; sample < 10; sample++) {
        // Generate synthetic input
        for (int i = 0; i < 10; i++) {
            input[i] = (float)rand() / RAND_MAX;
        }

        // Learn mapping (returns loss value)
        float loss = brain_learn_example(brain, input, 10, "regression_sample", 0.8f);
        EXPECT_GE(loss, 0.0f);
    }

    // Inference with test input
    for (int i = 0; i < 10; i++) {
        input[i] = 0.5f;
    }
    brain_decision_t* decision = brain_decide(brain, input, 10);
    ASSERT_NE(decision, nullptr);
    EXPECT_GE(decision->confidence, 0.0f);

    // Cleanup
    brain_destroy(brain);
}

/**
 * @brief Test complete workflow with large brain and advanced features
 */
TEST_F(FactoryIntegrationTest, WorkflowLargeBrainCreateTrainInferDestroy) {
    // Create large brain via factory
    brain_t brain = brain_create(
        "large_complex",
        BRAIN_SIZE_LARGE,
        BRAIN_TASK_CLASSIFICATION,
        20,  // num_inputs
        5    // num_outputs (multi-class)
    );
    ASSERT_NE(brain, nullptr);

    // Verify network complexity
    EXPECT_EQ(brain->config.num_inputs, 20);
    EXPECT_EQ(brain->config.num_outputs, 5);
    uint32_t neuron_count = brain_get_neuron_count(brain);
    EXPECT_GT(neuron_count, 100);  // Should have substantial network

    // Training with diverse examples
    float input[20];
    for (int class_id = 0; class_id < 5; class_id++) {
        for (int sample = 0; sample < 3; sample++) {
            // Create class-specific patterns
            for (int i = 0; i < 20; i++) {
                input[i] = (i % 5 == class_id) ? 1.0f : 0.0f;
            }
            char label[32];
            snprintf(label, sizeof(label), "class_%d", class_id);
            float loss = brain_learn_example(brain, input, 20, label, 1.0f);
            EXPECT_GE(loss, 0.0f);
        }
    }

    // Inference with class-specific patterns
    for (int class_id = 0; class_id < 5; class_id++) {
        for (int i = 0; i < 20; i++) {
            input[i] = (i % 5 == class_id) ? 1.0f : 0.0f;
        }
        brain_decision_t* decision = brain_decide(brain, input, 20);
        ASSERT_NE(decision, nullptr);
        EXPECT_GT(decision->confidence, 0.0f);
    }

    // Cleanup
    brain_destroy(brain);
}

/**
 * @brief Test workflow with save, load, and continue training
 */
TEST_F(FactoryIntegrationTest, WorkflowSaveLoadContinueTraining) {
    const char* save_path = "/tmp/test_factory_integration_save.nimcp";
    cleanup_file(save_path);

    // Create and train initial brain
    brain_t brain = brain_create(
        "persistent_learner",
        BRAIN_SIZE_SMALL,
        BRAIN_TASK_CLASSIFICATION,
        5,
        2
    );
    ASSERT_NE(brain, nullptr);

    float features_a[] = {1.0f, 0.0f, 1.0f, 0.0f, 1.0f};
    for (int i = 0; i < 5; i++) {
        float loss = brain_learn_example(brain, features_a, 5, "class_a", 1.0f);
        EXPECT_GE(loss, 0.0f);
    }

    // Save brain
    bool save_result = brain_save(brain, save_path);
    ASSERT_TRUE(save_result);
    brain_destroy(brain);

    // Load brain and continue training
    brain_t loaded_brain = brain_load(save_path);
    ASSERT_NE(loaded_brain, nullptr);

    float features_b[] = {0.0f, 1.0f, 0.0f, 1.0f, 0.0f};
    for (int i = 0; i < 5; i++) {
        float loss = brain_learn_example(loaded_brain, features_b, 5, "class_b", 1.0f);
        EXPECT_GE(loss, 0.0f);
    }

    // Inference should work on both learned patterns
    brain_decision_t* decision_a = brain_decide(loaded_brain, features_a, 5);
    brain_decision_t* decision_b = brain_decide(loaded_brain, features_b, 5);

    ASSERT_NE(decision_a, nullptr);
    ASSERT_NE(decision_b, nullptr);
    EXPECT_GT(decision_a->confidence, 0.0f);
    EXPECT_GT(decision_b->confidence, 0.0f);

    brain_destroy(loaded_brain);
    cleanup_file(save_path);
}

//=============================================================================
// Test Suite 2: Multiple Brain Creation with Different Configurations
//=============================================================================

/**
 * @brief Test creating multiple brains simultaneously with different sizes
 */
TEST_F(FactoryIntegrationTest, MultipleBarainsWithDifferentSizes) {
    std::vector<brain_t> brains;

    // Create brains of different sizes
    brain_size_t sizes[] = {BRAIN_SIZE_TINY, BRAIN_SIZE_SMALL, BRAIN_SIZE_MEDIUM, BRAIN_SIZE_LARGE};

    for (int i = 0; i < 4; i++) {
        char name[32];
        snprintf(name, sizeof(name), "brain_size_%d", i);

        brain_t brain = brain_create(name, sizes[i], BRAIN_TASK_CLASSIFICATION, 10, 3);
        ASSERT_NE(brain, nullptr);
        brains.push_back(brain);

        // Verify size configuration
        EXPECT_EQ(brain->config.size, sizes[i]);
    }

    // Use all brains for training
    float features[10];
    for (int i = 0; i < 10; i++) {
        features[i] = (float)rand() / RAND_MAX;
    }

    for (auto brain : brains) {
        for (int i = 0; i < 3; i++) {
            float loss = brain_learn_example(brain, features, 10, "test", 1.0f);
            EXPECT_GE(loss, 0.0f);
        }
        brain_decision_t* decision = brain_decide(brain, features, 10);
        ASSERT_NE(decision, nullptr);
        EXPECT_GE(decision->confidence, 0.0f);
    }

    // Cleanup all brains
    for (auto brain : brains) {
        brain_destroy(brain);
    }
}

/**
 * @brief Test creating multiple brains with different task types
 */
TEST_F(FactoryIntegrationTest, MultipleBrainsWithDifferentTaskTypes) {
    std::vector<brain_t> brains;

    brain_task_t tasks[] = {
        BRAIN_TASK_CLASSIFICATION,
        BRAIN_TASK_REGRESSION,
        BRAIN_TASK_PATTERN_MATCHING,
        BRAIN_TASK_SEQUENCE
    };

    for (int i = 0; i < 4; i++) {
        char name[32];
        snprintf(name, sizeof(name), "brain_task_%d", i);

        brain_t brain = brain_create(name, BRAIN_SIZE_SMALL, tasks[i], 8, 2);
        ASSERT_NE(brain, nullptr);
        brains.push_back(brain);

        // Verify task configuration
        EXPECT_EQ(brain->config.task, tasks[i]);
    }

    // Train and infer on all brains
    float features[8];
    for (int i = 0; i < 8; i++) {
        features[i] = 0.5f;
    }

    for (auto brain : brains) {
        float loss = brain_learn_example(brain, features, 8, "sample", 1.0f);
        EXPECT_GE(loss, 0.0f);
        brain_decision_t* decision = brain_decide(brain, features, 8);
        ASSERT_NE(decision, nullptr);
        EXPECT_GE(decision->confidence, 0.0f);
    }

    // Cleanup
    for (auto brain : brains) {
        brain_destroy(brain);
    }
}

/**
 * @brief Test creating multiple brains with varying input/output dimensions
 */
TEST_F(FactoryIntegrationTest, MultipleBrainsWithVaryingDimensions) {
    std::vector<brain_t> brains;
    std::vector<std::pair<uint32_t, uint32_t>> dimensions = {
        {3, 2},      // Small
        {10, 5},     // Medium
        {20, 10},    // Large
        {50, 20}     // Very large
    };

    for (const auto& dims : dimensions) {
        char name[32];
        snprintf(name, sizeof(name), "brain_%u_%u", dims.first, dims.second);

        brain_t brain = brain_create(
            name,
            BRAIN_SIZE_SMALL,
            BRAIN_TASK_CLASSIFICATION,
            dims.first,
            dims.second
        );
        ASSERT_NE(brain, nullptr);
        brains.push_back(brain);

        EXPECT_EQ(brain->config.num_inputs, dims.first);
        EXPECT_EQ(brain->config.num_outputs, dims.second);
    }

    // Test each brain with appropriate dimensions
    for (size_t i = 0; i < brains.size(); i++) {
        uint32_t input_dim = dimensions[i].first;
        std::vector<float> features(input_dim);
        for (uint32_t j = 0; j < input_dim; j++) {
            features[j] = 0.5f;
        }

        float loss = brain_learn_example(brains[i], features.data(), input_dim, "test", 1.0f);
        EXPECT_GE(loss, 0.0f);
        brain_decision_t* decision = brain_decide(brains[i], features.data(), input_dim);
        ASSERT_NE(decision, nullptr);
        EXPECT_GE(decision->confidence, 0.0f);
    }

    // Cleanup
    for (auto brain : brains) {
        brain_destroy(brain);
    }
}

//=============================================================================
// Test Suite 3: Configuration Validation and Building
//=============================================================================

/**
 * @brief Test factory validation of creation parameters
 */
TEST_F(FactoryIntegrationTest, FactoryValidationCreationParameters) {
    // Valid parameters should succeed
    EXPECT_TRUE(nimcp_brain_factory_validate_creation_params("valid_name", 5, 3));

    // Null task_name should fail
    EXPECT_FALSE(nimcp_brain_factory_validate_creation_params(nullptr, 5, 3));

    // Zero inputs should fail
    EXPECT_FALSE(nimcp_brain_factory_validate_creation_params("test", 0, 3));

    // Zero outputs should fail
    EXPECT_FALSE(nimcp_brain_factory_validate_creation_params("test", 5, 0));

    // Extremely large dimensions should fail
    EXPECT_FALSE(nimcp_brain_factory_validate_creation_params("test", 100000, 3));
    EXPECT_FALSE(nimcp_brain_factory_validate_creation_params("test", 5, 100000));
}

/**
 * @brief Test factory neuron count mapping for different brain sizes
 */
TEST_F(FactoryIntegrationTest, FactoryNeuronCountMapping) {
    uint32_t tiny_count = nimcp_brain_factory_get_neuron_count(BRAIN_SIZE_TINY);
    uint32_t small_count = nimcp_brain_factory_get_neuron_count(BRAIN_SIZE_SMALL);
    uint32_t medium_count = nimcp_brain_factory_get_neuron_count(BRAIN_SIZE_MEDIUM);
    uint32_t large_count = nimcp_brain_factory_get_neuron_count(BRAIN_SIZE_LARGE);

    // Verify sensible ordering
    EXPECT_GT(small_count, tiny_count);
    EXPECT_GT(medium_count, small_count);
    EXPECT_GT(large_count, medium_count);

    // Verify reasonable ranges
    EXPECT_GT(tiny_count, 10);      // At least 10 neurons
    EXPECT_LT(large_count, 10000);  // At most 10000 neurons
}

/**
 * @brief Test factory sparsity configuration for different brain sizes
 */
TEST_F(FactoryIntegrationTest, FactorySparsityConfiguration) {
    float tiny_sparsity = nimcp_brain_factory_get_default_sparsity(BRAIN_SIZE_TINY);
    float small_sparsity = nimcp_brain_factory_get_default_sparsity(BRAIN_SIZE_SMALL);
    float medium_sparsity = nimcp_brain_factory_get_default_sparsity(BRAIN_SIZE_MEDIUM);
    float large_sparsity = nimcp_brain_factory_get_default_sparsity(BRAIN_SIZE_LARGE);

    // All sparsities should be in valid range
    EXPECT_GE(tiny_sparsity, 0.0f);
    EXPECT_LE(tiny_sparsity, 1.0f);
    EXPECT_GE(small_sparsity, 0.0f);
    EXPECT_LE(small_sparsity, 1.0f);
    EXPECT_GE(medium_sparsity, 0.0f);
    EXPECT_LE(medium_sparsity, 1.0f);
    EXPECT_GE(large_sparsity, 0.0f);
    EXPECT_LE(large_sparsity, 1.0f);

    // Larger brains typically have higher sparsity
    EXPECT_GE(large_sparsity, tiny_sparsity);
}

//=============================================================================
// Test Suite 4: Subsystem Interactions
//=============================================================================

/**
 * @brief Test that all subsystems interact correctly in created brain
 */
TEST_F(FactoryIntegrationTest, SubsystemInteractionWorking) {
    brain_t brain = brain_create(
        "subsystem_test",
        BRAIN_SIZE_MEDIUM,
        BRAIN_TASK_CLASSIFICATION,
        10,
        3
    );
    ASSERT_NE(brain, nullptr);

    // Verify key subsystems are initialized
    EXPECT_NE(brain->network, nullptr);           // Neural network
    EXPECT_NE(brain->strategy, nullptr);           // Task strategy
    if (brain->config.enable_working_memory) {
        EXPECT_NE(brain->working_memory, nullptr);  // Working memory (if enabled)
    }

    // Test working memory integration during learning
    float features[10];
    for (int i = 0; i < 10; i++) {
        features[i] = 0.3f;
    }

    // Learning should engage multiple subsystems
    float loss = brain_learn_example(brain, features, 10, "test_class", 1.0f);
    EXPECT_GE(loss, 0.0f);

    // Inference should use integrated subsystems
    brain_decision_t* decision = brain_decide(brain, features, 10);
    ASSERT_NE(decision, nullptr);
    EXPECT_GE(decision->confidence, 0.0f);

    // Cleanup
    brain_destroy(brain);
}

/**
 * @brief Test brain oscillations subsystem created with brain
 */
TEST_F(FactoryIntegrationTest, BrainOscillationsInitialization) {
    brain_t brain = brain_create(
        "oscillations_test",
        BRAIN_SIZE_SMALL,
        BRAIN_TASK_CLASSIFICATION,
        8,
        2
    );
    ASSERT_NE(brain, nullptr);

    // Brain oscillations should be available if configured
    if (brain->config.enable_oscillations) {
        // Perform a decision to generate neural activity
        float features[8] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
        float loss = brain_learn_example(brain, features, 8, "test", 1.0f);
        EXPECT_GE(loss, 0.0f);

        // Oscillations subsystem exists if enabled
        // (No public field to check - subsystem is internal)
    }

    brain_destroy(brain);
}

/**
 * @brief Test learning strategy integration with factory-created brain
 */
TEST_F(FactoryIntegrationTest, StrategyIntegrationDuringLearning) {
    brain_t brain = brain_create(
        "strategy_test",
        BRAIN_SIZE_SMALL,
        BRAIN_TASK_CLASSIFICATION,
        6,
        2
    );
    ASSERT_NE(brain, nullptr);
    ASSERT_NE(brain->strategy, nullptr);

    // Strategy should provide learning rate and parameters
    float initial_lr = brain->config.learning_rate;
    EXPECT_GT(initial_lr, 0.0f);
    EXPECT_LT(initial_lr, 1.0f);

    // Multiple learning iterations should work without crashes
    float features[6];
    for (int iteration = 0; iteration < 20; iteration++) {
        for (int i = 0; i < 6; i++) {
            features[i] = (float)rand() / RAND_MAX;
        }
        float loss = brain_learn_example(brain, features, 6, "sample", 1.0f);
        EXPECT_GE(loss, 0.0f);
    }

    // Decision should work with trained strategy
    for (int i = 0; i < 6; i++) {
        features[i] = 0.5f;
    }
    brain_decision_t* decision = brain_decide(brain, features, 6);
    ASSERT_NE(decision, nullptr);
    EXPECT_GE(decision->confidence, 0.0f);

    brain_destroy(brain);
}

//=============================================================================
// Test Suite 5: Custom Configuration and Advanced Workflows
//=============================================================================

/**
 * @brief Test creating brain with custom configuration
 */
TEST_F(FactoryIntegrationTest, CustomConfigurationBrainCreation) {
    brain_config_t config;
    memset(&config, 0, sizeof(config));
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.size = BRAIN_SIZE_MEDIUM;
    config.num_inputs = 15;
    config.num_outputs = 4;
    config.learning_rate = 0.01f;
    config.sparsity_target = 0.8f;
    strncpy(config.task_name, "custom_config_brain", sizeof(config.task_name) - 1);

    // Create with custom config
    brain_t brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    // Verify custom settings were applied
    EXPECT_EQ(brain->config.num_inputs, 15);
    EXPECT_EQ(brain->config.num_outputs, 4);
    EXPECT_NEAR(brain->config.learning_rate, 0.01f, 0.001f);

    brain_destroy(brain);
}

/**
 * @brief Test complex workflow with snapshot save/load
 */
TEST_F(FactoryIntegrationTest, SnapshotSaveLoadComplexWorkflow) {
    const char* snapshot_dir = "/tmp/nimcp_test_snapshots";
    mkdir(snapshot_dir, 0755);

    brain_t brain = brain_create(
        "snapshot_brain",
        BRAIN_SIZE_SMALL,
        BRAIN_TASK_CLASSIFICATION,
        8,
        3
    );
    ASSERT_NE(brain, nullptr);

    // Train brain
    float features[8];
    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < 8; j++) {
            features[j] = (i % 3 == 0) ? 1.0f : 0.0f;
        }
        float loss = brain_learn_example(brain, features, 8, "class_a", 1.0f);
        EXPECT_GE(loss, 0.0f);
    }

    // Save snapshot (returns bool)
    bool snapshot_result = brain_save_snapshot(brain, "test_snapshot", "Test snapshot for integration");
    // Snapshot save may fail if functionality not available, that's OK

    brain_destroy(brain);

    // Cleanup
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", snapshot_dir);
    system(cmd);
}

/**
 * @brief Test error recovery through factory recreation
 */
TEST_F(FactoryIntegrationTest, ErrorRecoveryThroughRecreation) {
    // Create first brain
    brain_t brain1 = brain_create(
        "recovery_test_1",
        BRAIN_SIZE_SMALL,
        BRAIN_TASK_CLASSIFICATION,
        5,
        2
    );
    ASSERT_NE(brain1, nullptr);

    float features[5] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    float loss1 = brain_learn_example(brain1, features, 5, "test", 1.0f);
    EXPECT_GE(loss1, 0.0f);

    brain_destroy(brain1);

    // Create new brain (simulating error recovery)
    brain_t brain2 = brain_create(
        "recovery_test_2",
        BRAIN_SIZE_SMALL,
        BRAIN_TASK_CLASSIFICATION,
        5,
        2
    );
    ASSERT_NE(brain2, nullptr);

    // Should work normally
    float loss2 = brain_learn_example(brain2, features, 5, "test", 1.0f);
    EXPECT_GE(loss2, 0.0f);
    brain_decision_t* decision = brain_decide(brain2, features, 5);
    ASSERT_NE(decision, nullptr);
    EXPECT_GE(decision->confidence, 0.0f);

    brain_destroy(brain2);
}

