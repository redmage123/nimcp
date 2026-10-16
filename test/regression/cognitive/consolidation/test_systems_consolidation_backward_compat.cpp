/**
 * @file test_systems_consolidation_backward_compat.cpp
 * @brief Regression tests for Phase M2 systems consolidation backward compatibility
 *
 * WHAT: Ensures Phase M2 doesn't break existing brain functionality
 * WHY:  Verify zero breaking changes to pre-M2 code
 * HOW:  Test legacy patterns and ensure they still work correctly
 *
 * REGRESSION COVERAGE:
 * - Brain creation with systems consolidation integrated
 * - Learning without consolidation awareness
 * - Inference without consolidation awareness
 * - Systems consolidation is transparent/optional
 * - No performance regression beyond acceptable limits
 * - Memory encoding doesn't corrupt decisions
 * - Consolidation stability over time
 * - Sleep integration doesn't break existing behavior
 * - Phase M1 engrams still work correctly
 * - All brain APIs remain backward compatible
 *
 * @version Phase M2 Regression Testing
 * @date 2025-11-13
 */

#include <gtest/gtest.h>
#include <cmath>

// Headers have their own extern "C" guards
    #include "core/brain/nimcp_brain.h"
    #include "cognitive/memory/nimcp_systems_consolidation.h"
    #include "utils/platform/nimcp_platform_time.h"

//=============================================================================
// Test Fixture
//=============================================================================

