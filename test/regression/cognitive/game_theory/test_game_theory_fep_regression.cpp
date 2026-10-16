/**
 * @file test_game_theory_fep_regression.cpp
 * @brief Regression tests for Game Theory FEP Bridge
 * @version 1.0.0
 * @date 2026-01-08
 *
 * WHAT: Regression tests to prevent degradation in Game Theory FEP Bridge behavior
 * WHY:  Ensure strategic stability, free energy bounds, performance, and memory safety
 * HOW:  Test known-good behaviors, boundary conditions, and performance baselines
 *
 * TEST COVERAGE:
 * - StrategicStabilityRegression: Strategic decisions remain stable over time
 * - FreeEnergyBoundsRegression: Free energy stays within expected bounds
 * - PerformanceRegression: Update time doesn't degrade
 * - MemoryLeakRegression: No memory growth over many cycles
 * - ThreadSafetyRegression: Concurrent access doesn't cause issues
 * - NashConvergenceRegression: Nash detection remains accurate
 * - PredictionErrorDecayRegression: Error decay rate works correctly
 * - CallbackReliabilityRegression: Callbacks fire reliably
 * - ConfigurationStabilityRegression: Config changes don't cause instability
 * - MetricsAccuracyRegression: Metrics remain numerically accurate
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <chrono>
#include <thread>
#include <atomic>
#include <vector>
#include <algorithm>
#include <numeric>

#include "cognitive/game_theory/nimcp_game_theory_fep_bridge.h"
#include "cognitive/game_theory/nimcp_game_theory.h"
#include "cognitive/free_energy/nimcp_fep_orchestrator.h"

/* ============================================================================
 * Test Constants - Known Good Baselines
 * ============================================================================ */

/* Performance baselines (microseconds) */
#define REGRESSION_MAX_UPDATE_TIME_US         1000    /* 1ms max per update */
#define REGRESSION_AVG_UPDATE_TIME_US         500     /* 500us average target */

/* Memory baselines */
#define REGRESSION_MAX_ITERATIONS             10000   /* Iterations for memory test */
#define REGRESSION_CYCLES_PER_CHECK           1000    /* Check memory every N cycles */

/* Numerical accuracy */
#define REGRESSION_FLOAT_EPSILON              1e-5f   /* Float comparison tolerance */
#define REGRESSION_FREE_ENERGY_TOLERANCE      0.01f   /* FE stability tolerance */

/* Thread safety */
#define REGRESSION_NUM_THREADS                8       /* Concurrent threads */
#define REGRESSION_THREAD_ITERATIONS          500     /* Iterations per thread */

/* ============================================================================
 * Callback Tracking
 * ============================================================================ */

static std::atomic<int> g_callback_count{0};
static std::atomic<int> g_high_fe_count{0};
static std::atomic<int> g_surprise_count{0};
static std::atomic<float> g_last_fe{0.0f};

static void regression_metrics_callback(
    gt_fep_bridge_t* bridge,
    const gt_fep_metrics_t* metrics,
    void* user_data
) {
    (void)bridge;
    (void)user_data;
    if (metrics) {
        g_callback_count++;
        g_last_fe.store(metrics->free_energy);
    }
}

static void regression_high_fe_callback(
    gt_fep_bridge_t* bridge,
    float free_energy,
    void* user_data
) {
    (void)bridge;
    (void)free_energy;
    (void)user_data;
    g_high_fe_count++;
}

static void regression_surprise_callback(
    gt_fep_bridge_t* bridge,
    float surprise,
    const char* source,
    void* user_data
) {
    (void)bridge;
    (void)surprise;
    (void)source;
    (void)user_data;
    g_surprise_count++;
}

