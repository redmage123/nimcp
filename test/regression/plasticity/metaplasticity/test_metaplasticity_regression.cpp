/**
 * @file test_metaplasticity_regression.cpp
 * @brief Regression Tests for Extended Metaplasticity
 * @version 1.0.0
 * @date 2025-12-19
 */

#include <gtest/gtest.h>
#include "plasticity/metaplasticity/nimcp_extended_metaplasticity.h"
#include <cmath>

class MetaplasticityRegressionTest : public ::testing::Test {
protected:
    extended_metaplasticity_config_t config;
    extended_metaplasticity_state_t* state;

    void SetUp() override {
        config = metaplasticity_config_default();
        /* Use faster timescales for regression testing */
        config.baseline_tau_ms = 1000.0f;   /* 1 second - fast convergence for testing */
        config.activity_tau_ms = 100.0f;    /* 100 ms */
        state = nullptr;
    }

    void TearDown() override {
        if (state) {
            metaplasticity_state_destroy(state);
        }
    }
};

/* ============================================================================
 * Threshold Stability Tests
 * ============================================================================ */

TEST_F(MetaplasticityRegressionTest, ThresholdConvergesToActivitySquared) {
    state = metaplasticity_state_create(&config);
    ASSERT_NE(state, nullptr);

    float constant_activity = 2.0f;
    float dt = 100.0f;

    // Run for many iterations
    for (int i = 0; i < 1000; i++) {
        metaplasticity_update_baseline(state, constant_activity, dt, &config);
    }

    float expected = constant_activity * constant_activity;
    EXPECT_NEAR(state->theta_baseline, expected, 0.5f);
}

TEST_F(MetaplasticityRegressionTest, ThresholdStableUnderZeroActivity) {
    state = metaplasticity_state_create(&config);
    ASSERT_NE(state, nullptr);

    float dt = 100.0f;

    // Zero activity for many iterations
    for (int i = 0; i < 500; i++) {
        metaplasticity_update_baseline(state, 0.0f, dt, &config);
    }

    // Should converge to near-zero
    EXPECT_LT(state->theta_baseline, 0.5f);
    EXPECT_GE(state->theta_baseline, config.min_theta);
}

TEST_F(MetaplasticityRegressionTest, ThresholdDoesNotOscillate) {
    state = metaplasticity_state_create(&config);
    ASSERT_NE(state, nullptr);

    float activity = 2.0f;
    float dt = 100.0f;

    // Measure variance over time
    float values[100];
    for (int i = 0; i < 100; i++) {
        metaplasticity_update_baseline(state, activity, dt, &config);
        values[i] = state->theta_baseline;
    }

    // Check that values are monotonically converging (no oscillation)
    for (int i = 50; i < 99; i++) {
        float diff = fabs(values[i+1] - values[i]);
        EXPECT_LT(diff, 0.1f);  // Small changes near convergence
    }
}

/* ============================================================================
 * Neuromodulator Consistency Tests
 * ============================================================================ */

TEST_F(MetaplasticityRegressionTest, DopamineEffectConsistent) {
    state = metaplasticity_state_create(&config);
    ASSERT_NE(state, nullptr);

    neuromodulator_levels_t neuromod = {0};
    neuromod.dopamine = 0.75f;

    // Apply multiple times
    float shifts[10];
    for (int i = 0; i < 10; i++) {
        metaplasticity_apply_neuromodulator_shifts(state, &neuromod, &config);
        shifts[i] = state->da_shift;
    }

    // Should be consistent
    for (int i = 1; i < 10; i++) {
        EXPECT_FLOAT_EQ(shifts[i], shifts[0]);
    }
}

TEST_F(MetaplasticityRegressionTest, CombinedNeuromodulatorsAdditive) {
    state = metaplasticity_state_create(&config);
    ASSERT_NE(state, nullptr);

    neuromodulator_levels_t da_only = {0};
    da_only.dopamine = 1.0f;

    neuromodulator_levels_t ne_only = {0};
    ne_only.norepinephrine = 1.0f;

    neuromodulator_levels_t both = {0};
    both.dopamine = 1.0f;
    both.norepinephrine = 1.0f;

    metaplasticity_apply_neuromodulator_shifts(state, &da_only, &config);
    float da_shift = state->da_shift;

    metaplasticity_apply_neuromodulator_shifts(state, &ne_only, &config);
    float ne_shift = state->ne_shift;

    metaplasticity_apply_neuromodulator_shifts(state, &both, &config);
    // Both should be present
    EXPECT_GT(state->da_shift, 0.0f);
    EXPECT_GT(state->ne_shift, 0.0f);
}

/* ============================================================================
 * Sleep Reset Stability Tests
 * ============================================================================ */

TEST_F(MetaplasticityRegressionTest, RepeatedSleepResetsConvergeToBaseline) {
    state = metaplasticity_state_create(&config);
    ASSERT_NE(state, nullptr);

    // Elevate threshold
    state->theta_effective = 10.0f;
    state->theta_baseline = 1.0f;

    // Apply deep NREM reset multiple times
    for (int i = 0; i < 20; i++) {
        metaplasticity_apply_sleep_reset(state, SLEEP_STATE_DEEP_NREM, &config);
    }

    // Should be close to baseline
    EXPECT_NEAR(state->theta_effective, state->theta_baseline, 0.5f);
}

