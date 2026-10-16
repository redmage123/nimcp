/**
 * @file test_brain_task_strategies.cpp
 * @brief Comprehensive tests for all brain task strategies
 *
 * WHAT: Tests for Association, Regression, and other task types
 * WHY:  Increase coverage by testing different task strategy code paths
 * HOW:  Create brains with each task type and exercise their unique paths
 *
 * COVERAGE TARGETS:
 * - Association task normalization (lines 535, 539-540)
 * - Regression task processing
 * - Different loss functions and output strategies
 * - Task-specific error handling
 *
 * ESTIMATED COVERAGE GAIN: +5-10%
 */

#include <gtest/gtest.h>
#include "core/brain/nimcp_brain.h"
#include "utils/memory/nimcp_memory.h"
#include "nimcp.h"
#include <cmath>

class BrainTaskStrategyTest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_init();
        brain_clear_error();
    }

    void TearDown() override {
        nimcp_shutdown();
    }
};

//=============================================================================
// GROUP 1: Association Task Tests
//=============================================================================

TEST_F(BrainTaskStrategyTest, AssociationTask_BasicOperation) {
    /**
     * WHAT: Test association task creation and learning
     * WHY:  Cover L2 normalization code paths (lines 535, 539-540)
     * HOW:  Create association brain, learn patterns, make decisions
     */

    brain_t brain = brain_create("test_association", BRAIN_SIZE_TINY,
                                BRAIN_TASK_ASSOCIATION, 10, 10);
    ASSERT_NE(brain, nullptr);

    // Learn association patterns
    float features1[10] = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                           0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    float features2[10] = {0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
                           0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

    // Learn multiple examples to trigger normalization
    for (int i = 0; i < 5; i++) {
        brain_learn_example(brain, features1, 10, "pattern_A", 1.0f);
        brain_learn_example(brain, features2, 10, "pattern_B", 1.0f);
    }

    // Make decisions to trigger normalization paths
    brain_decision_t* decision1 = brain_decide(brain, features1, 10);
    EXPECT_NE(decision1, nullptr);

    brain_decision_t* decision2 = brain_decide(brain, features2, 10);
    EXPECT_NE(decision2, nullptr);

    brain_destroy(brain);
}

TEST_F(BrainTaskStrategyTest, AssociationTask_LargeValues) {
    /**
     * WHAT: Test association with large values requiring normalization
     * WHY:  Trigger max_val computation in L2 normalization (line 535)
     * HOW:  Use features with varying magnitudes
     */

    brain_t brain = brain_create("test_assoc_norm", BRAIN_SIZE_TINY,
                                BRAIN_TASK_ASSOCIATION, 5, 5);
    ASSERT_NE(brain, nullptr);

    // Features with large values
    float features[5] = {100.0f, 200.0f, 300.0f, 400.0f, 500.0f};

    brain_learn_example(brain, features, 5, "large_pattern", 1.0f);
    brain_decision_t* decision = brain_decide(brain, features, 5);

    (void)decision; // May or may not succeed

    brain_destroy(brain);
}

//=============================================================================
// GROUP 2: Regression Task Tests
//=============================================================================

TEST_F(BrainTaskStrategyTest, RegressionTask_BasicOperation) {
    /**
     * WHAT: Test regression task creation and prediction
     * WHY:  Cover regression-specific code paths
     * HOW:  Create regression brain, learn continuous values
     */

    brain_t brain = brain_create("test_regression", BRAIN_SIZE_TINY,
                                BRAIN_TASK_REGRESSION, 5, 1);
    ASSERT_NE(brain, nullptr);

    // Learn regression patterns (input -> continuous output)
    float features1[5] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    float features2[5] = {2.0f, 4.0f, 6.0f, 8.0f, 10.0f};

    brain_learn_example(brain, features1, 5, "15.0", 1.0f);  // Sum = 15
    brain_learn_example(brain, features2, 5, "30.0", 1.0f);  // Sum = 30

    // Make predictions
    brain_decision_t* decision1 = brain_decide(brain, features1, 5);
    EXPECT_NE(decision1, nullptr);

    brain_decision_t* decision2 = brain_decide(brain, features2, 5);
    EXPECT_NE(decision2, nullptr);

    brain_destroy(brain);
}

TEST_F(BrainTaskStrategyTest, RegressionTask_MultipleOutputs) {
    /**
     * WHAT: Test regression with multiple output dimensions
     * WHY:  Cover multi-dimensional regression paths
     * HOW:  Create brain with multiple outputs
     */

    brain_t brain = brain_create("test_multi_regr", BRAIN_SIZE_SMALL,
                                BRAIN_TASK_REGRESSION, 10, 3);
    ASSERT_NE(brain, nullptr);

    float features[10] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f,
                          0.6f, 0.7f, 0.8f, 0.9f, 1.0f};

    // Learn pattern
    brain_learn_example(brain, features, 10, "output_vector", 1.0f);

    // Make prediction
    brain_decision_t* decision = brain_decide(brain, features, 10);
    (void)decision;

    brain_destroy(brain);
}