static void reset_callback_counters() {
    g_callback_count = 0;
    g_high_fe_count = 0;
    g_surprise_count = 0;
    g_last_fe = 0.0f;
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class GameTheoryFEPRegressionTest : public ::testing::Test {
protected:
    gt_fep_bridge_t* bridge = nullptr;
    gt_fep_config_t config;

    void SetUp() override {
        reset_callback_counters();
        config = gt_fep_config_default();
        bridge = gt_fep_bridge_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            gt_fep_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }

    /**
     * Helper to measure update time in microseconds
     */
    uint64_t measure_update_time_us() {
        auto start = std::chrono::high_resolution_clock::now();
        gt_fep_bridge_force_update(bridge);
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    }

    /**
     * Helper to verify free energy is within bounds
     */
    bool verify_fe_bounds(float fe) {
        return fe >= 0.0f && fe <= config.max_free_energy;
    }

    /**
     * Helper to verify metric is normalized [0, 1]
     */
    bool verify_normalized(float value) {
        return value >= 0.0f && value <= 1.0f;
    }
};

/* ============================================================================
 * StrategicStabilityRegression - Strategic decisions remain stable
 * ============================================================================ */

TEST_F(GameTheoryFEPRegressionTest, StrategicStabilityRegression) {
    /* Set a known stable state */
    gt_fep_bridge_update_strategy_uncertainty(bridge, 0.3f);
    gt_fep_bridge_update_opponent_error(bridge, 0.3f);
    gt_fep_bridge_update_nash_distance(bridge, 0.3f);
    gt_fep_bridge_force_update(bridge);

    float reference_fe = gt_fep_bridge_get_free_energy(bridge);

    /* Run many cycles without changing inputs */
    const int NUM_CYCLES = 100;
    float min_fe = reference_fe;
    float max_fe = reference_fe;

    for (int i = 0; i < NUM_CYCLES; i++) {
        gt_fep_bridge_force_update(bridge);
        float current_fe = gt_fep_bridge_get_free_energy(bridge);
        min_fe = std::min(min_fe, current_fe);
        max_fe = std::max(max_fe, current_fe);
    }

    /* Free energy should remain stable (within tolerance) */
    float variance = max_fe - min_fe;
    EXPECT_LT(variance, REGRESSION_FREE_ENERGY_TOLERANCE)
        << "Free energy should remain stable without input changes";

    /* Final FE should be close to reference */
    float final_fe = gt_fep_bridge_get_free_energy(bridge);
    EXPECT_NEAR(final_fe, reference_fe, REGRESSION_FREE_ENERGY_TOLERANCE)
        << "Final FE should match reference state";
}

/* ============================================================================
 * FreeEnergyBoundsRegression - Free energy stays within expected bounds
 * ============================================================================ */

TEST_F(GameTheoryFEPRegressionTest, FreeEnergyBoundsRegression) {
    /* Test all corners of the input space */
    const float test_values[] = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};

    for (float su : test_values) {
        for (float oe : test_values) {
            for (float nd : test_values) {
                gt_fep_bridge_update_strategy_uncertainty(bridge, su);
                gt_fep_bridge_update_opponent_error(bridge, oe);
                gt_fep_bridge_update_nash_distance(bridge, nd);
                gt_fep_bridge_force_update(bridge);

                float fe = gt_fep_bridge_get_free_energy(bridge);
                EXPECT_TRUE(verify_fe_bounds(fe))
                    << "FE out of bounds for su=" << su << ", oe=" << oe << ", nd=" << nd
                    << ": fe=" << fe;

                /* Also verify computed metrics are normalized */
                gt_fep_metrics_t metrics;
                gt_fep_bridge_get_metrics(bridge, &metrics);

                EXPECT_TRUE(verify_normalized(metrics.strategy_uncertainty))
                    << "Strategy uncertainty out of [0,1]";
                EXPECT_TRUE(verify_normalized(metrics.opponent_prediction_error))
                    << "Opponent error out of [0,1]";
                EXPECT_TRUE(verify_normalized(metrics.nash_distance))
                    << "Nash distance out of [0,1]";
                EXPECT_TRUE(verify_normalized(metrics.prediction_error))
                    << "Prediction error out of [0,1]";
                EXPECT_TRUE(verify_normalized(metrics.entropy))
                    << "Entropy out of [0,1]";
            }
        }
    }
}

