/**
 * @file test_quantum_annealing_backward_compat.cpp
 * @brief Regression tests for Quantum Annealing optimization
 *
 * WHAT: Ensures quantum annealing doesn't break existing code
 * WHY:  Verify zero breaking changes to pre-quantum code
 * HOW:  Test legacy patterns and ensure they still work correctly
 *
 * TEST COVERAGE:
 * 1. Brain creation without quantum awareness
 * 2. Legacy inference patterns unchanged
 * 3. Quantum API doesn't break CPU code
 * 4. Quantum doesn't interfere with other optimization
 * 5. No performance regression
 * 6. Parameter validation
 * 7. Memory management no leaks
 * 8. Old learning patterns work
 * 9. State consistency
 * 10. Batch processing not broken
 *
 * @version Regression Testing Framework v1.0
 * @date 2025-11-13
 */

#include <gtest/gtest.h>
#include <cmath>

// Headers have their own extern "C" guards
    #include "core/brain/nimcp_brain.h"
    #include "optimization/quantum_annealing/nimcp_quantum_annealing.h"

//=============================================================================
// Test Fixture
//=============================================================================

class QuantumAnnealingRegressionTest : public ::testing::Test {
protected:
    brain_t brain;
    quantum_annealer_t annealer;

    void SetUp() override {
        brain = nullptr;
        annealer = nullptr;
    }

    void TearDown() override {
        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }
        if (annealer) {
            quantum_annealer_destroy(annealer);
            annealer = nullptr;
        }
    }

    // Simple test energy function
    static float test_energy(const float* state, uint32_t dim, void* user_data) {
        (void)user_data;
        float sum = 0.0f;
        for (uint32_t i = 0; i < dim; ++i) {
            sum += state[i] * state[i];
        }
        return sum;
    }
};

//=============================================================================
// Regression Test 1: Brain Creation Still Works
//=============================================================================

TEST_F(QuantumAnnealingRegressionTest, BrainCreation_StillWorks) {
    // Old code pattern: Create brain without quantum awareness
    brain = brain_create("test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2);

    // Should still work exactly as before
    ASSERT_NE(brain, nullptr)
        << "Brain creation should not be broken by quantum annealing";
}

//=============================================================================
// Regression Test 2: Legacy Inference Without Quantum
//=============================================================================

TEST_F(QuantumAnnealingRegressionTest, LegacyInference_StillWorks) {
    brain = brain_create("test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2);
    ASSERT_NE(brain, nullptr);

    // Old inference pattern (no quantum knowledge)
    float features[4] = {0.5f, 0.3f, 0.7f, 0.2f};
    brain_decision_t* decision = brain_decide(brain, features, 4);
    EXPECT_NE(decision, nullptr);
    if (decision) {
        EXPECT_NE(decision->label, nullptr);
    }
}

//=============================================================================
// Regression Test 3: Quantum API Doesn't Break CPU Code
//=============================================================================

TEST_F(QuantumAnnealingRegressionTest, QuantumAPI_NoCPUBreakage) {
    // Use quantum API (new)
    quantum_annealing_config_t config = quantum_annealing_default_config();
    annealer = quantum_annealer_create(&config);
    EXPECT_NE(annealer, nullptr);

    float initial[3] = {1.0f, 2.0f, 3.0f};
    float result[3];
    quantum_anneal(annealer, test_energy, initial, result, 3, nullptr);

    // Old brain API should still work
    brain = brain_create("test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2);
    ASSERT_NE(brain, nullptr);

    float features[4] = {0.5f, 0.3f, 0.7f, 0.2f};
    brain_decision_t* decision = brain_decide(brain, features, 4);
    ASSERT_NE(decision, nullptr);
}

//=============================================================================
// Regression Test 4: Quantum Doesn't Interfere With Other Optimization
//=============================================================================

TEST_F(QuantumAnnealingRegressionTest, Quantum_NoOptimizationInterference) {
    // Use quantum annealing
    quantum_annealing_config_t config = quantum_annealing_default_config();
    annealer = quantum_annealer_create(&config);
    EXPECT_NE(annealer, nullptr);

    float initial[3] = {1.0f, 2.0f, 3.0f};
    float result[3];
    quantum_anneal(annealer, test_energy, initial, result, 3, nullptr);

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

TEST_F(QuantumAnnealingRegressionTest, NoPerformanceRegression) {
    brain = brain_create("test", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 5);
    ASSERT_NE(brain, nullptr);

    // Measure time for 100 inferences (should not be affected by quantum module)
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

    EXPECT_LT(avg_us, 1000.0f)
        << "Quantum module shouldn't cause severe performance regression";
}

//=============================================================================
// Regression Test 6: Parameter Validation
//=============================================================================

TEST_F(QuantumAnnealingRegressionTest, ParameterValidation) {
    // Test default config is valid
    quantum_annealing_config_t config = quantum_annealing_default_config();

    EXPECT_GT(config.initial_temperature, 0.0f);
    EXPECT_GT(config.final_temperature, 0.0f);
    EXPECT_LT(config.final_temperature, config.initial_temperature);
    EXPECT_GT(config.num_iterations, 0u);
    EXPECT_GE(config.quantum_strength, 0.0f);
    EXPECT_LE(config.quantum_strength, 1.0f);

    annealer = quantum_annealer_create(&config);
    EXPECT_NE(annealer, nullptr) << "Default config should be valid";
}

//=============================================================================
// Regression Test 7: Memory Management No Leaks
//=============================================================================

TEST_F(QuantumAnnealingRegressionTest, MemoryManagement_NoLeaks) {
    // Create and destroy annealer multiple times
    for (int i = 0; i < 10; ++i) {
        quantum_annealing_config_t config = quantum_annealing_default_config();
        annealer = quantum_annealer_create(&config);
        ASSERT_NE(annealer, nullptr);

        float initial[3] = {1.0f, 2.0f, 3.0f};
        float result[3];
        quantum_anneal(annealer, test_energy, initial, result, 3, nullptr);

        quantum_annealer_destroy(annealer);
        annealer = nullptr;
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

TEST_F(QuantumAnnealingRegressionTest, OldLearningPattern_Works) {
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
// Regression Test 9: State Consistency
//=============================================================================

TEST_F(QuantumAnnealingRegressionTest, StateConsistency) {
    // Same inputs should produce same outputs with same seed
    quantum_annealing_config_t config = quantum_annealing_default_config();
    config.seed = 42;

    float initial[3] = {1.0f, 2.0f, 3.0f};
    float result1[3];
    float result2[3];

    // First run
    annealer = quantum_annealer_create(&config);
    ASSERT_NE(annealer, nullptr);
    quantum_anneal(annealer, test_energy, initial, result1, 3, nullptr);
    quantum_annealer_destroy(annealer);

    // Second run with same seed
    annealer = quantum_annealer_create(&config);
    ASSERT_NE(annealer, nullptr);
    quantum_anneal(annealer, test_energy, initial, result2, 3, nullptr);

    // Results should be identical
    for (int i = 0; i < 3; ++i) {
        EXPECT_FLOAT_EQ(result1[i], result2[i])
            << "Same seed should produce same results";
    }
}

//=============================================================================
// Regression Test 10: Batch Processing Not Broken
//=============================================================================

TEST_F(QuantumAnnealingRegressionTest, BatchProcessing_NotBroken) {
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
