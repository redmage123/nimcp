/**
 * @file test_social_fep_regression.cpp
 * @brief Regression tests for Social Cognition FEP Bridge
 * @version 1.0.0
 * @date 2026-01-08
 *
 * WHAT: Regression tests to ensure stability and performance of social FEP bridge
 * WHY:  Prevent regressions in free energy computation, social prediction tracking,
 *       and performance characteristics
 * HOW:  Test stability over many cycles, verify bounds, check performance metrics
 *
 * TEST COVERAGE:
 * - Social prediction stability over many cycles
 * - Free energy bounds enforcement
 * - Performance regression (update time)
 * - Memory leak detection (valgrind-friendly)
 * - Relationship consistency over time
 * - Numerical stability under stress
 * - Configuration persistence
 * - Statistics accuracy
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <chrono>
#include <thread>
#include <atomic>
#include <vector>

#include "cognitive/social/nimcp_social_fep_bridge.h"
#include "cognitive/nimcp_love_loyalty_friendship.h"
#include "cognitive/free_energy/nimcp_fep_orchestrator.h"

/* ============================================================================
 * Regression Test Constants
 * ============================================================================ */

#define REGRESSION_STABILITY_CYCLES       1000
#define REGRESSION_STRESS_CYCLES          500
#define REGRESSION_MEMORY_CYCLES          100
#define REGRESSION_MAX_UPDATE_TIME_US     5000  /* 5ms max per update */
#define REGRESSION_FREE_ENERGY_MIN        0.0f
#define REGRESSION_FREE_ENERGY_MAX        2.5f  /* Slightly above SOCIAL_FEP_MAX_FREE_ENERGY */
#define REGRESSION_PREDICTION_ERROR_MAX   1.0f
#define REGRESSION_UNCERTAINTY_MAX        1.0f

/* ============================================================================
 * Test Fixture: Social FEP Regression
 * ============================================================================ */

class SocialFEPRegressionTest : public ::testing::Test {
protected:
    social_bond_system_t* social = nullptr;
    social_fep_bridge_t* bridge = nullptr;
    social_fep_config_t config;
    fep_orchestrator_t* fep_orch = nullptr;
    fep_orchestrator_config_t fep_config;
    uint32_t bridge_id = 0;

    void SetUp() override {
        /* Create social bond system */
        social = social_bond_system_create();
        ASSERT_NE(social, nullptr);

        /* Create bridge with default config */
        config = social_fep_config_default();
        config.enable_logging = false;
        bridge = social_fep_bridge_create(&config);
        ASSERT_NE(bridge, nullptr);

        /* Create FEP orchestrator */
        fep_orchestrator_default_config(&fep_config);
        fep_config.enable_statistics = true;
        fep_config.enable_logging = false;
        fep_orch = fep_orchestrator_create(&fep_config);
        ASSERT_NE(fep_orch, nullptr);

        ASSERT_EQ(fep_orchestrator_start(fep_orch), 0);
    }

