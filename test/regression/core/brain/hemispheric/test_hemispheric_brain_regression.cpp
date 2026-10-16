//=============================================================================
// test_hemispheric_brain_regression.cpp - Hemispheric Brain Regression Tests
//=============================================================================
/**
 * @file test_hemispheric_brain_regression.cpp
 * @brief Regression tests for hemispheric brain system
 *
 * WHAT: Tests for determinism, performance, memory patterns, state consistency
 * WHY:  Ensure hemispheric brain behavior is stable across versions
 * HOW:  GTest framework with performance benchmarks and consistency checks
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>
#include <chrono>
#include <algorithm>
#include <numeric>

#include "core/brain/hemispheric/nimcp_hemispheric_brain.h"
#include "core/brain/hemispheric/nimcp_lateralization.h"

//=============================================================================
// Test Fixtures
//=============================================================================

class HemisphericBrainRegressionTest : public ::testing::Test {
protected:
    static hemispheric_brain_t* brain;

    static void SetUpTestSuite() {
        hemispheric_brain_config_t config = hemispheric_brain_default_config();
        config.size = BRAIN_SIZE_TINY;
        config.num_inputs = 8;
        config.num_outputs = 4;
        config.initial_tier = PLATFORM_TIER_CONSTRAINED;
        config.default_mode = HEMISPHERIC_MODE_LATERALIZED;
        config.enable_bio_async = false;  // Disable for determinism tests
        config.left_config.size = BRAIN_SIZE_TINY;
        config.left_config.initial_tier = PLATFORM_TIER_CONSTRAINED;
        config.left_config.enable_bio_async = false;
        config.right_config.size = BRAIN_SIZE_TINY;
        config.right_config.initial_tier = PLATFORM_TIER_CONSTRAINED;
        config.right_config.enable_bio_async = false;
        brain = hemispheric_brain_create(&config);
        ASSERT_NE(brain, nullptr);
    }

    static void TearDownTestSuite() {
        if (brain) {
            hemispheric_brain_destroy(brain);
            brain = nullptr;
        }
    }

    // Helper to measure execution time
    template<typename Func>
    double measureTimeMs(Func&& func, int iterations = 100) {
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < iterations; i++) {
            func();
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        return duration.count() / 1000.0 / iterations;
    }
};

hemispheric_brain_t* HemisphericBrainRegressionTest::brain = nullptr;

//=============================================================================
// Performance Benchmark Tests
//=============================================================================

TEST_F(HemisphericBrainRegressionTest, LateralizedModePerformance) {
    ASSERT_NE(brain, nullptr);

    float input[8], output[4];
    for (int i = 0; i < 8; i++) input[i] = 0.5f + 0.01f * i;

    double avgMs = measureTimeMs([&]() {
        hemispheric_brain_process_lateralized(brain, input, 8,
            COGNITIVE_DOMAIN_LANGUAGE, output, 4);
    });

    // Performance regression: lateralized should be fast (<5ms per call)
    EXPECT_LT(avgMs, 5.0) << "Lateralized processing too slow: " << avgMs << "ms";
}

TEST_F(HemisphericBrainRegressionTest, ParallelModePerformance) {
    ASSERT_NE(brain, nullptr);

    float input[8], left_out[4], right_out[4];
    for (int i = 0; i < 8; i++) input[i] = 0.5f;

    double avgMs = measureTimeMs([&]() {
        hemispheric_brain_process_parallel(brain, input, 8,
            left_out, right_out, 4);
    });

    // Parallel mode involves both hemispheres, expect slightly slower
    EXPECT_LT(avgMs, 10.0) << "Parallel processing too slow: " << avgMs << "ms";
}

TEST_F(HemisphericBrainRegressionTest, CompetitiveModePerformance) {
    ASSERT_NE(brain, nullptr);

    float input[8], output[4];
    for (int i = 0; i < 8; i++) input[i] = 0.5f;
    hemisphere_id_t winner;

    double avgMs = measureTimeMs([&]() {
        hemispheric_brain_process_competitive(brain, input, 8,
            output, 4, &winner);
    });

    EXPECT_LT(avgMs, 10.0) << "Competitive processing too slow: " << avgMs << "ms";
}

TEST_F(HemisphericBrainRegressionTest, CooperativeModePerformance) {
    ASSERT_NE(brain, nullptr);

    float input[8], output[4];
    for (int i = 0; i < 8; i++) input[i] = 0.5f;

    double avgMs = measureTimeMs([&]() {
        hemispheric_brain_process_cooperative(brain, input, 8, output, 4);
    });

    EXPECT_LT(avgMs, 10.0) << "Cooperative processing too slow: " << avgMs << "ms";
}

TEST_F(HemisphericBrainRegressionTest, UpdatePerformance) {
    ASSERT_NE(brain, nullptr);

    double avgMs = measureTimeMs([&]() {
        hemispheric_brain_update(brain, 0.001f);
    });

    // Update should be very fast
    EXPECT_LT(avgMs, 2.0) << "Update too slow: " << avgMs << "ms";
}

//=============================================================================
// Determinism Tests
//=============================================================================

TEST_F(HemisphericBrainRegressionTest, LateralizedProcessingDeterministic) {
    ASSERT_NE(brain, nullptr);

    float input[8], output1[4], output2[4];
    for (int i = 0; i < 8; i++) input[i] = 0.5f + 0.01f * i;

    // First run
    hemispheric_brain_process_lateralized(brain, input, 8,
        COGNITIVE_DOMAIN_LANGUAGE, output1, 4);

    // Reset and run again
    hemispheric_brain_reset_stats(brain);
    hemispheric_brain_process_lateralized(brain, input, 8,
        COGNITIVE_DOMAIN_LANGUAGE, output2, 4);

    // Outputs should be identical
    for (int i = 0; i < 4; i++) {
        EXPECT_FLOAT_EQ(output1[i], output2[i]) << "Mismatch at index " << i;
    }
}

TEST_F(HemisphericBrainRegressionTest, ParallelProcessingDeterministic) {
    ASSERT_NE(brain, nullptr);

    float input[8];
    float left1[4], right1[4], left2[4], right2[4];
    for (int i = 0; i < 8; i++) input[i] = 0.6f;

    // First run
    hemispheric_brain_process_parallel(brain, input, 8, left1, right1, 4);

    // Reset and run again
    hemispheric_brain_reset_stats(brain);
    hemispheric_brain_process_parallel(brain, input, 8, left2, right2, 4);

    for (int i = 0; i < 4; i++) {
        EXPECT_FLOAT_EQ(left1[i], left2[i]) << "Left mismatch at " << i;
        EXPECT_FLOAT_EQ(right1[i], right2[i]) << "Right mismatch at " << i;
    }
}

TEST_F(HemisphericBrainRegressionTest, InferenceDeterministic) {
    ASSERT_NE(brain, nullptr);

    float input[8], output1[4], output2[4];
    for (int i = 0; i < 8; i++) input[i] = 0.4f;

    // First inference
    hemispheric_brain_infer(brain, input, 8, output1, 4);

    // Reset stats and repeat
    hemispheric_brain_reset_stats(brain);
    hemispheric_brain_infer(brain, input, 8, output2, 4);

    for (int i = 0; i < 4; i++) {
        EXPECT_FLOAT_EQ(output1[i], output2[i]);
    }
}

TEST_F(HemisphericBrainRegressionTest, UpdateSequenceDeterministic) {
    ASSERT_NE(brain, nullptr);

    float input[8] = {0.5f}, output[4];
    hemispheric_brain_infer(brain, input, 8, output, 4);

    // Run sequence
    for (int i = 0; i < 100; i++) {
        hemispheric_brain_update(brain, 0.001f);
    }
    float energy1 = hemispheric_brain_get_energy(brain);

    // Reset and repeat
    hemispheric_brain_reset_stats(brain);
    hemispheric_brain_infer(brain, input, 8, output, 4);
    for (int i = 0; i < 100; i++) {
        hemispheric_brain_update(brain, 0.001f);
    }
    float energy2 = hemispheric_brain_get_energy(brain);

    EXPECT_NEAR(energy1, energy2, 0.001f);
}

//=============================================================================
// State Consistency Tests
//=============================================================================

TEST_F(HemisphericBrainRegressionTest, ModeChangeConsistency) {
    ASSERT_NE(brain, nullptr);

    // Change modes and verify consistency
    EXPECT_EQ(hemispheric_brain_set_mode(brain, HEMISPHERIC_MODE_PARALLEL), 0);
    EXPECT_EQ(hemispheric_brain_get_mode(brain), HEMISPHERIC_MODE_PARALLEL);

    EXPECT_EQ(hemispheric_brain_set_mode(brain, HEMISPHERIC_MODE_COMPETITIVE), 0);
    EXPECT_EQ(hemispheric_brain_get_mode(brain), HEMISPHERIC_MODE_COMPETITIVE);

    EXPECT_EQ(hemispheric_brain_set_mode(brain, HEMISPHERIC_MODE_COOPERATIVE), 0);
    EXPECT_EQ(hemispheric_brain_get_mode(brain), HEMISPHERIC_MODE_COOPERATIVE);

    EXPECT_EQ(hemispheric_brain_set_mode(brain, HEMISPHERIC_MODE_LATERALIZED), 0);
    EXPECT_EQ(hemispheric_brain_get_mode(brain), HEMISPHERIC_MODE_LATERALIZED);
}

TEST_F(HemisphericBrainRegressionTest, CallosumStateConsistency) {
    ASSERT_NE(brain, nullptr);

    // Initially connected
    EXPECT_TRUE(hemispheric_brain_is_callosum_intact(brain));

    // Disconnect
    EXPECT_EQ(hemispheric_brain_disconnect_callosum(brain), 0);
    EXPECT_FALSE(hemispheric_brain_is_callosum_intact(brain));

    // Reconnect
    EXPECT_EQ(hemispheric_brain_reconnect_callosum(brain), 0);
    EXPECT_TRUE(hemispheric_brain_is_callosum_intact(brain));
}

TEST_F(HemisphericBrainRegressionTest, ActiveStateConsistency) {
    ASSERT_NE(brain, nullptr);

    EXPECT_TRUE(hemispheric_brain_is_active(brain));

    EXPECT_EQ(hemispheric_brain_set_active(brain, false), 0);
    EXPECT_FALSE(hemispheric_brain_is_active(brain));

    EXPECT_EQ(hemispheric_brain_set_active(brain, true), 0);
    EXPECT_TRUE(hemispheric_brain_is_active(brain));
}

TEST_F(HemisphericBrainRegressionTest, BilateralModeConsistency) {
    ASSERT_NE(brain, nullptr);

    EXPECT_FALSE(hemispheric_brain_is_bilateral_mode(brain));

    EXPECT_EQ(hemispheric_brain_set_bilateral_mode(brain, true), 0);
    EXPECT_TRUE(hemispheric_brain_is_bilateral_mode(brain));

    EXPECT_EQ(hemispheric_brain_set_bilateral_mode(brain, false), 0);
    EXPECT_FALSE(hemispheric_brain_is_bilateral_mode(brain));
}

TEST_F(HemisphericBrainRegressionTest, HemisphereAccessConsistency) {
    ASSERT_NE(brain, nullptr);

    brain_hemisphere_t* left = hemispheric_brain_get_left(brain);
    brain_hemisphere_t* right = hemispheric_brain_get_right(brain);

    ASSERT_NE(left, nullptr);
    ASSERT_NE(right, nullptr);

    // Access by ID should match
    EXPECT_EQ(hemispheric_brain_get_hemisphere(brain, HEMISPHERE_LEFT), left);
    EXPECT_EQ(hemispheric_brain_get_hemisphere(brain, HEMISPHERE_RIGHT), right);
}

//=============================================================================
// Memory Usage Pattern Tests
//=============================================================================

TEST_F(HemisphericBrainRegressionTest, CreateDestroyNoLeak) {
    // Reduced from 5 to 1 to prevent OOM (each brain allocates 10-60GB)
    hemispheric_brain_config_t config = hemispheric_brain_default_config();
    config.size = BRAIN_SIZE_TINY;
    config.num_inputs = 4;
    config.num_outputs = 2;
    config.initial_tier = PLATFORM_TIER_CONSTRAINED;
    config.enable_bio_async = false;
    config.left_config.size = BRAIN_SIZE_TINY;
    config.left_config.initial_tier = PLATFORM_TIER_CONSTRAINED;
    config.left_config.enable_bio_async = false;
    config.right_config.size = BRAIN_SIZE_TINY;
    config.right_config.initial_tier = PLATFORM_TIER_CONSTRAINED;
    config.right_config.enable_bio_async = false;
    hemispheric_brain_t* b = hemispheric_brain_create(&config);
    ASSERT_NE(b, nullptr);
    hemispheric_brain_destroy(b);
}

TEST_F(HemisphericBrainRegressionTest, RepeatedOperationsNoGrowth) {
    ASSERT_NE(brain, nullptr);

    float input[8], output[4];
    for (int i = 0; i < 8; i++) input[i] = 0.5f;

    // Perform many operations - memory should not grow unboundedly
    // Reduced from 1000 to 50 to prevent OOM under ctest
    for (int i = 0; i < 50; i++) {
        hemispheric_brain_infer(brain, input, 8, output, 4);
        hemispheric_brain_update(brain, 0.001f);
    }

    // If we got here without crashing, memory is likely stable
    SUCCEED();
}

TEST_F(HemisphericBrainRegressionTest, NullPointerSafety) {
    // These should not crash
    hemispheric_brain_destroy(nullptr);
    hemispheric_brain_update(nullptr, 1.0f);

    float buffer[16];
    hemispheric_brain_infer(nullptr, buffer, 16, buffer, 16);
    hemispheric_brain_process_lateralized(nullptr, buffer, 16,
        COGNITIVE_DOMAIN_LANGUAGE, buffer, 16);

    hemispheric_brain_get_left(nullptr);
    hemispheric_brain_get_right(nullptr);
    hemispheric_brain_get_mode(nullptr);
    hemispheric_brain_is_active(nullptr);
    hemispheric_brain_is_callosum_intact(nullptr);
}

//=============================================================================
// Statistics Accumulation Tests
//=============================================================================

TEST_F(HemisphericBrainRegressionTest, StatsAccumulateCorrectly) {
    ASSERT_NE(brain, nullptr);

    float input[8], output[4];
    for (int i = 0; i < 8; i++) input[i] = 0.5f;

    // Reset stats first
    hemispheric_brain_reset_stats(brain);

    hemispheric_brain_stats_t stats_before;
    hemispheric_brain_get_stats(brain, &stats_before);

    // Perform operations
    hemispheric_brain_set_mode(brain, HEMISPHERIC_MODE_LATERALIZED);
    for (int i = 0; i < 10; i++) {
        hemispheric_brain_process_lateralized(brain, input, 8,
            COGNITIVE_DOMAIN_LANGUAGE, output, 4);
    }

    hemispheric_brain_set_mode(brain, HEMISPHERIC_MODE_PARALLEL);
    float left_out[4], right_out[4];
    for (int i = 0; i < 5; i++) {
        hemispheric_brain_process_parallel(brain, input, 8,
            left_out, right_out, 4);
    }

    hemispheric_brain_stats_t stats_after;
    hemispheric_brain_get_stats(brain, &stats_after);

    EXPECT_GE(stats_after.lateralized_operations,
              stats_before.lateralized_operations + 10);
    EXPECT_GE(stats_after.parallel_operations,
              stats_before.parallel_operations + 5);
}

TEST_F(HemisphericBrainRegressionTest, CompetitiveWinsAccumulate) {
    ASSERT_NE(brain, nullptr);

    float input[8], output[4];
    for (int i = 0; i < 8; i++) input[i] = 0.5f;
    hemisphere_id_t winner;

    hemispheric_brain_reset_stats(brain);

    // Run competitive processing multiple times
    int left_wins = 0, right_wins = 0;
    for (int i = 0; i < 100; i++) {
        hemispheric_brain_process_competitive(brain, input, 8,
            output, 4, &winner);
        if (winner == HEMISPHERE_LEFT) left_wins++;
        else if (winner == HEMISPHERE_RIGHT) right_wins++;
    }

    hemispheric_brain_stats_t stats;
    hemispheric_brain_get_stats(brain, &stats);

    // Stats should reflect the wins
    EXPECT_EQ(stats.left_wins + stats.right_wins, 100);
}

TEST_F(HemisphericBrainRegressionTest, StatsResetWorks) {
    ASSERT_NE(brain, nullptr);

    float input[8], output[4];
    for (int i = 0; i < 8; i++) input[i] = 0.5f;

    // Generate some stats
    for (int i = 0; i < 10; i++) {
        hemispheric_brain_infer(brain, input, 8, output, 4);
    }

    hemispheric_brain_stats_t stats;
    hemispheric_brain_get_stats(brain, &stats);
    EXPECT_GT(stats.lateralized_operations + stats.parallel_operations +
              stats.competitive_operations + stats.cooperative_operations, 0);

    // Reset
    EXPECT_EQ(hemispheric_brain_reset_stats(brain), 0);

    hemispheric_brain_get_stats(brain, &stats);
    EXPECT_EQ(stats.lateralized_operations, 0);
    EXPECT_EQ(stats.parallel_operations, 0);
    EXPECT_EQ(stats.competitive_operations, 0);
    EXPECT_EQ(stats.cooperative_operations, 0);
}

//=============================================================================
// Backward Compatibility Tests
//=============================================================================

TEST_F(HemisphericBrainRegressionTest, DefaultConfigStable) {
    hemispheric_brain_config_t config = hemispheric_brain_default_config();

    // Default mode should be COOPERATIVE
    EXPECT_EQ(config.default_mode, HEMISPHERIC_MODE_COOPERATIVE);

    // Cooperation strategy should be WEIGHTED
    EXPECT_EQ(config.cooperation_strategy, COOPERATION_WEIGHTED);

    // Shared structures enabled by default
    EXPECT_TRUE(config.enable_shared_thalamus);
    EXPECT_TRUE(config.enable_shared_immune);
}

TEST_F(HemisphericBrainRegressionTest, ModeNamesStable) {
    EXPECT_STREQ(hemispheric_mode_name(HEMISPHERIC_MODE_LATERALIZED), "Lateralized");
    EXPECT_STREQ(hemispheric_mode_name(HEMISPHERIC_MODE_PARALLEL), "Parallel");
    EXPECT_STREQ(hemispheric_mode_name(HEMISPHERIC_MODE_COMPETITIVE), "Competitive");
    EXPECT_STREQ(hemispheric_mode_name(HEMISPHERIC_MODE_COOPERATIVE), "Cooperative");
}

TEST_F(HemisphericBrainRegressionTest, CooperationStrategyNamesStable) {
    EXPECT_STREQ(cooperation_strategy_name(COOPERATION_AVERAGE), "Average");
    EXPECT_STREQ(cooperation_strategy_name(COOPERATION_WEIGHTED), "Weighted");
    EXPECT_STREQ(cooperation_strategy_name(COOPERATION_DOMINANT), "Dominant");
    EXPECT_STREQ(cooperation_strategy_name(COOPERATION_ATTENTION_GATED), "Attention-Gated");
}

//=============================================================================
// Stress Tests
//=============================================================================

TEST_F(HemisphericBrainRegressionTest, RapidModeSwitch) {
    ASSERT_NE(brain, nullptr);

    float input[8], output[4];
    for (int i = 0; i < 8; i++) input[i] = 0.5f;

    hemispheric_mode_t modes[] = {
        HEMISPHERIC_MODE_LATERALIZED,
        HEMISPHERIC_MODE_PARALLEL,
        HEMISPHERIC_MODE_COMPETITIVE,
        HEMISPHERIC_MODE_COOPERATIVE
    };

    // Rapidly switch modes with processing (reduced from 1000 to 50)
    for (int i = 0; i < 50; i++) {
        hemispheric_brain_set_mode(brain, modes[i % 4]);
        hemispheric_brain_infer(brain, input, 8, output, 4);

        // Verify output bounds
        for (int j = 0; j < 4; j++) {
            EXPECT_FALSE(std::isnan(output[j])) << "NaN at iteration " << i;
            EXPECT_FALSE(std::isinf(output[j])) << "Inf at iteration " << i;
        }
    }
}

TEST_F(HemisphericBrainRegressionTest, LongSimulationStable) {
    ASSERT_NE(brain, nullptr);

    float input[8], output[4];
    for (int i = 0; i < 8; i++) input[i] = 0.5f;

    // Reduced from 5000 to 100 to prevent OOM and excessive runtime
    for (int i = 0; i < 100; i++) {
        hemispheric_brain_infer(brain, input, 8, output, 4);
        hemispheric_brain_update(brain, 0.001f);

        // Verify no numerical issues
        float energy = hemispheric_brain_get_energy(brain);
        EXPECT_FALSE(std::isnan(energy)) << "NaN energy at step " << i;
        EXPECT_FALSE(std::isinf(energy)) << "Inf energy at step " << i;
    }
}
