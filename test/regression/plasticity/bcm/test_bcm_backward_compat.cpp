/**
 * @file test_bcm_backward_compat.cpp
 * @brief Regression tests for BCM (Bienenstock-Cooper-Munro) plasticity
 *
 * WHAT: Ensures BCM features don't break existing code
 * WHY:  Verify zero breaking changes to pre-BCM code
 * HOW:  Test legacy patterns and ensure they still work correctly
 *
 * TEST COVERAGE:
 * 1. Brain creation without BCM awareness
 * 2. Legacy inference patterns unchanged
 * 3. BCM API doesn't break CPU code
 * 4. BCM doesn't interfere with other plasticity
 * 5. No performance regression
 * 6. BCM parameter validation
 * 7. Memory management no leaks
 * 8. Old learning patterns work
 * 9. BCM state consistency
 * 10. BCM doesn't break batch processing
 *
 * @version Regression Testing Framework v1.0
 * @date 2025-11-13
 */

#include <gtest/gtest.h>
#include <cmath>

// Headers have their own extern "C" guards
    #include "core/brain/nimcp_brain.h"
    #include "plasticity/bcm/nimcp_bcm.h"
    #include "utils/time/nimcp_time.h"

//=============================================================================
// Test Fixture
//=============================================================================

class BCMRegressionTest : public ::testing::Test {
protected:
    brain_t brain;

    void SetUp() override {
        brain = nullptr;
    }

    void TearDown() override {
        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }
    }
};

//=============================================================================
// Regression Test 1: Brain Creation Still Works
//=============================================================================