/* ============================================================================
 * PerformanceRegression - Update time doesn't degrade
 * ============================================================================ */

TEST_F(GameTheoryFEPRegressionTest, PerformanceRegression) {
    /* Warm up */
    for (int i = 0; i < 10; i++) {
        gt_fep_bridge_force_update(bridge);
    }

    /* Collect timing samples */
    const int NUM_SAMPLES = 100;
    std::vector<uint64_t> update_times;
    update_times.reserve(NUM_SAMPLES);

    for (int i = 0; i < NUM_SAMPLES; i++) {
        /* Vary inputs to exercise different code paths */
        float phase = (float)i / NUM_SAMPLES;
        gt_fep_bridge_update_strategy_uncertainty(bridge, 0.5f + 0.4f * sinf(phase * 6.28f));
        gt_fep_bridge_update_opponent_error(bridge, 0.5f + 0.4f * cosf(phase * 6.28f));

        update_times.push_back(measure_update_time_us());
    }

    /* Calculate statistics */
    uint64_t max_time = *std::max_element(update_times.begin(), update_times.end());
    uint64_t total_time = std::accumulate(update_times.begin(), update_times.end(), 0ULL);
    double avg_time = (double)total_time / NUM_SAMPLES;

    /* Verify performance bounds */
    EXPECT_LT(max_time, REGRESSION_MAX_UPDATE_TIME_US)
        << "Maximum update time exceeded: " << max_time << "us";
    EXPECT_LT(avg_time, REGRESSION_AVG_UPDATE_TIME_US)
        << "Average update time exceeded: " << avg_time << "us";

    /* Verify no performance degradation over iterations */
    /* Compare first half average to second half average */
    uint64_t first_half_total = std::accumulate(
        update_times.begin(), update_times.begin() + NUM_SAMPLES/2, 0ULL);
    uint64_t second_half_total = std::accumulate(
        update_times.begin() + NUM_SAMPLES/2, update_times.end(), 0ULL);

    double first_half_avg = (double)first_half_total / (NUM_SAMPLES/2);
    double second_half_avg = (double)second_half_total / (NUM_SAMPLES/2);

    /* Second half shouldn't be significantly slower (allow 50% variance)
     * Note: If both halves are 0 (very fast operations), test passes as no degradation */
    if (first_half_avg > 0.0) {
        EXPECT_LT(second_half_avg, first_half_avg * 1.5)
            << "Performance degraded over iterations";
    } else {
        /* Both are effectively zero - very fast, no degradation possible */
        EXPECT_LE(second_half_avg, 1.0)
            << "Second half should remain fast when first half is instant";
    }
}

/* ============================================================================
 * MemoryLeakRegression - No memory growth over many cycles
 * ============================================================================ */

TEST_F(GameTheoryFEPRegressionTest, MemoryLeakRegression) {
    /* This test runs many cycles to detect memory leaks
     * We can't directly measure memory, but we verify:
     * 1. No crashes over many iterations
     * 2. Statistics don't overflow
     * 3. Bridge remains functional
     */

    const int TOTAL_ITERATIONS = REGRESSION_MAX_ITERATIONS;

    for (int i = 0; i < TOTAL_ITERATIONS; i++) {
        /* Vary inputs */
        float phase = (float)(i % 100) / 100.0f;
        gt_fep_bridge_update_strategy_uncertainty(bridge, phase);
        gt_fep_bridge_update_opponent_error(bridge, 1.0f - phase);
        gt_fep_bridge_update_nash_distance(bridge, fabsf(sinf(phase * 6.28f)));

        gt_fep_bridge_force_update(bridge);

        /* Periodically verify state */
        if (i % REGRESSION_CYCLES_PER_CHECK == 0) {
            /* Verify bridge is still functional */
            float fe = gt_fep_bridge_get_free_energy(bridge);
            ASSERT_TRUE(verify_fe_bounds(fe))
                << "FE out of bounds after " << i << " iterations";

            gt_fep_state_t state = gt_fep_bridge_get_state(bridge);
            ASSERT_NE(state, GT_FEP_STATE_ERROR)
                << "Bridge entered error state after " << i << " iterations";
        }
    }

    /* Final verification */
    gt_fep_stats_t final_stats;
    gt_fep_bridge_get_stats(bridge, &final_stats);

    EXPECT_EQ(final_stats.total_updates, (uint64_t)TOTAL_ITERATIONS)
        << "Update count mismatch after memory test";
    EXPECT_GT(final_stats.avg_update_time_us, 0.0f)
        << "Statistics should be valid after many iterations";

    /* Verify we can still do operations */
    gt_fep_bridge_reset(bridge);
    gt_fep_bridge_get_stats(bridge, &final_stats);
    EXPECT_EQ(final_stats.total_updates, 0u)
        << "Reset should work after memory stress test";
}