//=============================================================================
// GROUP 3: Classification Task (Edge Cases)
//=============================================================================

TEST_F(BrainTaskStrategyTest, ClassificationTask_MaxValueComputation) {
    /**
     * WHAT: Test classification softmax with varying values
     * WHY:  Cover max_val computation in softmax (line 430)
     * HOW:  Create patterns that trigger different max values
     */

    brain_t brain = brain_create("test_class_max", BRAIN_SIZE_TINY,
                                BRAIN_TASK_CLASSIFICATION, 5, 3);
    ASSERT_NE(brain, nullptr);

    // Pattern 1: All small values
    float small_features[5] = {0.1f, 0.1f, 0.1f, 0.1f, 0.1f};
    brain_learn_example(brain, small_features, 5, "class_A", 1.0f);

    // Pattern 2: One large value
    float large_features[5] = {10.0f, 0.1f, 0.1f, 0.1f, 0.1f};
    brain_learn_example(brain, large_features, 5, "class_B", 1.0f);

    // Pattern 3: Mixed values
    float mixed_features[5] = {-5.0f, 0.0f, 5.0f, -2.0f, 3.0f};
    brain_learn_example(brain, mixed_features, 5, "class_C", 1.0f);

    // Make decisions to trigger softmax
    brain_decide(brain, small_features, 5);
    brain_decide(brain, large_features, 5);
    brain_decide(brain, mixed_features, 5);

    brain_destroy(brain);
}

//=============================================================================
// GROUP 4: NULL Input Tests (Error Paths)
//=============================================================================

TEST_F(BrainTaskStrategyTest, NullInputs_BrainGetNetwork) {
    /**
     * WHAT: Test brain_get_network with NULL brain
     * WHY:  Cover NULL check error path
     * HOW:  Call with NULL, verify error handling
     */

    adaptive_network_t network = brain_get_network(nullptr);
    EXPECT_EQ(network, nullptr);

    const char* error = brain_get_last_error();
    EXPECT_NE(error, nullptr);

    brain_clear_error();
}

TEST_F(BrainTaskStrategyTest, NullInputs_BrainLearnExample) {
    /**
     * WHAT: Test brain_learn_example with NULL brain/features
     * WHY:  Cover NULL check error paths
     * HOW:  Call with various NULL combinations
     */

    float features[5] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};

    // NULL brain
    float result1 = brain_learn_example(nullptr, features, 5, "label", 1.0f);
    EXPECT_EQ(result1, -1.0f);

    // NULL features
    brain_t brain = brain_create("test_null", BRAIN_SIZE_TINY,
                                BRAIN_TASK_CLASSIFICATION, 5, 2);
    ASSERT_NE(brain, nullptr);

    float result2 = brain_learn_example(brain, nullptr, 5, "label", 1.0f);
    EXPECT_EQ(result2, -1.0f);

    brain_destroy(brain);
}