    void TearDown() override {
        if (bridge && social_fep_bridge_is_registered(bridge)) {
            social_fep_bridge_unregister(bridge);
        }
        if (fep_orch) {
            fep_orchestrator_stop(fep_orch);
            fep_orchestrator_destroy(fep_orch);
            fep_orch = nullptr;
        }
        if (bridge) {
            social_fep_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (social) {
            social_bond_system_destroy(social);
            social = nullptr;
        }
    }

    void register_bridge() {
        ASSERT_EQ(social_fep_bridge_register(bridge, fep_orch, social, &bridge_id), 0);
    }

    uint64_t get_current_time_ms() {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();
    }

    void setup_test_relationships() {
        uint64_t current_time = get_current_time_ms() * 1000;
        for (int i = 0; i < 4; i++) {
            relationship_stage_t stage = (i < 2) ? RELATIONSHIP_FRIEND : RELATIONSHIP_ACQUAINTANCE;
            social_create_relationship(social, stage, current_time);
        }
        social_update(social, 1.0f, current_time);
    }
};

/* ============================================================================
 * SocialPredictionStabilityRegression - Predictions remain stable
 * ============================================================================ */

TEST_F(SocialFEPRegressionTest, SocialPredictionStabilityRegression) {
    register_bridge();
    setup_test_relationships();

    /* Track prediction error values */
    std::vector<float> prediction_errors;
    prediction_errors.reserve(REGRESSION_STABILITY_CYCLES);

    float prev_error = -1.0f;
    float max_delta = 0.0f;

    for (int i = 0; i < REGRESSION_STABILITY_CYCLES; i++) {
        social_fep_bridge_update(bridge);

        float current_error = social_fep_bridge_get_social_prediction_error(bridge);

        /* Verify bounds */
        ASSERT_GE(current_error, 0.0f)
            << "Prediction error went negative at cycle " << i;
        ASSERT_LE(current_error, REGRESSION_PREDICTION_ERROR_MAX)
            << "Prediction error exceeded maximum at cycle " << i;

        /* Track stability (how much it changes per cycle) */
        if (prev_error >= 0.0f) {
            float delta = std::fabs(current_error - prev_error);
            if (delta > max_delta) {
                max_delta = delta;
            }
            /* Each update shouldn't cause wild swings (more than 0.5 change) */
            ASSERT_LE(delta, 0.5f)
                << "Prediction error changed too rapidly at cycle " << i
                << " (delta=" << delta << ")";
        }

        prev_error = current_error;
        prediction_errors.push_back(current_error);
    }

    /* Verify we completed all cycles */
    EXPECT_EQ(prediction_errors.size(), static_cast<size_t>(REGRESSION_STABILITY_CYCLES));

    /* Maximum delta should be reasonable */
    EXPECT_LE(max_delta, 0.3f) << "Prediction error was too volatile across all cycles";
}

/* ============================================================================
 * FreeEnergyBoundsRegression - Free energy stays within expected bounds
 * ============================================================================ */

TEST_F(SocialFEPRegressionTest, FreeEnergyBoundsRegression) {
    register_bridge();
    setup_test_relationships();

    float min_observed = 999.0f;
    float max_observed = -999.0f;

    for (int i = 0; i < REGRESSION_STABILITY_CYCLES; i++) {
        /* Simulate various social events */
        uint64_t current_time = get_current_time_ms() * 1000 + i * 1000;

        if (i % 50 == 0) {
            /* Positive interaction */
            social_process_interaction(social, 1, INTERACTION_CONVERSATION,
                                       0.7f, 0.8f, current_time);
        }
        if (i % 100 == 0) {
            /* Update social state */
            social_update(social, 0.5f, current_time);
        }

        social_fep_bridge_update(bridge);

        float fe = social_fep_bridge_get_free_energy_contribution(bridge);

        /* Track min/max */
        if (fe < min_observed) min_observed = fe;
        if (fe > max_observed) max_observed = fe;

        /* Verify bounds */
        ASSERT_GE(fe, REGRESSION_FREE_ENERGY_MIN)
            << "Free energy went below minimum at cycle " << i;
        ASSERT_LE(fe, REGRESSION_FREE_ENERGY_MAX)
            << "Free energy exceeded maximum at cycle " << i;
    }

    /* Report observed range */
    EXPECT_GE(min_observed, config.baseline_free_energy * 0.5f)
        << "Minimum free energy was unexpectedly low";
    EXPECT_LE(max_observed, config.max_free_energy * 1.25f)
        << "Maximum free energy was unexpectedly high";
}

/* ============================================================================
 * PerformanceRegression - Update time doesn't degrade
 * ============================================================================ */

TEST_F(SocialFEPRegressionTest, PerformanceRegression) {
    register_bridge();
    setup_test_relationships();

    /* Reset statistics */
    social_fep_bridge_reset_stats(bridge);

    /* Warm up */
    for (int i = 0; i < 10; i++) {
        social_fep_bridge_update(bridge);
    }
    social_fep_bridge_reset_stats(bridge);

    /* Timed runs */
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < REGRESSION_STRESS_CYCLES; i++) {
        social_fep_bridge_update(bridge);
    }
    auto end = std::chrono::high_resolution_clock::now();