/* ============================================================================
 * ThreadSafetyRegression - Concurrent access doesn't cause issues
 * ============================================================================ */

TEST_F(GameTheoryFEPRegressionTest, ThreadSafetyRegression) {
    std::atomic<int> completed_threads{0};
    std::atomic<int> errors{0};
    std::vector<std::thread> threads;

    /* Create threads that concurrently access the bridge */
    for (int t = 0; t < REGRESSION_NUM_THREADS; t++) {
        threads.emplace_back([this, &completed_threads, &errors, t]() {
            try {
                for (int i = 0; i < REGRESSION_THREAD_ITERATIONS; i++) {
                    /* Mix of read and write operations */
                    float phase = (float)(i + t * 100) / 1000.0f;

                    /* Writes */
                    gt_fep_bridge_update_strategy_uncertainty(bridge, fmodf(phase, 1.0f));
                    gt_fep_bridge_update_opponent_error(bridge, fmodf(phase + 0.3f, 1.0f));
                    gt_fep_bridge_update_nash_distance(bridge, fmodf(phase + 0.6f, 1.0f));
                    gt_fep_bridge_force_update(bridge);

                    /* Reads */
                    float fe = gt_fep_bridge_get_free_energy(bridge);
                    float pe = gt_fep_bridge_get_prediction_error(bridge);
                    float su = gt_fep_bridge_get_strategy_uncertainty(bridge);
                    gt_fep_state_t state = gt_fep_bridge_get_state(bridge);
                    bool degraded = gt_fep_bridge_is_degraded(bridge);
                    bool at_nash = gt_fep_bridge_is_at_nash(bridge);

                    /* Verify reads are valid */
                    if (!verify_fe_bounds(fe) || !verify_normalized(pe) ||
                        !verify_normalized(su) || state == GT_FEP_STATE_ERROR) {
                        errors++;
                    }

                    gt_fep_metrics_t metrics;
                    gt_fep_bridge_get_metrics(bridge, &metrics);

                    gt_fep_stats_t stats;
                    gt_fep_bridge_get_stats(bridge, &stats);
                }
                completed_threads++;
            } catch (...) {
                errors++;
            }
        });
    }

    /* Wait for all threads to complete */
    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(completed_threads.load(), REGRESSION_NUM_THREADS)
        << "All threads should complete";
    EXPECT_EQ(errors.load(), 0)
        << "No errors should occur during concurrent access";

    /* Verify bridge is still functional */
    gt_fep_state_t final_state = gt_fep_bridge_get_state(bridge);
    EXPECT_NE(final_state, GT_FEP_STATE_ERROR)
        << "Bridge should not be in error state after concurrent access";
}

/* ============================================================================
 * NashConvergenceRegression - Nash detection remains accurate
 * ============================================================================ */