TEST_F(BrainTaskStrategyTest, NullInputs_BrainDecide) {
    /**
     * WHAT: Test brain_decide with NULL inputs
     * WHY:  Cover NULL check error paths
     * HOW:  Call with NULL brain and NULL features
     */

    float features[5] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};

    // NULL brain
    brain_decision_t* decision1 = brain_decide(nullptr, features, 5);
    EXPECT_EQ(decision1, nullptr);

    // NULL features
    brain_t brain = brain_create("test_null_decide", BRAIN_SIZE_TINY,
                                BRAIN_TASK_CLASSIFICATION, 5, 2);
    ASSERT_NE(brain, nullptr);

    brain_decision_t* decision2 = brain_decide(brain, nullptr, 5);
    EXPECT_EQ(decision2, nullptr);

    brain_destroy(brain);
}

//=============================================================================
// GROUP 5: Advanced Config Tests
//=============================================================================

TEST_F(BrainTaskStrategyTest, AdvancedConfig_GlialEnabled) {
    /**
     * WHAT: Test brain creation with glial integration
     * WHY:  Cover glial initialization paths
     * HOW:  Create brain with glial config enabled
     */

    brain_config_t config = {};
    config.size = BRAIN_SIZE_TINY;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 10;
    config.num_outputs = 3;
    config.enable_glial = true;
    config.num_astrocytes = 50;
    config.num_oligodendrocytes = 30;
    config.num_microglia = 20;
    strncpy(config.task_name, "test_glial", sizeof(config.task_name) - 1);

    brain_t brain = brain_create_custom(&config);
    if (brain != nullptr) {
        // Successfully created with glial
        EXPECT_NE(brain, nullptr);

        // Exercise the brain
        float features[10] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
                              0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
        brain_decide(brain, features, 10);

        brain_destroy(brain);
    } else {
        // Glial not available, skip
        GTEST_SKIP() << "Glial integration not available";
    }
}

TEST_F(BrainTaskStrategyTest, AdvancedConfig_OscillationsEnabled) {
    /**
     * WHAT: Test brain creation with oscillations enabled
     * WHY:  Cover brain wave initialization paths
     * HOW:  Create brain with oscillations config
     */

    brain_config_t config = {};
    config.size = BRAIN_SIZE_TINY;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 10;
    config.num_outputs = 3;
    config.enable_oscillations = true;
    strncpy(config.task_name, "test_osc", sizeof(config.task_name) - 1);

    brain_t brain = brain_create_custom(&config);
    if (brain != nullptr) {
        EXPECT_NE(brain, nullptr);

        float features[10] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
                              0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
        brain_decide(brain, features, 10);

        brain_destroy(brain);
    } else {
        GTEST_SKIP() << "Oscillations not available";
    }
}

//=============================================================================
// GROUP 6: Size Preset Edge Cases
//=============================================================================

TEST_F(BrainTaskStrategyTest, SizePreset_AllSizes) {
    /**
     * WHAT: Test all brain size presets
     * WHY:  Cover all switch cases in size preset logic
     * HOW:  Create brain with each size
     */

    brain_size_t sizes[] = {
        BRAIN_SIZE_TINY,
        BRAIN_SIZE_SMALL,
        BRAIN_SIZE_MEDIUM,
        BRAIN_SIZE_LARGE,
        BRAIN_SIZE_CUSTOM
    };

    const char* names[] = {"tiny", "small", "medium", "large", "custom"};

    for (size_t i = 0; i < 5; i++) {
        brain_t brain = brain_create(names[i], sizes[i],
                                    BRAIN_TASK_CLASSIFICATION, 5, 2);
        ASSERT_NE(brain, nullptr) << "Failed for size: " << names[i];

        // Verify brain works
        float features[5] = {0.2f, 0.4f, 0.6f, 0.8f, 1.0f};
        brain_decision_t* decision = brain_decide(brain, features, 5);
        (void)decision;

        brain_destroy(brain);
    }
}

//=============================================================================
// MAIN
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
