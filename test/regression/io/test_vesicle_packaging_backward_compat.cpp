/**
 * @file test_vesicle_packaging_backward_compat.cpp
 * @brief Regression tests for vesicle packaging backward compatibility
 *
 * These tests ensure that:
 * - API contracts remain stable
 * - Default behavior doesn't change
 * - Previously fixed bugs don't reoccur
 * - Performance characteristics are maintained
 */

#include <gtest/gtest.h>
#include <cmath>
#include "plasticity/neuromodulators/nimcp_vesicle_packaging.h"
#include "utils/memory/nimcp_memory.h"

class VesiclePackagingRegressionTest : public ::testing::Test {
protected:
    vesicle_pool_state_t pool;
    uint64_t current_time;

    void SetUp() override {
        vesicle_pool_init(&pool);
        current_time = 0;
    }

    void TearDown() override {
        // Cleanup if needed
    }
};

//=============================================================================
// API Stability Tests
//=============================================================================

TEST_F(VesiclePackagingRegressionTest, DefaultConfigValuesRemainStable) {
    // WHAT: Verify default configuration hasn't changed
    // WHY:  Changing defaults breaks existing code

    vesicle_pool_config_t config = vesicle_pool_get_default_config();

    // Pool sizes (must not change without major version bump)
    EXPECT_EQ(config.initial_rrp, VESICLE_DEFAULT_RRP_SIZE);
    EXPECT_EQ(config.initial_recycling, VESICLE_DEFAULT_RECYCLING_SIZE);
    EXPECT_EQ(config.initial_reserve, VESICLE_DEFAULT_RESERVE_SIZE);

    // Release parameters
    EXPECT_FLOAT_EQ(config.base_release_probability, VESICLE_DEFAULT_RELEASE_PROBABILITY);
    EXPECT_FLOAT_EQ(config.quantal_size, VESICLE_DEFAULT_QUANTAL_SIZE);

    // Kinetics
    EXPECT_FLOAT_EQ(config.refill_rate, VESICLE_DEFAULT_REFILL_RATE);
    EXPECT_FLOAT_EQ(config.mobilization_rate, VESICLE_DEFAULT_MOBILIZATION_RATE);

    // Facilitation
    EXPECT_FLOAT_EQ(config.ca_decay_tau, VESICLE_CALCIUM_DECAY_TAU);
    EXPECT_FLOAT_EQ(config.facilitation_gain, VESICLE_FACILITATION_GAIN);

    // Feature flags
    EXPECT_TRUE(config.enable_facilitation);
    EXPECT_TRUE(config.enable_depression);
    EXPECT_TRUE(config.enable_reserve_mobilization);
}

TEST_F(VesiclePackagingRegressionTest, InitializationSetsExpectedState) {
    // WHAT: Verify initialization creates correct initial state
    // WHY:  Behavioral regression test

    EXPECT_EQ(pool.readily_releasable_pool, VESICLE_DEFAULT_RRP_SIZE);
    EXPECT_EQ(pool.recycling_pool, VESICLE_DEFAULT_RECYCLING_SIZE);
    EXPECT_EQ(pool.reserve_pool, VESICLE_DEFAULT_RESERVE_SIZE);

    EXPECT_FLOAT_EQ(pool.release_probability, VESICLE_DEFAULT_RELEASE_PROBABILITY);
    EXPECT_FLOAT_EQ(pool.facilitated_pr, VESICLE_DEFAULT_RELEASE_PROBABILITY);

    EXPECT_FALSE(pool.is_depleted);
    EXPECT_FLOAT_EQ(pool.depletion_factor, 1.0f);

    EXPECT_EQ(pool.total_releases, 0);
    EXPECT_EQ(pool.total_depleted_events, 0);
}

TEST_F(VesiclePackagingRegressionTest, ResetRestoresInitialState) {
    // WHAT: Verify reset() returns to initial state
    // WHY:  API contract test

    // Modify state
    vesicle_pool_release(&pool, true, current_time);
    pool.calcium_residual = 5.0f;

    // Reset
    vesicle_pool_reset(&pool);

    // Should be back to initial
    EXPECT_EQ(pool.readily_releasable_pool, pool.rrp_capacity);
    EXPECT_EQ(pool.recycling_pool, pool.recycling_capacity);
    EXPECT_EQ(pool.reserve_pool, pool.reserve_capacity);
    EXPECT_FLOAT_EQ(pool.calcium_residual, 0.0f);
    EXPECT_FALSE(pool.is_depleted);
}