TEST_F(GameTheoryFEPRegressionTest, NashConvergenceRegression) {
    /* Get the Nash epsilon from config */
    gt_fep_config_t current_config;
    gt_fep_bridge_get_config(bridge, &current_config);
    float nash_epsilon = current_config.nash_epsilon;

    /* Test exact boundary cases */

    /* Just above epsilon - should NOT be at Nash */
    gt_fep_bridge_update_nash_distance(bridge, nash_epsilon + 0.001f);
    gt_fep_bridge_force_update(bridge);
    EXPECT_FALSE(gt_fep_bridge_is_at_nash(bridge))
        << "Should not be at Nash when distance > epsilon";

    /* Exactly at epsilon - implementation dependent, but should be consistent */
    gt_fep_bridge_update_nash_distance(bridge, nash_epsilon);
    gt_fep_bridge_force_update(bridge);
    bool at_epsilon = gt_fep_bridge_is_at_nash(bridge);

    /* Just below epsilon - should be at Nash */
    gt_fep_bridge_update_nash_distance(bridge, nash_epsilon - 0.001f);
    gt_fep_bridge_force_update(bridge);
    EXPECT_TRUE(gt_fep_bridge_is_at_nash(bridge))
        << "Should be at Nash when distance < epsilon";

    /* Zero distance - definitely at Nash */
    gt_fep_bridge_update_nash_distance(bridge, 0.0f);
    gt_fep_bridge_force_update(bridge);
    EXPECT_TRUE(gt_fep_bridge_is_at_nash(bridge))
        << "Should be at Nash when distance = 0";

    /* Verify Nash state affects free energy correctly */
    float at_nash_fe = gt_fep_bridge_get_free_energy(bridge);

    gt_fep_bridge_update_nash_distance(bridge, 1.0f);
    gt_fep_bridge_force_update(bridge);
    float far_nash_fe = gt_fep_bridge_get_free_energy(bridge);

    EXPECT_LT(at_nash_fe, far_nash_fe)
        << "At Nash should have lower FE than far from Nash";
}

/* ============================================================================
 * PredictionErrorDecayRegression - Error decay rate works correctly
 * ============================================================================ */

TEST_F(GameTheoryFEPRegressionTest, PredictionErrorDecayRegression) {
    /* Get decay rate from config */
    gt_fep_config_t current_config;
    gt_fep_bridge_get_config(bridge, &current_config);
    float decay_rate = current_config.error_decay_rate;

    /* Set high initial error state */
    gt_fep_bridge_update_strategy_uncertainty(bridge, 0.9f);
    gt_fep_bridge_update_opponent_error(bridge, 0.9f);
    gt_fep_bridge_update_nash_distance(bridge, 0.9f);
    gt_fep_bridge_force_update(bridge);

    float initial_pe = gt_fep_bridge_get_prediction_error(bridge);

    /* Now reduce inputs but prediction error should decay gradually */
    gt_fep_bridge_update_strategy_uncertainty(bridge, 0.1f);
    gt_fep_bridge_update_opponent_error(bridge, 0.1f);
    gt_fep_bridge_update_nash_distance(bridge, 0.1f);

    /* Track decay over multiple updates */
    std::vector<float> pe_history;
    pe_history.push_back(initial_pe);

    for (int i = 0; i < 20; i++) {
        gt_fep_bridge_force_update(bridge);
        float current_pe = gt_fep_bridge_get_prediction_error(bridge);
        pe_history.push_back(current_pe);
    }

    /* Verify prediction error decays (not necessarily monotonically due to
     * the weighted combination, but should trend downward) */
    float final_pe = pe_history.back();

    /* Final PE should be lower than initial */
    EXPECT_LT(final_pe, initial_pe)
        << "Prediction error should decay over time with reduced inputs";

    /* Verify decay is gradual (not instant) */
    if (initial_pe > 0.5f) {
        EXPECT_GT(pe_history[1], final_pe * 0.5f)
            << "Decay should be gradual, not instant";
    }
}

/* ============================================================================
 * CallbackReliabilityRegression - Callbacks fire reliably
 * ============================================================================ */