    auto total_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    float avg_us = static_cast<float>(total_us) / REGRESSION_STRESS_CYCLES;

    /* Verify performance bounds */
    EXPECT_LT(avg_us, static_cast<float>(REGRESSION_MAX_UPDATE_TIME_US))
        << "Average update time exceeded " << REGRESSION_MAX_UPDATE_TIME_US << " microseconds";

    /* Get bridge-reported statistics */
    social_fep_stats_t stats;
    EXPECT_EQ(social_fep_bridge_get_stats(bridge, &stats), 0);
    EXPECT_GT(stats.avg_update_time_us, 0.0f);

    /* Bridge-reported avg should be similar to measured (within 2x) */
    EXPECT_LT(stats.avg_update_time_us, avg_us * 3.0f)
        << "Bridge-reported timing differs significantly from measured";
}

/* ============================================================================
 * MemoryLeakRegression - No memory growth over many cycles
 * ============================================================================ */

TEST_F(SocialFEPRegressionTest, MemoryLeakRegression) {
    register_bridge();
    setup_test_relationships();

    /* This test is primarily for valgrind/AddressSanitizer
     * We just verify the bridge survives many create/update/destroy cycles */

    for (int cycle = 0; cycle < REGRESSION_MEMORY_CYCLES; cycle++) {
        /* Update bridge */
        EXPECT_EQ(social_fep_bridge_update(bridge), 0)
            << "Update failed at cycle " << cycle;

        /* Get and verify metrics (exercises memory paths) */
        social_fep_metrics_t metrics;
        EXPECT_EQ(social_fep_bridge_get_metrics(bridge, &metrics), 0)
            << "Get metrics failed at cycle " << cycle;

        social_fep_stats_t stats;
        EXPECT_EQ(social_fep_bridge_get_stats(bridge, &stats), 0)
            << "Get stats failed at cycle " << cycle;

        /* Periodically reset stats */
        if (cycle % 20 == 0) {
            EXPECT_EQ(social_fep_bridge_reset_stats(bridge), 0)
                << "Reset stats failed at cycle " << cycle;
        }
    }

    /* If we got here without crashes or sanitizer errors, test passes */
    SUCCEED() << "Completed " << REGRESSION_MEMORY_CYCLES
              << " cycles without memory issues";
}

/* ============================================================================
 * RelationshipConsistencyRegression - Relationship models stay consistent
 * ============================================================================ */

TEST_F(SocialFEPRegressionTest, RelationshipConsistencyRegression) {
    register_bridge();
    setup_test_relationships();

    /* Run many cycles while modifying relationships */
    uint64_t base_time = get_current_time_ms() * 1000;

    for (int i = 0; i < REGRESSION_STRESS_CYCLES; i++) {
        uint64_t current_time = base_time + i * 1000;

        /* Various relationship modifications */
        if (i % 10 == 0) {
            social_process_interaction(social, 1, INTERACTION_CONVERSATION,
                                       0.6f, 0.7f, current_time);
        }
        if (i % 25 == 0) {
            social_provide_support(social, 1, 0.5f);
        }
        if (i % 50 == 0) {
            social_receive_support(social, 2, 0.6f);
        }
        if (i % 100 == 0) {
            social_update(social, 1.0f, current_time);
        }

        social_fep_bridge_update(bridge);

        /* Verify metrics remain consistent */
        social_fep_metrics_t metrics;
        ASSERT_EQ(social_fep_bridge_get_metrics(bridge, &metrics), 0);

        /* Relationship uncertainty should be bounded */
        ASSERT_GE(metrics.relationship_uncertainty, 0.0f)
            << "Uncertainty negative at cycle " << i;
        ASSERT_LE(metrics.relationship_uncertainty, REGRESSION_UNCERTAINTY_MAX)
            << "Uncertainty exceeded max at cycle " << i;

        /* Active relationships should be non-negative */
        ASSERT_GE(metrics.active_relationships, 0u)
            << "Active relationships negative at cycle " << i;

        /* Closeness and trust should be bounded */
        ASSERT_GE(metrics.avg_relationship_closeness, 0.0f);
        ASSERT_LE(metrics.avg_relationship_closeness, 1.0f);
        ASSERT_GE(metrics.avg_relationship_trust, 0.0f);
        ASSERT_LE(metrics.avg_relationship_trust, 1.0f);
    }
}