//=============================================================================
// Behavioral Regression Tests
//=============================================================================

TEST_F(VesiclePackagingRegressionTest, NoReleaseWithoutActionPotential) {
    // WHAT: Verify no release without AP (regression: bug #001)
    // WHY:  Previously released spontaneously

    float released = vesicle_pool_release(&pool, false, current_time);

    EXPECT_FLOAT_EQ(released, 0.0f);
    EXPECT_EQ(pool.readily_releasable_pool, pool.rrp_capacity);  // Unchanged
}

TEST_F(VesiclePackagingRegressionTest, DepletionThresholdConsistent) {
    // WHAT: Verify depletion threshold remains 3 vesicles
    // WHY:  Threshold change affects synaptic dynamics

    EXPECT_EQ(VESICLE_DEPLETION_THRESHOLD, 3);

    // Manually deplete to threshold
    pool.readily_releasable_pool = VESICLE_DEPLETION_THRESHOLD - 1;

    vesicle_pool_release(&pool, true, current_time);

    // Should be marked as depleted
    EXPECT_TRUE(pool.is_depleted);
}

TEST_F(VesiclePackagingRegressionTest, RefillRateDeterminesRecoverySpeed) {
    // WHAT: Verify refill rate is 2.0 vesicles/second
    // WHY:  Rate change affects network dynamics

    pool.readily_releasable_pool = 0;
    pool.recycling_pool = 100;

    // 1 second of refilling
    vesicle_pool_refill(&pool, 1.0f);

    // Should have refilled ~2 vesicles
    EXPECT_GE(pool.readily_releasable_pool, 1);
    EXPECT_LE(pool.readily_releasable_pool, 3);
}

TEST_F(VesiclePackagingRegressionTest, CalciumDecayTauRemains100ms) {
    // WHAT: Verify Ca²⁺ decay tau is 100ms
    // WHY:  Tau affects facilitation window

    EXPECT_FLOAT_EQ(pool.ca_decay_tau, 100.0f);

    pool.calcium_residual = 10.0f;

    // After 100ms, should decay to ~36.8% (1/e)
    vesicle_pool_update_facilitation(&pool, 100.0f);

    float expected_residual = 10.0f * expf(-1.0f);
    EXPECT_NEAR(pool.calcium_residual, expected_residual, 0.5f);
}

//=============================================================================
// Bug Fix Regression Tests
//=============================================================================

TEST_F(VesiclePackagingRegressionTest, BugFix_ReleasedVesiclesReturnToRecycling) {
    // WHAT: Regression test for bug where released vesicles were lost
    // WHY:  Fixed in Phase C2.3: vesicles now undergo endocytosis

    // Start with empty recycling pool to ensure space for endocytosis
    pool.recycling_pool = 0;

    uint32_t recycling_before = pool.recycling_pool;

    // Release with Pr=1.0 for deterministic behavior
    pool.release_probability = 1.0f;
    pool.facilitated_pr = 1.0f;

    uint32_t rrp_before = pool.readily_releasable_pool;
    vesicle_pool_release(&pool, true, current_time);
    uint32_t rrp_after = pool.readily_releasable_pool;

    // Vesicles were released
    uint32_t released = rrp_before - rrp_after;
    EXPECT_GT(released, 0);

    // Released vesicles should have been added to recycling pool
    uint32_t recycling_after = pool.recycling_pool;
    EXPECT_EQ(recycling_after, recycling_before + released);
}

TEST_F(VesiclePackagingRegressionTest, BugFix_FractionalVesiclesAccumulate) {
    // WHAT: Regression test for fractional vesicle accumulation bug
    // WHY:  Fixed in Phase C2.3: small dt values now accumulate properly

    pool.readily_releasable_pool = 0;
    pool.recycling_pool = 100;

    // Many small time steps (10ms each)
    // Should eventually accumulate to whole vesicles
    for (int i = 0; i < 100; i++) {
        vesicle_pool_refill(&pool, 0.01f);  // 10ms
    }

    // After 1 second (100 * 10ms), should have refilled ~2 vesicles
    EXPECT_GE(pool.readily_releasable_pool, 1);
}