TEST_F(MetaplasticityRegressionTest, SleepResetDoesNotGoNegative) {
    state = metaplasticity_state_create(&config);
    ASSERT_NE(state, nullptr);

    state->theta_effective = 0.5f;
    state->theta_baseline = 1.0f;

    // Reset should not make threshold negative
    metaplasticity_apply_sleep_reset(state, SLEEP_STATE_DEEP_NREM, &config);

    EXPECT_GE(state->theta_effective, config.min_theta);
}

TEST_F(MetaplasticityRegressionTest, SleepResetRespectsBounds) {
    state = metaplasticity_state_create(&config);
    ASSERT_NE(state, nullptr);

    state->theta_effective = 100.0f;
    state->theta_baseline = 1.0f;

    for (int i = 0; i < 50; i++) {
        metaplasticity_apply_sleep_reset(state, SLEEP_STATE_DEEP_NREM, &config);
    }

    EXPECT_LE(state->theta_effective, config.max_theta);
    EXPECT_GE(state->theta_effective, config.min_theta);
}

/* ============================================================================
 * History Buffer Stability Tests
 * ============================================================================ */

TEST_F(MetaplasticityRegressionTest, HistoryBufferNoMemoryLeak) {
    config.history_size = 100;
    state = metaplasticity_state_create(&config);
    ASSERT_NE(state, nullptr);

    // Add many entries (more than buffer size)
    for (uint32_t i = 0; i < 1000; i++) {
        metaplasticity_update_history(state, (float)i, i * 1000, &config);
    }

    // Count should not exceed buffer size
    EXPECT_EQ(state->history_count, config.history_size);
}

TEST_F(MetaplasticityRegressionTest, HistoryBufferCircularOverwrite) {
    config.history_size = 4;
    state = metaplasticity_state_create(&config);
    ASSERT_NE(state, nullptr);

    // Fill buffer
    for (uint32_t i = 0; i < 4; i++) {
        metaplasticity_update_history(state, (float)i, i * 1000, &config);
    }

    // Add one more (should overwrite oldest)
    metaplasticity_update_history(state, 99.0f, 5000, &config);

    // Check that newest entry is present
    bool found_newest = false;
    for (uint32_t i = 0; i < state->history_count; i++) {
        if (fabs(state->history[i].activity_squared - 99.0f * 99.0f) < 0.01f) {
            found_newest = true;
            break;
        }
    }
    EXPECT_TRUE(found_newest);
}

/* ============================================================================
 * Long-term Stability Tests
 * ============================================================================ */

TEST_F(MetaplasticityRegressionTest, ExtendedRunNoNaN) {
    state = metaplasticity_state_create(&config);
    ASSERT_NE(state, nullptr);

    neuromodulator_levels_t neuromod = {0};
    neuromod.dopamine = 0.5f;

    // Run for very long simulation
    for (int i = 0; i < 10000; i++) {
        float activity = sinf((float)i * 0.01f) + 1.5f;  // Varying activity
        metaplasticity_update(state, activity, &neuromod, 100.0f, &config);

        // Check for NaN
        ASSERT_FALSE(std::isnan(state->theta_baseline));
        ASSERT_FALSE(std::isnan(state->theta_effective));
        ASSERT_FALSE(std::isinf(state->theta_baseline));
        ASSERT_FALSE(std::isinf(state->theta_effective));
    }
}

TEST_F(MetaplasticityRegressionTest, ExtendedRunBoundsRespected) {
    state = metaplasticity_state_create(&config);
    ASSERT_NE(state, nullptr);

    // Run with extreme activities
    for (int i = 0; i < 5000; i++) {
        float activity = (i % 2 == 0) ? 100.0f : 0.1f;  // Alternating extremes
        metaplasticity_update_baseline(state, activity, 100.0f, &config);

        EXPECT_LE(state->theta_baseline, config.max_theta);
        EXPECT_GE(state->theta_baseline, config.min_theta);
    }
}

/* ============================================================================
 * Controller Regression Tests
 * ============================================================================ */

TEST_F(MetaplasticityRegressionTest, ControllerConsistentAcrossSynapses) {
    metaplasticity_controller_t controller = metaplasticity_controller_create(&config, 100);
    ASSERT_NE(controller, nullptr);

    float activities[100];
    for (int i = 0; i < 100; i++) {
        activities[i] = 2.0f;  // Same activity for all
    }

    neuromodulator_levels_t neuromod = {0};

    // Update many times
    for (int iter = 0; iter < 500; iter++) {
        metaplasticity_controller_update_all(controller, activities, &neuromod, 100.0f);
    }

    // All thresholds should converge to similar values
    metaplasticity_stats_t stats;
    metaplasticity_controller_get_stats(controller, &stats);

    EXPECT_LT(stats.theta_variance, 1.0f);  // Low variance

    metaplasticity_controller_destroy(controller);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