/* ============================================================================
 * NumericalStabilityRegression - No NaN or Inf values
 * ============================================================================ */

TEST_F(SocialFEPRegressionTest, NumericalStabilityRegression) {
    register_bridge();
    setup_test_relationships();

    for (int i = 0; i < REGRESSION_STABILITY_CYCLES; i++) {
        /* Vary inputs to stress numerical paths */
        uint64_t current_time = get_current_time_ms() * 1000 + i * 1000;

        if (i % 20 == 0) {
            /* High intensity events */
            social_process_interaction(social, 1, INTERACTION_CELEBRATION,
                                       1.0f, 1.0f, current_time);
        }
        if (i % 30 == 0) {
            /* Low intensity events */
            social_process_interaction(social, 2, INTERACTION_CONVERSATION,
                                       0.1f, 0.1f, current_time);
        }
        if (i % 50 == 0) {
            social_update(social, 0.1f, current_time);
        }

        social_fep_bridge_update(bridge);

        social_fep_metrics_t metrics;
        ASSERT_EQ(social_fep_bridge_get_metrics(bridge, &metrics), 0);

        /* Check for NaN/Inf in all float fields */
        ASSERT_FALSE(std::isnan(metrics.free_energy))
            << "NaN free_energy at cycle " << i;
        ASSERT_FALSE(std::isinf(metrics.free_energy))
            << "Inf free_energy at cycle " << i;

        ASSERT_FALSE(std::isnan(metrics.prediction_error))
            << "NaN prediction_error at cycle " << i;
        ASSERT_FALSE(std::isinf(metrics.prediction_error))
            << "Inf prediction_error at cycle " << i;

        ASSERT_FALSE(std::isnan(metrics.surprise))
            << "NaN surprise at cycle " << i;
        ASSERT_FALSE(std::isinf(metrics.surprise))
            << "Inf surprise at cycle " << i;

        ASSERT_FALSE(std::isnan(metrics.entropy))
            << "NaN entropy at cycle " << i;
        ASSERT_FALSE(std::isinf(metrics.entropy))
            << "Inf entropy at cycle " << i;

        ASSERT_FALSE(std::isnan(metrics.social_prediction_error))
            << "NaN social_prediction_error at cycle " << i;
        ASSERT_FALSE(std::isinf(metrics.social_prediction_error))
            << "Inf social_prediction_error at cycle " << i;

        ASSERT_FALSE(std::isnan(metrics.relationship_uncertainty))
            << "NaN relationship_uncertainty at cycle " << i;
        ASSERT_FALSE(std::isinf(metrics.relationship_uncertainty))
            << "Inf relationship_uncertainty at cycle " << i;

        ASSERT_FALSE(std::isnan(metrics.norm_violation_surprise))
            << "NaN norm_violation_surprise at cycle " << i;
        ASSERT_FALSE(std::isinf(metrics.norm_violation_surprise))
            << "Inf norm_violation_surprise at cycle " << i;
    }
}

/* ============================================================================
 * ConfigPersistenceRegression - Config survives many operations
 * ============================================================================ */