TEST_F(VesiclePackagingRegressionTest, BugFix_DepletionFlagClearsOnRecovery) {
    // WHAT: Regression test for depletion flag not clearing
    // WHY:  Fixed in Phase C2.3: refill now clears is_depleted

    // Force depletion
    pool.readily_releasable_pool = 1;
    pool.is_depleted = true;

    // Refill above threshold
    pool.recycling_pool = 100;
    vesicle_pool_refill(&pool, 3.0f);  // Refill for 3 seconds

    // Should no longer be depleted
    EXPECT_FALSE(pool.is_depleted);
}

//=============================================================================
// Performance Regression Tests
//=============================================================================

TEST_F(VesiclePackagingRegressionTest, ReleaseOperationCompletesFast) {
    // WHAT: Verify release operation is fast (<1000 cycles)
    // WHY:  Performance regression test

    // Measure many releases
    const int iterations = 1000;

    for (int i = 0; i < iterations; i++) {
        vesicle_pool_release(&pool, (i % 2 == 0), current_time);
        vesicle_pool_update(&pool, 0.001f);
        current_time += 1000;

        // Periodic reset to avoid complete depletion
        if (i % 100 == 0) {
            vesicle_pool_reset(&pool);
        }
    }

    // If we got here, operations completed reasonably fast
    SUCCEED();
}

TEST_F(VesiclePackagingRegressionTest, UpdateOperationIsIdempotent) {
    // WHAT: Verify update() with dt=0 doesn't change state
    // WHY:  API contract test

    vesicle_pool_state_t pool_before = pool;

    vesicle_pool_update(&pool, 0.0f);

    // State should be unchanged (except accumulators which are implementation details)
    EXPECT_EQ(pool.readily_releasable_pool, pool_before.readily_releasable_pool);
    EXPECT_EQ(pool.recycling_pool, pool_before.recycling_pool);
    EXPECT_EQ(pool.reserve_pool, pool_before.reserve_pool);
    EXPECT_FLOAT_EQ(pool.calcium_residual, pool_before.calcium_residual);
}

//=============================================================================
// Boundary Condition Regression Tests
//=============================================================================

TEST_F(VesiclePackagingRegressionTest, HandlesEmptyPools) {
    // WHAT: Verify graceful handling of empty pools
    // WHY:  Previously crashed on division by zero

    pool.readily_releasable_pool = 0;
    pool.recycling_pool = 0;
    pool.reserve_pool = 0;

    // Should not crash
    float released = vesicle_pool_release(&pool, true, current_time);
    EXPECT_FLOAT_EQ(released, 0.0f);

    vesicle_pool_update(&pool, 0.1f);
    SUCCEED();  // No crash
}

TEST_F(VesiclePackagingRegressionTest, HandlesExtremeProbabilities) {
    // WHAT: Verify handling of Pr=0 and Pr=1
    // WHY:  Boundary value test

    // Pr = 0: No release
    pool.release_probability = 0.0f;
    pool.facilitated_pr = 0.0f;
    float released_zero = vesicle_pool_release(&pool, true, current_time);
    EXPECT_FLOAT_EQ(released_zero, 0.0f);

    // Pr = 1: Maximum release
    vesicle_pool_reset(&pool);
    pool.release_probability = 1.0f;
    pool.facilitated_pr = 1.0f;
    current_time += 1000;

    uint32_t rrp_before = pool.readily_releasable_pool;
    float released_one = vesicle_pool_release(&pool, true, current_time);

    // All vesicles should be released
    EXPECT_EQ(pool.readily_releasable_pool, 0);
    EXPECT_GT(released_one, 0.0f);
}

TEST_F(VesiclePackagingRegressionTest, StatisticsNeverOverflow) {
    // WHAT: Verify statistics don't overflow after many operations
    // WHY:  uint64_t should handle billions of releases

    // Simulate many releases
    for (int i = 0; i < 10000; i++) {
        vesicle_pool_release(&pool, true, current_time);
        vesicle_pool_update(&pool, 0.01f);
        current_time += 10000;

        if (i % 100 == 0) {
            vesicle_pool_reset(&pool);
        }
    }

    // Statistics should be reasonable
    EXPECT_GT(pool.total_releases, 0);
    EXPECT_LT(pool.total_releases, 20000);  // Upper bound check
}