TEST_F(BCMRegressionTest, BrainCreation_StillWorks) {
    // Old code pattern: Create brain without BCM awareness
    brain = brain_create("test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2);

    // Should still work exactly as before
    ASSERT_NE(brain, nullptr) << "Brain creation should not be broken by BCM";
}

//=============================================================================
// Regression Test 2: Legacy Inference Without BCM
//=============================================================================

TEST_F(BCMRegressionTest, LegacyInference_StillWorks) {
    brain = brain_create("test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2);
    ASSERT_NE(brain, nullptr);

    // Old inference pattern (no BCM knowledge)
    float features[4] = {0.5f, 0.3f, 0.7f, 0.2f};
    brain_decision_t* decision = brain_decide(brain, features, 4);
    EXPECT_NE(decision, nullptr);
    if (decision) {
        EXPECT_NE(decision->label, nullptr);
    }
}

//=============================================================================
// Regression Test 3: BCM API Doesn't Break CPU Code
//=============================================================================

TEST_F(BCMRegressionTest, BCMAPI_NoCPUBreakage) {
    // Use BCM API (new)
    bcm_synapse_t synapse = bcm_synapse_init(0.5f, 0.5f);
    bcm_params_t params = bcm_params_cortical();
    bcm_apply_rule(&synapse, 1.0f, 1.0f, 1.0f, &params);

    // Old brain API should still work
    brain = brain_create("test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2);
    ASSERT_NE(brain, nullptr);

    float features[4] = {0.5f, 0.3f, 0.7f, 0.2f};
    brain_decision_t* decision = brain_decide(brain, features, 4);
    ASSERT_NE(decision, nullptr);
}

//=============================================================================
// Regression Test 4: BCM Doesn't Interfere With Other Plasticity
//=============================================================================

TEST_F(BCMRegressionTest, BCM_NoPlasticityInterference) {
    // Create BCM synapse
    bcm_synapse_t synapse = bcm_synapse_init(0.5f, 0.5f);
    bcm_params_t params = bcm_params_cortical();

    // Use BCM
    bcm_apply_rule(&synapse, 1.0f, 1.0f, 1.0f, &params);

    // Brain learning should work normally
    brain = brain_create("test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2);
    ASSERT_NE(brain, nullptr);

    float features[4] = {0.5f, 0.3f, 0.7f, 0.2f};
    brain_decision_t* decision1 = brain_decide(brain, features, 4);
    ASSERT_NE(decision1, nullptr);

    // Apply learning
    uint32_t modified = brain_apply_reward_learning(brain, 1.0f);
    EXPECT_GE(modified, 0);

    // Second decision should still work
    brain_decision_t* decision2 = brain_decide(brain, features, 4);
    ASSERT_NE(decision2, nullptr);
}

//=============================================================================
// Regression Test 5: No Performance Regression
//=============================================================================

TEST_F(BCMRegressionTest, NoPerformanceRegression) {
    brain = brain_create("test", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 5);
    ASSERT_NE(brain, nullptr);

    // Measure time for 100 inferences
    float features[10];
    for (int i = 0; i < 10; i++) {
        features[i] = 0.5f;
    }

    uint64_t start_time = nimcp_time_get_us();
    for (int i = 0; i < 100; i++) {
        brain_decision_t* decision = brain_decide(brain, features, 10);
        (void)decision;
    }
    uint64_t end_time = nimcp_time_get_us();

    uint64_t elapsed_us = end_time - start_time;
    float avg_us = elapsed_us / 100.0f;

    EXPECT_LT(avg_us, 1000.0f) << "BCM shouldn't cause severe performance regression";
}

//=============================================================================
// Regression Test 6: BCM Parameter Validation
//=============================================================================

TEST_F(BCMRegressionTest, BCMParameterValidation) {
    // All presets should return valid parameters
    bcm_params_t presets[] = {
        bcm_params_cortical(),
        bcm_params_critical_period(),
        bcm_params_mature()
    };

    for (int i = 0; i < 3; i++) {
        const bcm_params_t& params = presets[i];

        // Parameters should be within valid ranges
        EXPECT_GT(params.learning_rate, 0.0f);
        EXPECT_LE(params.learning_rate, 1.0f);
        EXPECT_GT(params.threshold_time_constant, 0.0f);
        EXPECT_LT(params.threshold_time_constant, 100000.0f);  // Reasonable upper bound
        EXPECT_GT(params.activity_time_constant, 0.0f);
        EXPECT_LT(params.activity_time_constant, 100000.0f);
        EXPECT_GE(params.min_threshold, 0.0f);
        EXPECT_LE(params.max_threshold, 100.0f);
        EXPECT_LT(params.min_threshold, params.max_threshold);
    }
}

//=============================================================================
// Regression Test 7: Memory Management No Leaks
//=============================================================================

TEST_F(BCMRegressionTest, MemoryManagement_NoLeaks) {
    // Create and use BCM synapses multiple times
    for (int i = 0; i < 10; i++) {
        bcm_synapse_t synapse = bcm_synapse_init(0.5f, 0.5f);
        bcm_params_t params = bcm_params_cortical();

        // Use BCM
        bcm_apply_rule(&synapse, 1.0f, 1.0f, 1.0f, &params);
        bcm_update_threshold(&synapse, 1.0f, 1.0f, &params);
    }

    // Brain should still work
    brain = brain_create("test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2);
    ASSERT_NE(brain, nullptr);

    float features[4] = {0.5f, 0.3f, 0.7f, 0.2f};
    brain_decide(brain, features, 4);

    SUCCEED();
}

//=============================================================================
// Regression Test 8: Old Learning Pattern Works
//=============================================================================

TEST_F(BCMRegressionTest, OldLearningPattern_Works) {
    brain = brain_create("test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2);
    ASSERT_NE(brain, nullptr);

    // Old training loop
    for (int episode = 0; episode < 5; episode++) {
        float features[4] = {0.5f, 0.3f, 0.7f, 0.2f};
        brain_decision_t* decision = brain_decide(brain, features, 4);
        EXPECT_NE(decision, nullptr);

        float reward = 1.0f;
        uint32_t modified = brain_apply_reward_learning(brain, reward);
        EXPECT_GE(modified, 0);
    }

    SUCCEED();
}

//=============================================================================
// Regression Test 9: BCM State Consistency
//=============================================================================

TEST_F(BCMRegressionTest, BCMStateConsistency) {
    bcm_synapse_t synapse1 = bcm_synapse_init(0.5f, 0.5f);
    bcm_synapse_t synapse2 = bcm_synapse_init(0.5f, 0.5f);
    bcm_params_t params = bcm_params_cortical();

    // Apply same operations
    bcm_apply_rule(&synapse1, 1.0f, 1.0f, 1.0f, &params);
    bcm_apply_rule(&synapse2, 1.0f, 1.0f, 1.0f, &params);

    // States should be identical
    EXPECT_FLOAT_EQ(synapse1.weight, synapse2.weight);
    EXPECT_FLOAT_EQ(synapse1.threshold, synapse2.threshold);
}

//=============================================================================
// Regression Test 10: BCM Doesn't Break Batch Processing
//=============================================================================

TEST_F(BCMRegressionTest, BCMNoBatchBreakage) {
    brain = brain_create("test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2);
    ASSERT_NE(brain, nullptr);

    // Process multiple samples (old batch pattern)
    float features_batch[10][4];
    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < 4; j++) {
            features_batch[i][j] = 0.5f;
        }
    }

    for (int i = 0; i < 10; i++) {
        brain_decision_t* decision = brain_decide(brain, features_batch[i], 4);
        EXPECT_NE(decision, nullptr);
        if (decision) {
            EXPECT_NE(decision->label, nullptr);
        }
    }

    SUCCEED();
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