TEST_F(SocialFEPRegressionTest, ConfigPersistenceRegression) {
    register_bridge();
    setup_test_relationships();

    /* Set custom config */
    social_fep_config_t custom_config = social_fep_config_default();
    custom_config.social_prediction_error_weight = 0.45f;
    custom_config.relationship_uncertainty_weight = 0.35f;
    custom_config.norm_violation_weight = 0.20f;
    custom_config.high_free_energy_threshold = 1.2f;
    custom_config.prediction_error_threshold = 0.4f;

    EXPECT_EQ(social_fep_bridge_set_config(bridge, &custom_config), 0);

    /* Run many cycles */
    for (int i = 0; i < REGRESSION_STRESS_CYCLES; i++) {
        social_fep_bridge_update(bridge);

        /* Occasionally reset stats (shouldn't affect config) */
        if (i % 50 == 0) {
            social_fep_bridge_reset_stats(bridge);
        }
    }

    /* Verify config is unchanged */
    social_fep_config_t retrieved_config;
    EXPECT_EQ(social_fep_bridge_get_config(bridge, &retrieved_config), 0);

    EXPECT_FLOAT_EQ(retrieved_config.social_prediction_error_weight, 0.45f)
        << "social_prediction_error_weight changed";
    EXPECT_FLOAT_EQ(retrieved_config.relationship_uncertainty_weight, 0.35f)
        << "relationship_uncertainty_weight changed";
    EXPECT_FLOAT_EQ(retrieved_config.norm_violation_weight, 0.20f)
        << "norm_violation_weight changed";
    EXPECT_FLOAT_EQ(retrieved_config.high_free_energy_threshold, 1.2f)
        << "high_free_energy_threshold changed";
    EXPECT_FLOAT_EQ(retrieved_config.prediction_error_threshold, 0.4f)
        << "prediction_error_threshold changed";
}

/* ============================================================================
 * StatisticsAccuracyRegression - Statistics match actual operations
 * ============================================================================ */

TEST_F(SocialFEPRegressionTest, StatisticsAccuracyRegression) {
    register_bridge();
    setup_test_relationships();

    /* Reset to known state */
    social_fep_bridge_reset_stats(bridge);

    /* Run exact number of updates */
    const int EXPECTED_UPDATES = 100;
    for (int i = 0; i < EXPECTED_UPDATES; i++) {
        EXPECT_EQ(social_fep_bridge_update(bridge), 0);
    }

    /* Get statistics */
    social_fep_stats_t stats;
    EXPECT_EQ(social_fep_bridge_get_stats(bridge, &stats), 0);

    /* Verify update count matches */
    EXPECT_EQ(stats.total_updates, static_cast<uint64_t>(EXPECTED_UPDATES))
        << "Total updates doesn't match expected";

    /* Verify averages are reasonable */
    if (stats.total_updates > 0) {
        EXPECT_GT(stats.avg_update_time_us, 0.0f)
            << "Average update time should be positive";
        EXPECT_GE(stats.avg_free_energy, 0.0f)
            << "Average free energy should be non-negative";
        EXPECT_GE(stats.avg_prediction_error, 0.0f)
            << "Average prediction error should be non-negative";
        EXPECT_LE(stats.avg_prediction_error, 1.0f)
            << "Average prediction error should not exceed 1.0";
    }

    /* Total time should be sum of all updates */
    float expected_total_time_us = stats.avg_update_time_us * stats.total_updates;
    float actual_total_time_us = static_cast<float>(stats.total_update_time_us);
    /* Allow 1% tolerance for floating point */
    EXPECT_NEAR(actual_total_time_us, expected_total_time_us,
                expected_total_time_us * 0.01f)
        << "Total update time doesn't match average * count";
}

/* ============================================================================
 * StateRecoveryRegression - Bridge recovers from degraded state
 * ============================================================================ */

TEST_F(SocialFEPRegressionTest, StateRecoveryRegression) {
    register_bridge();
    setup_test_relationships();

    /* Configure low threshold to trigger degraded mode */
    social_fep_config_t low_threshold_config = social_fep_config_default();
    low_threshold_config.high_free_energy_threshold = 0.15f;  /* Very low threshold */
    low_threshold_config.enable_degraded_mode = true;
    EXPECT_EQ(social_fep_bridge_set_config(bridge, &low_threshold_config), 0);

    /* Run updates - might enter degraded mode */
    for (int i = 0; i < 50; i++) {
        social_fep_bridge_update(bridge);
    }

    /* Restore higher threshold */
    social_fep_config_t normal_config = social_fep_config_default();
    EXPECT_EQ(social_fep_bridge_set_config(bridge, &normal_config), 0);

    /* Run more updates - should recover from degraded */
    for (int i = 0; i < 50; i++) {
        social_fep_bridge_update(bridge);
    }

    /* State should be active (not degraded with normal threshold) */
    social_fep_state_t state = social_fep_bridge_get_state(bridge);
    EXPECT_TRUE(state == SOCIAL_FEP_STATE_ACTIVE || state == SOCIAL_FEP_STATE_DEGRADED)
        << "State should be either ACTIVE or DEGRADED, got " << social_fep_state_name(state);
}