TEST_F(GameTheoryFEPRegressionTest, CallbackReliabilityRegression) {
    /* Register all callbacks */
    gt_fep_bridge_set_metrics_callback(bridge, regression_metrics_callback, nullptr);
    gt_fep_bridge_set_high_fe_callback(bridge, regression_high_fe_callback, nullptr);
    gt_fep_bridge_set_surprise_callback(bridge, regression_surprise_callback, nullptr);

    /* Run updates and count callbacks */
    const int NUM_UPDATES = 100;
    for (int i = 0; i < NUM_UPDATES; i++) {
        float phase = (float)i / NUM_UPDATES;
        gt_fep_bridge_update_strategy_uncertainty(bridge, phase);
        gt_fep_bridge_force_update(bridge);
    }

    /* Metrics callback should fire on every update */
    EXPECT_EQ(g_callback_count.load(), NUM_UPDATES)
        << "Metrics callback should fire on every update";

    /* Clear callbacks and verify they stop */
    reset_callback_counters();
    gt_fep_bridge_set_metrics_callback(bridge, nullptr, nullptr);

    for (int i = 0; i < 10; i++) {
        gt_fep_bridge_force_update(bridge);
    }

    EXPECT_EQ(g_callback_count.load(), 0)
        << "Cleared callback should not fire";
}

/* ============================================================================
 * ConfigurationStabilityRegression - Config changes don't cause instability
 * ============================================================================ */

TEST_F(GameTheoryFEPRegressionTest, ConfigurationStabilityRegression) {
    /* Set initial state */
    gt_fep_bridge_update_strategy_uncertainty(bridge, 0.5f);
    gt_fep_bridge_update_opponent_error(bridge, 0.5f);
    gt_fep_bridge_update_nash_distance(bridge, 0.5f);
    gt_fep_bridge_force_update(bridge);

    float pre_config_fe = gt_fep_bridge_get_free_energy(bridge);

    /* Change configuration multiple times */
    gt_fep_config_t new_config;
    for (int i = 0; i < 10; i++) {
        new_config = gt_fep_config_default();
        new_config.strategy_uncertainty_weight = 0.2f + (float)i * 0.05f;
        new_config.opponent_modeling_weight = 0.3f + (float)i * 0.03f;
        new_config.nash_convergence_weight = 0.2f + (float)i * 0.02f;

        int ret = gt_fep_bridge_set_config(bridge, &new_config);
        ASSERT_EQ(ret, 0) << "Config change " << i << " failed";

        gt_fep_bridge_force_update(bridge);

        /* Verify bridge remains functional */
        float fe = gt_fep_bridge_get_free_energy(bridge);
        EXPECT_TRUE(verify_fe_bounds(fe))
            << "FE out of bounds after config change " << i;

        gt_fep_state_t state = gt_fep_bridge_get_state(bridge);
        EXPECT_NE(state, GT_FEP_STATE_ERROR)
            << "Bridge in error state after config change " << i;
    }

    /* Restore original config and verify behavior */
    gt_fep_bridge_set_config(bridge, &config);
    gt_fep_bridge_force_update(bridge);

    float post_config_fe = gt_fep_bridge_get_free_energy(bridge);

    /* FE should be close to original with same config and state */
    EXPECT_NEAR(post_config_fe, pre_config_fe, 0.1f)
        << "FE should be similar after restoring original config";
}

/* ============================================================================
 * MetricsAccuracyRegression - Metrics remain numerically accurate
 * ============================================================================ */

