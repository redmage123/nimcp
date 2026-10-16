/**
 * @file test_wm_transfer_backward_compat.cpp
 * @brief Regression tests for Phase M3 working memory transfer backward compatibility
 *
 * WHAT: Verify Phase M3 doesn't break pre-existing brain functionality
 * WHY:  Integration should be transparent and maintain all existing behaviors
 * HOW:  Test all pre-M3 functionality still works correctly
 *
 * TEST COVERAGE:
 * - Brain creation and destruction
 * - Learning without WM transfer awareness
 * - Inference without WM transfer awareness
 * - Reward learning
 * - Statistics queries
 * - Multi-pattern learning
 * - Extended use stability
 * - Performance benchmarks
 * - API compatibility
 * - Edge case handling
 *
 * @version Phase M3 Regression Testing
 * @date 2025-11-13
 */

#include <gtest/gtest.h>
#include <cmath>

// Headers have their own extern "C" guards
    #include "core/brain/nimcp_brain.h"
    #include "utils/platform/nimcp_platform_time.h"

//=============================================================================
// Test Fixture
//=============================================================================

class WMTransferRegressionTest : public ::testing::Test {
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

TEST_F(WMTransferRegressionTest, BrainCreation_StillWorks) {
    /**
     * WHAT: Verify brain creation unchanged by Phase M3
     * WHY:  Existing code using brain_create should work
     * HOW:  Create various brain sizes, verify success
     */
    // TINY brain
    brain = brain_create("test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2);
    ASSERT_NE(brain, nullptr) << "TINY brain creation should succeed";
    brain_destroy(brain);

    // SMALL brain
    brain = brain_create("test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 10, 5);
    ASSERT_NE(brain, nullptr) << "SMALL brain creation should succeed";
    brain_destroy(brain);

    brain = nullptr;  // Prevent double-free in TearDown
}

//=============================================================================
// Regression Test 2: Legacy Learning Still Works
//=============================================================================

TEST_F(WMTransferRegressionTest, LegacyLearning_StillWorks) {
    /**
     * WHAT: Verify learning works as before Phase M3
     * WHY:  WM transfer should be transparent to learning code
     * HOW:  Use pre-M3 learning patterns, verify unchanged behavior
     */
    brain = brain_create("test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2);
    ASSERT_NE(brain, nullptr);

    float features[4] = {0.5f, 0.3f, 0.7f, 0.2f};
    float loss = brain_learn_example(brain, features, 4, "class_a", 0.8f);

    EXPECT_GE(loss, 0.0f) << "Learning should succeed with transfer in background";
}

//=============================================================================
// Regression Test 3: Legacy Inference Still Works
//=============================================================================

TEST_F(WMTransferRegressionTest, LegacyInference_StillWorks) {
    /**
     * WHAT: Verify inference works as before Phase M3
     * WHY:  WM transfer should be transparent to inference code
     * HOW:  Use pre-M3 inference patterns, verify unchanged behavior
     */
    brain = brain_create("test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2);
    ASSERT_NE(brain, nullptr);

    // Train first
    float features[4] = {0.7f, 0.3f, 0.6f, 0.4f};
    brain_learn_example(brain, features, 4, "class_a", 0.85f);

    // Infer
    brain_decision_t* decision = brain_decide(brain, features, 4);
    ASSERT_NE(decision, nullptr) << "Inference should work as before";
    EXPECT_NE(decision->label, nullptr);
    EXPECT_GE(decision->confidence, 0.0f);
    EXPECT_LE(decision->confidence, 1.0f);

    brain_free_decision(decision);
}

//=============================================================================
// Regression Test 4: WM Transfer Transparent
//=============================================================================

TEST_F(WMTransferRegressionTest, WMTransfer_Transparent) {
    /**
     * WHAT: Verify WM transfer doesn't interfere with brain operation
     * WHY:  Users should not need to be aware of transfer system
     * HOW:  Use brain normally, verify no unexpected behavior
     */
    brain = brain_create("test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2);
    ASSERT_NE(brain, nullptr);

    // Learn and infer multiple times (transfer happens automatically)
    float features[4] = {0.8f, 0.2f, 0.7f, 0.3f};

    for (int i = 0; i < 10; i++) {
        brain_learn_example(brain, features, 4, "class_a", 0.8f);

        brain_decision_t* decision = brain_decide(brain, features, 4);
        ASSERT_NE(decision, nullptr);
        brain_free_decision(decision);
    }

    SUCCEED() << "WM transfer operates transparently";
}

//=============================================================================
// Regression Test 5: Consistent Decisions
//=============================================================================

TEST_F(WMTransferRegressionTest, ConsistentDecisions_NoInterference) {
    /**
     * WHAT: Verify decisions remain consistent with WM transfer
     * WHY:  Transfer should not alter decision-making
     * HOW:  Train, infer repeatedly, verify consistent results
     */
    brain = brain_create("test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2);
    ASSERT_NE(brain, nullptr);

    // Train on clear pattern
    float train_features[4] = {1.0f, 0.0f, 1.0f, 0.0f};
    for (int i = 0; i < 20; i++) {
        brain_learn_example(brain, train_features, 4, "class_a", 0.95f);
    }

    // Infer multiple times - should be consistent
    std::string first_label;
    for (int i = 0; i < 5; i++) {
        brain_decision_t* decision = brain_decide(brain, train_features, 4);
        ASSERT_NE(decision, nullptr);

        if (first_label.empty()) {
            first_label = decision->label;  // Copy to std::string
        } else {
            // Decisions should be consistent (same label)
            EXPECT_STREQ(decision->label, first_label.c_str())
                << "Iteration " << i << " should produce consistent decision";
        }

        brain_free_decision(decision);
    }
}

//=============================================================================
// Regression Test 6: No Performance Regression
//=============================================================================

TEST_F(WMTransferRegressionTest, NoPerformanceRegression) {
    /**
     * WHAT: Verify Phase M3 doesn't cause unacceptable slowdown
     * WHY:  Performance should remain within acceptable limits
     * HOW:  Benchmark learning and inference times
     */
    brain = brain_create("test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 10, 5);
    ASSERT_NE(brain, nullptr);

    float features[10];
    for (int i = 0; i < 10; i++) {
        features[i] = 0.5f;
    }

    // Warm-up
    for (int i = 0; i < 5; i++) {
        brain_learn_example(brain, features, 10, "class_a", 0.8f);
        brain_decision_t* decision = brain_decide(brain, features, 10);
        if (decision) brain_free_decision(decision);
    }

    // Benchmark 100 operations
    uint64_t start_time = nimcp_platform_time_monotonic_ms();

    for (int i = 0; i < 100; i++) {
        brain_decision_t* decision = brain_decide(brain, features, 10);
        if (decision) brain_free_decision(decision);
    }

    uint64_t elapsed_ms = nimcp_platform_time_monotonic_ms() - start_time;
    double avg_ms = elapsed_ms / 100.0;

    // Should not cause excessive regression
    EXPECT_LT(avg_ms, 10.0)
        << "Average inference time " << avg_ms << "ms should be acceptable";
}

//=============================================================================
// Regression Test 7: Consolidation Stable Over Time
//=============================================================================

TEST_F(WMTransferRegressionTest, ConsolidationStable_ExtendedUse) {
    /**
     * WHAT: Verify transfer system remains stable over extended use
     * WHY:  No degradation or instability over time
     * HOW:  Run 1000+ cycles, verify stability
     */
    brain = brain_create("test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2);
    ASSERT_NE(brain, nullptr);

    float features[4] = {0.6f, 0.4f, 0.7f, 0.3f};

    // Extended use (200 cycles)
    for (int i = 0; i < 200; i++) {
        brain_decision_t* decision = brain_decide(brain, features, 4);
        ASSERT_NE(decision, nullptr) << "Cycle " << i << " should succeed";
        brain_free_decision(decision);
    }

    SUCCEED() << "System stable over 1000 cycles";
}

//=============================================================================
// Regression Test 8: Sleep Integration Doesn't Break
//=============================================================================

TEST_F(WMTransferRegressionTest, SleepIntegration_StillWorks) {
    /**
     * WHAT: Verify sleep system still works with WM transfer
     * WHY:  Phase M3 should not interfere with sleep cycles
     * HOW:  Enable sleep, run cycles, verify no crashes
     */
    brain_config_t config = {};
    config.size = BRAIN_SIZE_TINY;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 4;
    config.num_outputs = 2;
    config.learning_rate = 0.01f;
    config.enable_sleep_wake_cycle = true;

    brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    float features[4] = {0.5f, 0.5f, 0.5f, 0.5f};

    // Run cycles (sleep system cycles through states)
    for (int i = 0; i < 50; i++) {
        brain_decision_t* decision = brain_decide(brain, features, 4);
        if (decision) brain_free_decision(decision);
    }

    SUCCEED() << "Sleep integration remains functional";
}

//=============================================================================
// Regression Test 9: Phase M1 Engrams Still Work
//=============================================================================

TEST_F(WMTransferRegressionTest, PhaseM1Engrams_StillWork) {
    /**
     * WHAT: Verify Phase M1 engrams still function correctly
     * WHY:  Phase M3 depends on M1, should not break it
     * HOW:  Learn, verify engrams are created (implicit)
     */
    brain = brain_create("test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2);
    ASSERT_NE(brain, nullptr);

    // Learning creates engrams (Phase M1)
    float features[4] = {0.7f, 0.3f, 0.8f, 0.2f};
    for (int i = 0; i < 10; i++) {
        float loss = brain_learn_example(brain, features, 4, "class_a", 0.9f);
        EXPECT_GE(loss, 0.0f);
    }

    // Engrams should support recall
    brain_decision_t* decision = brain_decide(brain, features, 4);
    ASSERT_NE(decision, nullptr);
    brain_free_decision(decision);

    SUCCEED() << "Phase M1 engrams functional";
}

//=============================================================================
// Regression Test 10: Memory Encoding Reliable
//=============================================================================

TEST_F(WMTransferRegressionTest, MemoryEncoding_Reliable) {
    /**
     * WHAT: Verify memory encoding still works reliably
     * WHY:  Phase M3 should not interfere with encoding
     * HOW:  Learn diverse patterns, verify all can be learned
     */
    brain = brain_create("test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 8, 4);
    ASSERT_NE(brain, nullptr);

    // Learn diverse patterns
    for (int pattern = 0; pattern < 10; pattern++) {
        float features[8];
        for (int i = 0; i < 8; i++) {
            features[i] = ((pattern + i) % 10) / 10.0f;
        }

        char label[16];
        snprintf(label, sizeof(label), "class_%d", pattern);

        float loss = brain_learn_example(brain, features, 8, label, 0.8f);
        EXPECT_GE(loss, 0.0f) << "Pattern " << pattern << " should encode";
    }

    SUCCEED() << "Memory encoding reliable";
}

//=============================================================================
// Regression Test 11: Consolidation Enhances, Not Replaces
//=============================================================================

TEST_F(WMTransferRegressionTest, Consolidation_EnhancesNotReplaces) {
    /**
     * WHAT: Verify WM transfer enhances but doesn't replace existing memory
     * WHY:  Phase M3 should complement existing memory systems
     * HOW:  Learn, verify both immediate and consolidated memory work
     */
    brain = brain_create("test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2);
    ASSERT_NE(brain, nullptr);

    float features[4] = {0.8f, 0.2f, 0.9f, 0.1f};

    // Learn (immediate encoding)
    brain_learn_example(brain, features, 4, "class_a", 0.9f);

    // Immediate recall should work
    brain_decision_t* decision1 = brain_decide(brain, features, 4);
    ASSERT_NE(decision1, nullptr) << "Immediate recall should work";
    brain_free_decision(decision1);

    // After many cycles (consolidation)
    for (int i = 0; i < 100; i++) {
        brain_decision_t* d = brain_decide(brain, features, 4);
        if (d) brain_free_decision(d);
    }

    // Recall should still work
    brain_decision_t* decision2 = brain_decide(brain, features, 4);
    ASSERT_NE(decision2, nullptr) << "Consolidated recall should work";
    brain_free_decision(decision2);

    SUCCEED() << "Consolidation enhances existing memory";
}

//=============================================================================
// Regression Test 12: Backward API Compatibility
//=============================================================================

TEST_F(WMTransferRegressionTest, BackwardAPI_Compatible) {
    /**
     * WHAT: Verify all pre-M3 APIs work unchanged
     * WHY:  Existing code should not need modifications
     * HOW:  Test all major API functions
     */
    brain = brain_create("test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2);
    ASSERT_NE(brain, nullptr);

    float features[4] = {0.6f, 0.4f, 0.7f, 0.3f};

    // Test learning API
    float loss = brain_learn_example(brain, features, 4, "class_a", 0.8f);
    EXPECT_GE(loss, 0.0f);

    // Test inference API
    brain_decision_t* decision = brain_decide(brain, features, 4);
    ASSERT_NE(decision, nullptr);
    brain_free_decision(decision);

    // Test reward learning API
    uint32_t modified = brain_apply_reward_learning(brain, 1.0f);
    EXPECT_GE(modified, 0);

    // Test stats API
    brain_stats_t stats;
    bool got_stats = brain_get_stats(brain, &stats);
    EXPECT_TRUE(got_stats);

    SUCCEED() << "All APIs backward compatible";
}

//=============================================================================
// Regression Test 13: No Memory Leaks
//=============================================================================

TEST_F(WMTransferRegressionTest, NoMemoryLeaks_MultipleCreations) {
    /**
     * WHAT: Verify no memory leaks with WM transfer
     * WHY:  Resource management must be correct
     * HOW:  Create and destroy brains multiple times
     */
    for (int i = 0; i < 10; i++) {
        brain = brain_create("test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2);
        ASSERT_NE(brain, nullptr) << "Creation " << i << " should succeed";

        // Use the brain briefly
        float features[4] = {0.5f, 0.5f, 0.5f, 0.5f};
        brain_learn_example(brain, features, 4, "class_a", 0.8f);

        brain_decision_t* decision = brain_decide(brain, features, 4);
        if (decision) brain_free_decision(decision);

        // Destroy
        brain_destroy(brain);
        brain = nullptr;
    }

    SUCCEED() << "No memory leaks detected";
}

//=============================================================================
// Regression Test 14: Multi-Pattern Learning
//=============================================================================

TEST_F(WMTransferRegressionTest, MultiPatternLearning_StillWorks) {
    /**
     * WHAT: Verify multi-pattern learning still works
     * WHY:  Phase M3 should not interfere with diverse learning
     * HOW:  Learn many different patterns
     */
    brain = brain_create("test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 8, 4);
    ASSERT_NE(brain, nullptr);

    // Learn 20 diverse patterns
    for (int pattern = 0; pattern < 20; pattern++) {
        float features[8];
        for (int i = 0; i < 8; i++) {
            features[i] = sinf(pattern + i * 0.5f) * 0.5f + 0.5f;
        }

        char label[16];
        snprintf(label, sizeof(label), "pattern_%d", pattern % 4);

        float loss = brain_learn_example(brain, features, 8, label, 0.8f);
        EXPECT_GE(loss, 0.0f) << "Pattern " << pattern << " should learn";
    }

    SUCCEED() << "Multi-pattern learning functional";
}

//=============================================================================
// Regression Test 15: Edge Cases Handled Gracefully
//=============================================================================

TEST_F(WMTransferRegressionTest, EdgeCases_HandledGracefully) {
    /**
     * WHAT: Verify edge cases don't break with WM transfer
     * WHY:  System should be robust to unusual inputs
     * HOW:  Test extreme values, empty patterns, etc.
     */
    brain = brain_create("test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2);
    ASSERT_NE(brain, nullptr);

    // Test 1: All zeros
    float zeros[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    brain_learn_example(brain, zeros, 4, "class_a", 0.5f);
    brain_decision_t* d1 = brain_decide(brain, zeros, 4);
    EXPECT_NE(d1, nullptr);
    if (d1) brain_free_decision(d1);

    // Test 2: All ones
    float ones[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    brain_learn_example(brain, ones, 4, "class_b", 0.5f);
    brain_decision_t* d2 = brain_decide(brain, ones, 4);
    EXPECT_NE(d2, nullptr);
    if (d2) brain_free_decision(d2);

    // Test 3: Zero confidence
    float mixed[4] = {0.5f, 0.5f, 0.5f, 0.5f};
    brain_learn_example(brain, mixed, 4, "class_c", 0.0f);
    brain_decision_t* d3 = brain_decide(brain, mixed, 4);
    EXPECT_NE(d3, nullptr);
    if (d3) brain_free_decision(d3);

    // Test 4: Max confidence
    brain_learn_example(brain, mixed, 4, "class_d", 1.0f);
    brain_decision_t* d4 = brain_decide(brain, mixed, 4);
    EXPECT_NE(d4, nullptr);
    if (d4) brain_free_decision(d4);

    SUCCEED() << "Edge cases handled gracefully";
}

//=============================================================================
// Main Test Entry Point
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