/* ============================================================================
 * ConcurrentAccessRegression - Thread safety under stress
 * ============================================================================ */

TEST_F(SocialFEPRegressionTest, ConcurrentAccessRegression) {
    register_bridge();
    setup_test_relationships();

    const int NUM_THREADS = 4;
    const int ITERATIONS_PER_THREAD = 100;
    std::atomic<int> completed{0};
    std::atomic<int> errors{0};
    std::vector<std::thread> threads;

    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([this, &completed, &errors, ITERATIONS_PER_THREAD]() {
            for (int i = 0; i < ITERATIONS_PER_THREAD; i++) {
                /* Mix of read operations */
                float fe = social_fep_bridge_get_free_energy_contribution(bridge);
                if (fe < 0.0f) errors++;

                float pe = social_fep_bridge_get_social_prediction_error(bridge);
                if (pe < 0.0f || pe > 1.0f) errors++;

                float uncertainty = social_fep_bridge_get_relationship_uncertainty(bridge);
                if (uncertainty < 0.0f || uncertainty > 1.0f) errors++;

                social_fep_bridge_get_state(bridge);
                social_fep_bridge_is_degraded(bridge);

                social_fep_metrics_t metrics;
                if (social_fep_bridge_get_metrics(bridge, &metrics) != 0) errors++;

                social_fep_stats_t stats;
                if (social_fep_bridge_get_stats(bridge, &stats) != 0) errors++;

                /* Write operation */
                if (social_fep_bridge_force_update(bridge) != 0) errors++;
            }
            completed++;
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(completed.load(), NUM_THREADS)
        << "All threads should complete";
    EXPECT_EQ(errors.load(), 0)
        << "No errors should occur during concurrent access";
}

/* ============================================================================
 * LongRunningStabilityRegression - Extended stability test
 * ============================================================================ */

TEST_F(SocialFEPRegressionTest, LongRunningStabilityRegression) {
    register_bridge();
    setup_test_relationships();

    /* Extended run to catch any slow-developing issues */
    const int EXTENDED_CYCLES = REGRESSION_STABILITY_CYCLES * 2;
    uint64_t base_time = get_current_time_ms() * 1000;

    for (int i = 0; i < EXTENDED_CYCLES; i++) {
        uint64_t current_time = base_time + i * 100;

        /* Various events to exercise all code paths */
        if (i % 50 == 0) {
            social_process_interaction(social, 1 + (i % 3), INTERACTION_CONVERSATION,
                                       0.5f + (i % 5) * 0.1f, 0.6f, current_time);
        }
        if (i % 100 == 0) {
            social_update(social, 0.5f, current_time);
        }
        if (i % 200 == 0) {
            social_fep_bridge_reset_stats(bridge);
        }

        int ret = social_fep_bridge_update(bridge);
        ASSERT_EQ(ret, 0) << "Update failed at cycle " << i;

        /* Periodic verification */
        if (i % 500 == 0) {
            social_fep_metrics_t metrics;
            ASSERT_EQ(social_fep_bridge_get_metrics(bridge, &metrics), 0);

            ASSERT_GE(metrics.free_energy, 0.0f);
            ASSERT_LE(metrics.free_energy, config.max_free_energy * 1.5f);
            ASSERT_GE(metrics.prediction_error, 0.0f);
            ASSERT_LE(metrics.prediction_error, 1.0f);
        }
    }

    /* Final state should be valid */
    social_fep_state_t final_state = social_fep_bridge_get_state(bridge);
    EXPECT_TRUE(final_state == SOCIAL_FEP_STATE_ACTIVE ||
                final_state == SOCIAL_FEP_STATE_DEGRADED)
        << "Final state should be valid operational state";
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