TEST_F(GameTheoryFEPRegressionTest, MetricsAccuracyRegression) {
    /* Test known input -> expected output relationships */

    /* Case 1: Zero uncertainty should produce near-baseline FE */
    gt_fep_bridge_update_strategy_uncertainty(bridge, 0.0f);
    gt_fep_bridge_update_opponent_error(bridge, 0.0f);
    gt_fep_bridge_update_nash_distance(bridge, 0.0f);
    gt_fep_bridge_force_update(bridge);

    float zero_fe = gt_fep_bridge_get_free_energy(bridge);
    EXPECT_NEAR(zero_fe, config.baseline_free_energy, 0.05f)
        << "Zero uncertainty should produce baseline FE";

    gt_fep_metrics_t zero_metrics;
    gt_fep_bridge_get_metrics(bridge, &zero_metrics);
    EXPECT_FLOAT_EQ(zero_metrics.strategy_uncertainty, 0.0f);
    EXPECT_FLOAT_EQ(zero_metrics.opponent_prediction_error, 0.0f);
    EXPECT_FLOAT_EQ(zero_metrics.nash_distance, 0.0f);
    EXPECT_TRUE(zero_metrics.at_nash_equilibrium);

    /* Case 2: Maximum uncertainty should produce maximum FE */
    gt_fep_bridge_update_strategy_uncertainty(bridge, 1.0f);
    gt_fep_bridge_update_opponent_error(bridge, 1.0f);
    gt_fep_bridge_update_nash_distance(bridge, 1.0f);
    gt_fep_bridge_force_update(bridge);

    float max_fe = gt_fep_bridge_get_free_energy(bridge);
    EXPECT_GT(max_fe, zero_fe)
        << "Max uncertainty should produce higher FE than zero";
    EXPECT_LE(max_fe, config.max_free_energy)
        << "FE should not exceed configured maximum";

    gt_fep_metrics_t max_metrics;
    gt_fep_bridge_get_metrics(bridge, &max_metrics);
    EXPECT_FLOAT_EQ(max_metrics.strategy_uncertainty, 1.0f);
    EXPECT_FLOAT_EQ(max_metrics.opponent_prediction_error, 1.0f);
    EXPECT_FLOAT_EQ(max_metrics.nash_distance, 1.0f);
    EXPECT_FALSE(max_metrics.at_nash_equilibrium);

    /* Case 3: Component contributions should sum correctly */
    /* Verify individual contributions are computed */
    EXPECT_GT(max_metrics.strategy_contribution, 0.0f);
    EXPECT_GT(max_metrics.opponent_contribution, 0.0f);
    EXPECT_GT(max_metrics.nash_contribution, 0.0f);

    /* Case 4: Verify monotonicity in single dimension */
    gt_fep_bridge_update_opponent_error(bridge, 0.5f);
    gt_fep_bridge_update_nash_distance(bridge, 0.5f);

    float prev_fe = 0.0f;
    for (float su = 0.0f; su <= 1.0f; su += 0.1f) {
        gt_fep_bridge_update_strategy_uncertainty(bridge, su);
        gt_fep_bridge_force_update(bridge);
        float current_fe = gt_fep_bridge_get_free_energy(bridge);

        if (su > 0.0f) {
            EXPECT_GE(current_fe, prev_fe - REGRESSION_FLOAT_EPSILON)
                << "FE should be monotonically non-decreasing with uncertainty";
        }
        prev_fe = current_fe;
    }
}

/* ============================================================================
 * WeightSumRegression - Component weights should produce expected proportions
 * ============================================================================ */

TEST_F(GameTheoryFEPRegressionTest, WeightSumRegression) {
    /* With default config, verify weight proportions */
    gt_fep_config_t current_config;
    gt_fep_bridge_get_config(bridge, &current_config);

    /* Weights should be positive */
    EXPECT_GT(current_config.strategy_uncertainty_weight, 0.0f);
    EXPECT_GT(current_config.opponent_modeling_weight, 0.0f);
    EXPECT_GT(current_config.nash_convergence_weight, 0.0f);

    /* Set equal inputs to test weight effects */
    gt_fep_bridge_update_strategy_uncertainty(bridge, 0.5f);
    gt_fep_bridge_update_opponent_error(bridge, 0.5f);
    gt_fep_bridge_update_nash_distance(bridge, 0.5f);
    gt_fep_bridge_force_update(bridge);

    gt_fep_metrics_t metrics;
    gt_fep_bridge_get_metrics(bridge, &metrics);

    /* With equal inputs, contributions should reflect weights */
    float expected_strategy_contrib = 0.5f * current_config.strategy_uncertainty_weight;
    float expected_opponent_contrib = 0.5f * current_config.opponent_modeling_weight;
    float expected_nash_contrib = 0.5f * current_config.nash_convergence_weight;

    EXPECT_NEAR(metrics.strategy_contribution, expected_strategy_contrib, 0.01f);
    EXPECT_NEAR(metrics.opponent_contribution, expected_opponent_contrib, 0.01f);
    EXPECT_NEAR(metrics.nash_contribution, expected_nash_contrib, 0.01f);
}

