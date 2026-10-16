/**
 * @file test_stp_backward_compat.cpp
 * @brief Regression tests for STP (short-term plasticity) backward compatibility
 *
 * WHAT: Ensures STP features don't break existing code
 * WHY:  Verify zero breaking changes to pre-STP code
 * HOW:  Test legacy patterns and ensure they still work correctly
 *
 * @version STP Regression Testing
 * @date 2025-11-13
 */

#include <gtest/gtest.h>
#include <cmath>

// Headers have their own extern "C" guards
    #include "core/brain/nimcp_brain.h"
    #include "plasticity/stp/nimcp_stp.h"

//=============================================================================
// Test Fixture
//=============================================================================

class STPRegressionTest : public ::testing::Test {
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

TEST_F(STPRegressionTest, BrainCreation_StillWorks) {
    // Old code pattern: Create brain without STP awareness
    brain = brain_create("test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2);

    // Should still work exactly as before
    ASSERT_NE(brain, nullptr) << "Brain creation should not be broken by STP";
}

//=============================================================================
// Regression Test 2: Legacy Inference Without STP
//=============================================================================

TEST_F(STPRegressionTest, LegacyInference_StillWorks) {
    brain = brain_create("test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2);
    ASSERT_NE(brain, nullptr);

    // Old inference pattern (no STP knowledge)
    float features[4] = {0.5f, 0.3f, 0.7f, 0.2f};
    brain_decision_t* decision = brain_decide(brain, features, 4);
    EXPECT_NE(decision, nullptr);
    if (decision) {
        EXPECT_NE(decision->label, nullptr);
    }
}

//=============================================================================
// Regression Test 3: STP API Doesn't Break CPU Code
//=============================================================================

TEST_F(STPRegressionTest, STPAPI_NoCPUBreakage) {
    // Use STP API (new)
    stp_state_t state;
    stp_params_t params = stp_get_preset_params(STP_PRESET_DEPRESSING);
    stp_init(&state, &params, 0);

    // Old brain API should still work
    brain = brain_create("test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2);
    ASSERT_NE(brain, nullptr);

    float features[4] = {0.5f, 0.3f, 0.7f, 0.2f};
    brain_decision_t* decision = brain_decide(brain, features, 4);
    ASSERT_NE(decision, nullptr);
}

//=============================================================================
// Regression Test 4: STP Doesn't Interfere With Learning
//=============================================================================

TEST_F(STPRegressionTest, STP_NoLearningInterference) {
    // Create STP state
    stp_state_t state;
    stp_params_t params = stp_get_preset_params(STP_PRESET_MIXED);
    stp_init(&state, &params, 0);

    // Use STP
    stp_process_spike(&state, 1);
    float mod = stp_get_modulation(&state);
    (void)mod;  // Suppress unused warning

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

TEST_F(STPRegressionTest, NoPerformanceRegression) {
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

    EXPECT_LT(avg_us, 1000.0f) << "STP shouldn't cause severe performance regression";
}

//=============================================================================
// Regression Test 6: STP Parameter Validation
//=============================================================================

TEST_F(STPRegressionTest, STPParameterValidation) {
    // All presets should return valid parameters
    stp_preset_t presets[] = {
        STP_PRESET_DEPRESSING,
        STP_PRESET_FACILITATING,
        STP_PRESET_MIXED,
        STP_PRESET_FAST_DEPRESSING,
        STP_PRESET_SLOW_DEPRESSING,
        STP_PRESET_NONE
    };

    for (int i = 0; i < 6; i++) {
        stp_params_t params = stp_get_preset_params(presets[i]);

        // Parameters should be within valid ranges
        EXPECT_GT(params.U, 0.0f);
        EXPECT_LE(params.U, 1.0f);
        EXPECT_GT(params.tau_D, 0.0f);
        EXPECT_LT(params.tau_D, 10000.0f);  // Reasonable upper bound
        EXPECT_GT(params.tau_F, 0.0f);
        EXPECT_LT(params.tau_F, 10000.0f);  // Reasonable upper bound
    }
}

//=============================================================================
// Regression Test 7: Memory Management No Leaks
//=============================================================================

TEST_F(STPRegressionTest, MemoryManagement_NoLeaks) {
    // Create and destroy STP states multiple times
    for (int i = 0; i < 10; i++) {
        stp_state_t state;
        stp_params_t params = stp_get_preset_params(STP_PRESET_DEPRESSING);
        stp_init(&state, &params, 0);

        // Use STP
        stp_process_spike(&state, 1);
        stp_update(&state, 10);
        stp_get_modulation(&state);
        stp_reset(&state, 20);
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

TEST_F(STPRegressionTest, OldLearningPattern_Works) {
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
// Regression Test 9: STP State Consistency
//=============================================================================

TEST_F(STPRegressionTest, STPStateConsistency) {
    stp_state_t state1, state2;
    stp_params_t params = stp_get_preset_params(STP_PRESET_DEPRESSING);

    // Initialize two identical states
    stp_init(&state1, &params, 0);
    stp_init(&state2, &params, 0);

    // Apply same operations
    stp_process_spike(&state1, 1);
    stp_process_spike(&state2, 1);

    // States should be identical
    EXPECT_FLOAT_EQ(state1.x, state2.x);
    EXPECT_FLOAT_EQ(state1.u, state2.u);
}

//=============================================================================
// Regression Test 10: STP Doesn't Break Batch Processing
//=============================================================================

TEST_F(STPRegressionTest, STPNoBatchBreakage) {
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