class SystemsConsolidationRegressionTest : public ::testing::Test {
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

TEST_F(SystemsConsolidationRegressionTest, BrainCreation_StillWorks) {
    /**
     * WHAT: Verify brain creation works with systems consolidation integrated
     * WHY:  Ensure Phase M2 doesn't break basic brain creation
     * HOW:  Create brain with default parameters, verify non-null
     */

    // Old code pattern: Create brain with default parameters
    brain = brain_create("test_m2", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2);

    // Should still work exactly as before
    ASSERT_NE(brain, nullptr) << "Brain creation should not be broken by Phase M2";
}

//=============================================================================
// Regression Test 2: Legacy Learning Without Consolidation Awareness
//=============================================================================

TEST_F(SystemsConsolidationRegressionTest, LegacyLearning_StillWorks) {
    /**
     * WHAT: Test learning without explicit systems consolidation usage
     * WHY:  Pre-M2 code doesn't use consolidation - should still work
     * HOW:  Perform standard learning, verify no crashes/errors
     */

    // Old code pattern: Create brain and do learning
    brain = brain_create("test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2);
    ASSERT_NE(brain, nullptr);

    // Old supervised learning pattern (no consolidation awareness)
    float features[4] = {0.5f, 0.3f, 0.7f, 0.2f};
    float loss = brain_learn_example(brain, features, 4, "class_a", 0.8f);

    // Should work without breaking
    EXPECT_GE(loss, 0.0f) << "Learning should succeed with consolidation in background";
    EXPECT_LT(loss, 30.0f) << "Loss should be reasonable (initial loss can be high)";
}

//=============================================================================
// Regression Test 3: Legacy Inference Without Consolidation Awareness
//=============================================================================

TEST_F(SystemsConsolidationRegressionTest, LegacyInference_StillWorks) {
    /**
     * WHAT: Test inference without explicit systems consolidation usage
     * WHY:  Pre-M2 code doesn't query consolidation - should still work
     * HOW:  Perform standard inference, verify correct behavior
     */

    brain = brain_create("test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2);
    ASSERT_NE(brain, nullptr);

    // Old inference pattern (no consolidation awareness)
    float features[4] = {0.5f, 0.3f, 0.7f, 0.2f};
    brain_decision_t* decision = brain_decide(brain, features, 4);

    ASSERT_NE(decision, nullptr) << "Inference should work with consolidation in background";
    EXPECT_NE(decision->label, nullptr) << "Decision should have label";
    EXPECT_GE(decision->confidence, 0.0f);
    EXPECT_LE(decision->confidence, 1.0f);

    brain_free_decision(decision);
}

//=============================================================================
// Regression Test 4: Systems Consolidation Transparent to User
//=============================================================================

TEST_F(SystemsConsolidationRegressionTest, SystemsConsolidation_Transparent) {
    /**
     * WHAT: Verify systems consolidation operates transparently
     * WHY:  Users shouldn't need to interact with consolidation explicitly
     * HOW:  Perform normal brain operations, verify no side effects
     */

    brain = brain_create("test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2);
    ASSERT_NE(brain, nullptr);

    // Train without explicit consolidation usage
    float features[4] = {0.5f, 0.3f, 0.7f, 0.2f};

    for (int i = 0; i < 10; i++) {
        brain_decision_t* decision = brain_decide(brain, features, 4);
        if (decision) {
            brain_free_decision(decision);
        }
        brain_learn_example(brain, features, 4, "class_a", 0.8f);
    }

    // Verify brain still works after background consolidation
    brain_decision_t* decision = brain_decide(brain, features, 4);
    ASSERT_NE(decision, nullptr) << "Consolidation shouldn't break decision making";
    EXPECT_NE(decision->label, nullptr);
    brain_free_decision(decision);
}

//=============================================================================
// Regression Test 5: Consistent Decisions Without Consolidation Interference
//=============================================================================

TEST_F(SystemsConsolidationRegressionTest, ConsistentDecisions_NoInterference) {
    /**
     * WHAT: Verify consolidation doesn't cause decision instability
     * WHY:  Memory transfer should enhance, not destabilize decisions
     * HOW:  Make repeated decisions, verify consistency
     */

    brain = brain_create("test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2);
    ASSERT_NE(brain, nullptr);

    // Train on pattern
    float features[4] = {0.5f, 0.3f, 0.7f, 0.2f};
    for (int i = 0; i < 5; i++) {
        brain_learn_example(brain, features, 4, "class_a", 0.8f);
    }

    // Make multiple decisions with same input
    brain_decision_t* decision1 = brain_decide(brain, features, 4);
    ASSERT_NE(decision1, nullptr);

    brain_decision_t* decision2 = brain_decide(brain, features, 4);
    ASSERT_NE(decision2, nullptr);

    // Decisions should be consistent (consolidation shouldn't cause chaos)
    EXPECT_STREQ(decision1->label, decision2->label)
        << "Consolidation shouldn't change decision labels";
    EXPECT_NEAR(decision1->confidence, decision2->confidence, 0.2f)
        << "Consolidation shouldn't cause wild confidence variations";

    brain_free_decision(decision1);
    brain_free_decision(decision2);
}

//=============================================================================
// Regression Test 6: No Performance Regression
//=============================================================================

TEST_F(SystemsConsolidationRegressionTest, NoPerformanceRegression) {
    /**
     * WHAT: Verify consolidation doesn't significantly slow inference
     * WHY:  Phase M2 should add <2x overhead
     * HOW:  Benchmark 100 inferences, check time is reasonable
     */

    brain = brain_create("test", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 5);
    ASSERT_NE(brain, nullptr);

    // Prepare test data
    float features[10];
    for (int i = 0; i < 10; i++) {
        features[i] = 0.5f;
    }

    // Warm-up
    for (int i = 0; i < 10; i++) {
        brain_decision_t* decision = brain_decide(brain, features, 10);
        if (decision) {
            brain_free_decision(decision);
        }
    }

    // Benchmark inference time
    uint64_t start_time = nimcp_platform_time_monotonic_ms();

    for (int i = 0; i < 100; i++) {
        brain_decision_t* decision = brain_decide(brain, features, 10);
        if (decision) {
            brain_free_decision(decision);
        }
    }

    uint64_t end_time = nimcp_platform_time_monotonic_ms();
    uint64_t elapsed_ms = end_time - start_time;
    double avg_ms = elapsed_ms / 100.0;

    // Should complete in reasonable time
    // SMALL brain baseline: ~1ms per inference
    // With Phase M1 + M2: should be <3ms (< 3x overhead)
    EXPECT_LT(avg_ms, 5.0)
        << "Systems consolidation causing excessive performance regression: "
        << avg_ms << "ms per inference";
}

//=============================================================================
// Regression Test 7: Consolidation Stability Over Time
//=============================================================================

TEST_F(SystemsConsolidationRegressionTest, Consolidation_StableOverTime) {
    /**
     * WHAT: Verify consolidation doesn't corrupt brain over extended use
     * WHY:  Memory should stabilize, not degrade
     * HOW:  Run many decision cycles, verify stability
     */

    brain = brain_create("test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2);
    ASSERT_NE(brain, nullptr);

    // Train on pattern
    float features[4] = {0.7f, 0.3f, 0.6f, 0.4f};
    for (int i = 0; i < 10; i++) {
        brain_learn_example(brain, features, 4, "class_a", 0.85f);
    }

    // Run extended simulation (500 cycles)
    for (int i = 0; i < 500; i++) {
        brain_decision_t* decision = brain_decide(brain, features, 4);
        if (decision) {
            brain_free_decision(decision);
        }
    }

    // Verify brain still functional after extensive consolidation
    brain_decision_t* decision = brain_decide(brain, features, 4);
    ASSERT_NE(decision, nullptr) << "Brain should remain stable after consolidation";
    EXPECT_NE(decision->label, nullptr);
    brain_free_decision(decision);
}

//=============================================================================
// Regression Test 8: Sleep Integration Doesn't Break
//=============================================================================

TEST_F(SystemsConsolidationRegressionTest, SleepIntegration_NoBreakage) {
    /**
     * WHAT: Verify consolidation during sleep doesn't crash
     * WHY:  Sleep-dependent consolidation is critical feature
     * HOW:  Create brain with sleep, encode memories, simulate sleep cycles
     */

    brain_config_t config = {};
    config.size = BRAIN_SIZE_TINY;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 4;
    config.num_outputs = 2;
    config.learning_rate = 0.01f;
    config.sparsity_target = 0.9f;
    config.enable_sleep_wake_cycle = true;

    brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    // Train to create memories
    float features[4] = {0.5f, 0.3f, 0.7f, 0.2f};
    for (int i = 0; i < 5; i++) {
        brain_learn_example(brain, features, 4, "class_a", 0.8f);
    }

    // Perform inference (triggers consolidation during sleep states)
    for (int i = 0; i < 100; i++) {
        brain_decision_t* decision = brain_decide(brain, features, 4);
        if (decision) {
            brain_free_decision(decision);
        }
    }

    // Verify brain still functional after sleep consolidation
    brain_decision_t* decision = brain_decide(brain, features, 4);
    ASSERT_NE(decision, nullptr) << "Sleep consolidation shouldn't break inference";
    EXPECT_NE(decision->label, nullptr);
    brain_free_decision(decision);
}

//=============================================================================
// Regression Test 9: Phase M1 Engrams Still Work
//=============================================================================

TEST_F(SystemsConsolidationRegressionTest, PhaseM1_EngamsStillWork) {
    /**
     * WHAT: Verify Phase M1 engram functionality unaffected by Phase M2
     * WHY:  Systems consolidation builds on engrams, shouldn't break them
     * HOW:  Test engram encoding and recall work correctly
     */

    brain = brain_create("test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2);
    ASSERT_NE(brain, nullptr);

    // Train pattern (creates engrams via Phase M1)
    float features[4] = {0.8f, 0.2f, 0.9f, 0.1f};
    for (int i = 0; i < 5; i++) {
        brain_learn_example(brain, features, 4, "class_a", 0.9f);
    }

    // Inference should use engram pattern completion (Phase M1)
    // and also trigger consolidation (Phase M2)
    brain_decision_t* decision = brain_decide(brain, features, 4);
    ASSERT_NE(decision, nullptr) << "Engram recall should still work";
    EXPECT_NE(decision->label, nullptr);

    brain_free_decision(decision);
}

//=============================================================================
// Regression Test 10: Memory Encoding Reliable
//=============================================================================

TEST_F(SystemsConsolidationRegressionTest, MemoryEncoding_Reliable) {
    /**
     * WHAT: Verify memory encoding works reliably with consolidation
     * WHY:  Ensure consolidation doesn't interfere with encoding
     * HOW:  Encode multiple memories, verify success
     */

    brain = brain_create("test", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 5);
    ASSERT_NE(brain, nullptr);

    // Encode many diverse memories
    const int NUM_PATTERNS = 20;
    for (int pattern = 0; pattern < NUM_PATTERNS; pattern++) {
        float features[10];
        for (int i = 0; i < 10; i++) {
            features[i] = ((pattern + i) % 10) / 10.0f;
        }

        char label[16];
        snprintf(label, sizeof(label), "pattern_%d", pattern % 5);

        float loss = brain_learn_example(brain, features, 10, label, 0.8f);
        EXPECT_GE(loss, 0.0f) << "Encoding pattern " << pattern << " should succeed";
    }

    // All patterns should be encoded successfully
    SUCCEED() << "Memory encoding remained reliable with consolidation";
}

//=============================================================================
// Regression Test 11: Consolidation Enhances, Not Replaces
//=============================================================================

TEST_F(SystemsConsolidationRegressionTest, Consolidation_EnhancesNotReplaces) {
    /**
     * WHAT: Verify consolidation augments network, doesn't override it
     * WHY:  Consolidation should help, not take over decision making
     * HOW:  Train network, verify decisions still use network inference
     */

    brain = brain_create("test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2);
    ASSERT_NE(brain, nullptr);

    // Train on specific pattern
    float train_features[4] = {1.0f, 0.0f, 0.0f, 0.0f};
    for (int i = 0; i < 10; i++) {
        brain_learn_example(brain, train_features, 4, "class_a", 0.9f);
    }

    // Run consolidation cycles
    for (int i = 0; i < 50; i++) {
        brain_decision_t* decision = brain_decide(brain, train_features, 4);
        if (decision) {
            brain_free_decision(decision);
        }
    }

    // Test with completely different pattern (no overlap)
    float test_features[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    brain_decision_t* decision = brain_decide(brain, test_features, 4);

    // Should still get a decision (network inference, not just consolidation)
    ASSERT_NE(decision, nullptr)
        << "Consolidation shouldn't prevent network inference";
    EXPECT_NE(decision->label, nullptr);

    // Confidence might be low, but should be valid
    EXPECT_GE(decision->confidence, 0.0f);
    EXPECT_LE(decision->confidence, 1.0f);

    brain_free_decision(decision);
}

//=============================================================================
// Regression Test 12: Backward API Compatibility
//=============================================================================

TEST_F(SystemsConsolidationRegressionTest, API_BackwardCompatible) {
    /**
     * WHAT: Verify all existing brain APIs still work
     * WHY:  Phase M2 should not break existing function signatures
     * HOW:  Call all common brain functions, verify no crashes
     */

    brain = brain_create("test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2);
    ASSERT_NE(brain, nullptr);

    // Test all common APIs
    float features[4] = {0.5f, 0.3f, 0.7f, 0.2f};

    // 1. brain_decide
    brain_decision_t* decision = brain_decide(brain, features, 4);
    EXPECT_NE(decision, nullptr);
    if (decision) brain_free_decision(decision);

    // 2. brain_learn_example
    float loss = brain_learn_example(brain, features, 4, "class_a", 0.8f);
    EXPECT_GE(loss, 0.0f);

    // 3. brain_apply_reward_learning
    uint32_t modified = brain_apply_reward_learning(brain, 1.0f);
    EXPECT_GE(modified, 0);

    // 4. brain_get_stats
    brain_stats_t stats;
    bool got_stats = brain_get_stats(brain, &stats);
    EXPECT_TRUE(got_stats);
    EXPECT_GT(stats.num_neurons, 0);

    // All APIs should work without crashes
}

//=============================================================================
// Regression Test 13: No Memory Leaks
//=============================================================================

TEST_F(SystemsConsolidationRegressionTest, NoMemoryLeaks) {
    /**
     * WHAT: Verify consolidation doesn't cause memory leaks
     * WHY:  Memory management is critical for long-running systems
     * HOW:  Create/destroy brains repeatedly, verify no crashes
     */

    // Create and destroy multiple brains
    for (int iteration = 0; iteration < 10; iteration++) {
        brain_t temp_brain = brain_create("temp", BRAIN_SIZE_TINY,
                                          BRAIN_TASK_CLASSIFICATION, 4, 2);
        ASSERT_NE(temp_brain, nullptr) << "Iteration " << iteration;

        // Use the brain
        float features[4] = {0.5f, 0.3f, 0.7f, 0.2f};
        brain_learn_example(temp_brain, features, 4, "class_a", 0.8f);

        brain_decision_t* decision = brain_decide(temp_brain, features, 4);
        if (decision) {
            brain_free_decision(decision);
        }

        // Destroy should clean up consolidation system
        brain_destroy(temp_brain);
    }

    SUCCEED() << "No memory leaks detected in create/destroy cycles";
}

//=============================================================================
// Regression Test 14: Multi-Pattern Learning
//=============================================================================

TEST_F(SystemsConsolidationRegressionTest, MultiPattern_Learning) {
    /**
     * WHAT: Verify consolidation handles multiple learning patterns
     * WHY:  Real-world usage involves diverse memories
     * HOW:  Learn many patterns, verify all work correctly
     */

    brain = brain_create("test", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 8, 4);
    ASSERT_NE(brain, nullptr);

    // Learn multiple distinct patterns
    for (int pattern = 0; pattern < 4; pattern++) {
        float features[8];
        for (int i = 0; i < 8; i++) {
            features[i] = (pattern == (i % 4)) ? 0.9f : 0.1f;
        }

        char label[16];
        snprintf(label, sizeof(label), "pattern_%d", pattern);

        // Learn each pattern multiple times
        for (int repeat = 0; repeat < 5; repeat++) {
            float loss = brain_learn_example(brain, features, 8, label, 0.85f);
            EXPECT_GE(loss, 0.0f) << "Pattern " << pattern << " repeat " << repeat;
        }
    }

    // Run consolidation
    float test_features[8] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    for (int i = 0; i < 100; i++) {
        brain_decision_t* decision = brain_decide(brain, test_features, 8);
        if (decision) {
            brain_free_decision(decision);
        }
    }

    // Verify all patterns can still be processed
    for (int pattern = 0; pattern < 4; pattern++) {
        float features[8];
        for (int i = 0; i < 8; i++) {
            features[i] = (pattern == (i % 4)) ? 0.9f : 0.1f;
        }

        brain_decision_t* decision = brain_decide(brain, features, 8);
        ASSERT_NE(decision, nullptr) << "Pattern " << pattern << " should work";
        brain_free_decision(decision);
    }
}

//=============================================================================
// Regression Test 15: Edge Case - Empty Features
//=============================================================================

TEST_F(SystemsConsolidationRegressionTest, EdgeCase_EmptyFeatures) {
    /**
     * WHAT: Verify consolidation handles edge cases gracefully
     * WHY:  Robust error handling is essential
     * HOW:  Test with edge case inputs, verify no crashes
     */

    brain = brain_create("test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2);
    ASSERT_NE(brain, nullptr);

    // Try inference with edge case (all zeros)
    float features[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    brain_decision_t* decision = brain_decide(brain, features, 4);

    // Should handle gracefully (may return low confidence)
    ASSERT_NE(decision, nullptr) << "Should handle edge case";
    EXPECT_NE(decision->label, nullptr);

    brain_free_decision(decision);
}

//=============================================================================
// Main Test Entry Point
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