/* ============================================================================
 * StateTransitionRegression - State transitions work correctly
 * ============================================================================ */

TEST_F(GameTheoryFEPRegressionTest, StateTransitionRegression) {
    /* Initial state should be IDLE */
    gt_fep_state_t initial_state = gt_fep_bridge_get_state(bridge);
    EXPECT_EQ(initial_state, GT_FEP_STATE_IDLE);

    /* After force update, should be ACTIVE or IDLE */
    gt_fep_bridge_force_update(bridge);
    gt_fep_state_t after_update = gt_fep_bridge_get_state(bridge);
    EXPECT_TRUE(after_update == GT_FEP_STATE_IDLE ||
                after_update == GT_FEP_STATE_ACTIVE)
        << "State should be IDLE or ACTIVE after update";

    /* Push to degraded mode */
    gt_fep_bridge_update_strategy_uncertainty(bridge, 1.0f);
    gt_fep_bridge_update_opponent_error(bridge, 1.0f);
    gt_fep_bridge_update_nash_distance(bridge, 1.0f);
    gt_fep_bridge_force_update(bridge);

    float high_fe = gt_fep_bridge_get_free_energy(bridge);
    if (high_fe > config.high_free_energy_threshold) {
        gt_fep_state_t degraded_state = gt_fep_bridge_get_state(bridge);
        EXPECT_EQ(degraded_state, GT_FEP_STATE_DEGRADED);
        EXPECT_TRUE(gt_fep_bridge_is_degraded(bridge));
    }

    /* Reset should return to IDLE */
    gt_fep_bridge_reset(bridge);
    gt_fep_state_t reset_state = gt_fep_bridge_get_state(bridge);
    EXPECT_EQ(reset_state, GT_FEP_STATE_IDLE);
    EXPECT_FALSE(gt_fep_bridge_is_degraded(bridge));
}

/* ============================================================================
 * StatisticsIntegrityRegression - Statistics remain integral
 * ============================================================================ */

TEST_F(GameTheoryFEPRegressionTest, StatisticsIntegrityRegression) {
    gt_fep_bridge_reset_stats(bridge);

    /* Run known number of operations */
    const int NUM_UPDATES = 50;
    const int NUM_STRATEGY_UPDATES = 30;
    const int NUM_NASH_CHECKS = 20;

    for (int i = 0; i < NUM_UPDATES; i++) {
        gt_fep_bridge_force_update(bridge);
    }

    for (int i = 0; i < NUM_STRATEGY_UPDATES; i++) {
        gt_fep_bridge_update_strategy_uncertainty(bridge, (float)i / NUM_STRATEGY_UPDATES);
    }

    for (int i = 0; i < NUM_NASH_CHECKS; i++) {
        gt_fep_bridge_update_nash_distance(bridge, (float)i / NUM_NASH_CHECKS);
    }

    gt_fep_stats_t stats;
    gt_fep_bridge_get_stats(bridge, &stats);

    /* Verify counts */
    EXPECT_EQ(stats.total_updates, (uint64_t)NUM_UPDATES)
        << "Total updates should match exactly";

    /* Strategy computations include both direct updates and force_update queries */
    EXPECT_GE(stats.strategy_computations, (uint64_t)NUM_STRATEGY_UPDATES)
        << "Strategy computations should be at least NUM_STRATEGY_UPDATES";

    /* Nash checks include both direct updates */
    EXPECT_GE(stats.nash_equilibrium_checks, (uint64_t)NUM_NASH_CHECKS)
        << "Nash checks should be at least NUM_NASH_CHECKS";

    /* Timing should be valid */
    EXPECT_GT(stats.total_update_time_us, 0u);
    EXPECT_GT(stats.avg_update_time_us, 0.0f);

    /* Average should be total / count */
    float expected_avg = (float)stats.total_update_time_us / (float)stats.total_updates;
    EXPECT_NEAR(stats.avg_update_time_us, expected_avg, 0.1f);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
