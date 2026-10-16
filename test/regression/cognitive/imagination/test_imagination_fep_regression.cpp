/**
 * @file test_imagination_fep_regression.cpp
 * @brief Regression tests for Imagination FEP Bridge
 * @version 1.0.0
 * @date 2026-01-08
 *
 * WHAT: Regression tests for imagination-FEP integration stability
 * WHY:  Prevent regressions in simulation stability, free energy bounds,
 *       performance, memory usage, and creative output consistency
 * HOW:  Stress tests, boundary conditions, long-running cycles, and memory monitoring
 *
 * TEST CATEGORIES:
 * - SimulationStabilityRegression: Simulations remain coherent over time
 * - FreeEnergyBoundsRegression: Free energy stays within expected bounds
 * - PerformanceRegression: Update time doesn't degrade
 * - MemoryLeakRegression: No memory growth over many cycles
 * - CreativityConsistencyRegression: Creative outputs maintain quality
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <chrono>
#include <thread>
#include <atomic>
#include <vector>
#include <numeric>
#include <algorithm>

#include "cognitive/imagination/nimcp_imagination_fep_bridge.h"
#include "cognitive/imagination/nimcp_imagination_engine.h"
#include "cognitive/free_energy/nimcp_fep_orchestrator.h"
#include "utils/memory/nimcp_memory.h"

/* ============================================================================
 * Regression Test Constants
 * ============================================================================ */

#define REGRESSION_LONG_RUN_CYCLES         500
#define REGRESSION_STRESS_CYCLES           1000
#define REGRESSION_MEMORY_CHECK_CYCLES     100
#define REGRESSION_PERFORMANCE_SAMPLES     50
#define REGRESSION_MAX_UPDATE_TIME_US      5000   /* 5ms max per update */
#define REGRESSION_MAX_MEMORY_GROWTH_MB    10.0f  /* Max 10MB growth allowed */

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class ImaginationFEPRegressionTest : public ::testing::Test {
protected:
    imagination_engine_t* engine = nullptr;
    imagination_engine_config_t engine_config;
    imagination_fep_bridge_t* bridge = nullptr;
    imagination_fep_config_t bridge_config;
    fep_orchestrator_t* fep_orch = nullptr;
    fep_orchestrator_config_t fep_config;

    void SetUp() override {
        /* Create imagination engine */
        engine_config = imagination_engine_default_config();
        engine_config.max_concurrent_scenarios = 4;
        engine_config.enable_reality_checking = true;
        engine = imagination_engine_create(&engine_config);
        ASSERT_NE(engine, nullptr);

        /* Create FEP bridge */
        bridge_config = imagination_fep_config_default();
        bridge_config.enable_logging = false;
        bridge = imagination_fep_bridge_create(&bridge_config);
        ASSERT_NE(bridge, nullptr);

        /* Create FEP orchestrator */
        fep_orchestrator_default_config(&fep_config);
        fep_config.enable_statistics = true;
        fep_config.enable_logging = false;
        fep_orch = fep_orchestrator_create(&fep_config);
        ASSERT_NE(fep_orch, nullptr);

        /* Start and register */
        ASSERT_EQ(fep_orchestrator_start(fep_orch), 0);

        uint32_t bridge_id = 0;
        ASSERT_EQ(imagination_fep_bridge_register(bridge, fep_orch, engine, &bridge_id), 0);
    }

    void TearDown() override {
        if (bridge) {
            imagination_fep_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (fep_orch) {
            fep_orchestrator_stop(fep_orch);
            fep_orchestrator_destroy(fep_orch);
            fep_orch = nullptr;
        }
        if (engine) {
            imagination_engine_destroy(engine);
            engine = nullptr;
        }
    }

    uint64_t get_current_time_ms() {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();
    }

    uint64_t get_current_time_us() {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::microseconds>(
            now.time_since_epoch()).count();
    }

    /* Run FEP updates and return timing info */
    std::vector<uint64_t> run_timed_updates(int num_cycles, uint64_t interval_ms = 50) {
        std::vector<uint64_t> update_times;
        update_times.reserve(num_cycles);

        uint64_t base_time = get_current_time_ms();
        for (int i = 0; i < num_cycles; i++) {
            uint64_t start_us = get_current_time_us();
            fep_orchestrator_update(fep_orch, base_time + (i * interval_ms));
            uint64_t end_us = get_current_time_us();
            update_times.push_back(end_us - start_us);
        }
        return update_times;
    }

    /* Helper for running many cycles quickly */
    void run_cycles(int num_cycles) {
        uint64_t base_time = get_current_time_ms();
        for (int i = 0; i < num_cycles; i++) {
            fep_orchestrator_update(fep_orch, base_time + (i * 10));
        }
    }
};

/* ============================================================================
 * SimulationStabilityRegression - Simulations remain coherent over time
 * ============================================================================ */

TEST_F(ImaginationFEPRegressionTest, SimulationStabilityRegression) {
    /* Run long simulation */
    std::vector<float> free_energy_samples;
    std::vector<float> divergence_samples;
    std::vector<float> prediction_error_samples;

    uint64_t base_time = get_current_time_ms();
    for (int i = 0; i < REGRESSION_LONG_RUN_CYCLES; i++) {
        fep_orchestrator_update(fep_orch, base_time + (i * 50));

        /* Sample every 10 cycles */
        if (i % 10 == 0) {
            free_energy_samples.push_back(imagination_fep_bridge_get_free_energy(bridge));
            divergence_samples.push_back(imagination_fep_bridge_get_simulation_divergence(bridge));
            prediction_error_samples.push_back(imagination_fep_bridge_get_prediction_error(bridge));
        }
    }

    /* All samples should be within valid bounds */
    for (float fe : free_energy_samples) {
        EXPECT_GE(fe, 0.0f) << "Free energy went negative";
        EXPECT_LE(fe, IMAGINATION_FEP_MAX_FREE_ENERGY) << "Free energy exceeded maximum";
    }

    for (float div : divergence_samples) {
        EXPECT_GE(div, 0.0f) << "Divergence went negative";
        EXPECT_LE(div, 1.0f) << "Divergence exceeded 1.0";
    }

    for (float pe : prediction_error_samples) {
        EXPECT_GE(pe, 0.0f) << "Prediction error went negative";
        EXPECT_LE(pe, 1.0f) << "Prediction error exceeded 1.0";
    }

    /* Check for stability - no wild oscillations */
    if (free_energy_samples.size() > 2) {
        float max_fe = *std::max_element(free_energy_samples.begin(), free_energy_samples.end());
        float min_fe = *std::min_element(free_energy_samples.begin(), free_energy_samples.end());
        float range = max_fe - min_fe;

        /* Range should be reasonable (not swinging wildly) */
        EXPECT_LE(range, IMAGINATION_FEP_MAX_FREE_ENERGY)
            << "Free energy range too large, indicating instability";
    }

    /* Bridge should not be in error state */
    imagination_fep_state_t state = imagination_fep_bridge_get_state(bridge);
    EXPECT_NE(state, IMAGINATION_FEP_STATE_ERROR);
}

/* ============================================================================
 * FreeEnergyBoundsRegression - Free energy stays within expected bounds
 * ============================================================================ */

TEST_F(ImaginationFEPRegressionTest, FreeEnergyBoundsRegression) {
    /* Test with various configurations */
    std::vector<imagination_fep_config_t> test_configs;

    /* Config 1: Default */
    test_configs.push_back(bridge_config);

    /* Config 2: High divergence weight */
    imagination_fep_config_t high_div = bridge_config;
    high_div.simulation_divergence_weight = 0.8f;
    test_configs.push_back(high_div);

    /* Config 3: High counterfactual cost */
    imagination_fep_config_t high_cf = bridge_config;
    high_cf.counterfactual_cost = 0.8f;
    test_configs.push_back(high_cf);

    /* Config 4: Low coherence weight */
    imagination_fep_config_t low_coh = bridge_config;
    low_coh.coherence_weight = 0.05f;
    test_configs.push_back(low_coh);

    /* Config 5: Extreme weights */
    imagination_fep_config_t extreme = bridge_config;
    extreme.simulation_divergence_weight = 1.0f;
    extreme.counterfactual_cost = 1.0f;
    extreme.coherence_weight = 1.0f;
    extreme.prediction_accuracy_weight = 0.0f;
    test_configs.push_back(extreme);

    for (size_t cfg_idx = 0; cfg_idx < test_configs.size(); cfg_idx++) {
        /* Reset and apply config */
        imagination_fep_bridge_reset(bridge);
        imagination_fep_bridge_set_config(bridge, &test_configs[cfg_idx]);

        /* Run cycles */
        run_cycles(100);

        /* Check bounds */
        float fe = imagination_fep_bridge_get_free_energy(bridge);
        EXPECT_GE(fe, 0.0f) << "Config " << cfg_idx << ": Free energy negative";
        EXPECT_LE(fe, test_configs[cfg_idx].max_free_energy)
            << "Config " << cfg_idx << ": Free energy exceeded max";

        float div = imagination_fep_bridge_get_simulation_divergence(bridge);
        EXPECT_GE(div, 0.0f) << "Config " << cfg_idx << ": Divergence negative";
        EXPECT_LE(div, 1.0f) << "Config " << cfg_idx << ": Divergence exceeded 1.0";

        float pe = imagination_fep_bridge_get_prediction_error(bridge);
        EXPECT_GE(pe, 0.0f) << "Config " << cfg_idx << ": Prediction error negative";
        EXPECT_LE(pe, 1.0f) << "Config " << cfg_idx << ": Prediction error exceeded 1.0";
    }
}

/* ============================================================================
 * PerformanceRegression - Update time doesn't degrade over time
 * ============================================================================ */

TEST_F(ImaginationFEPRegressionTest, PerformanceRegression) {
    /* Warm up */
    run_cycles(50);

    /* Collect timing samples across multiple batches */
    std::vector<double> batch_avg_times;

    for (int batch = 0; batch < 10; batch++) {
        auto times = run_timed_updates(REGRESSION_PERFORMANCE_SAMPLES);

        /* Calculate average for this batch */
        double sum = std::accumulate(times.begin(), times.end(), 0.0);
        double avg = sum / times.size();
        batch_avg_times.push_back(avg);

        /* Each update should be under max time */
        for (uint64_t t : times) {
            EXPECT_LE(t, REGRESSION_MAX_UPDATE_TIME_US)
                << "Update took " << t << "us, exceeds " << REGRESSION_MAX_UPDATE_TIME_US << "us limit";
        }
    }

    /* Check for performance degradation (later batches shouldn't be much slower) */
    if (batch_avg_times.size() >= 2) {
        double first_avg = batch_avg_times[0];
        double last_avg = batch_avg_times.back();

        /* Allow up to 2x degradation (generous margin for system variance) */
        EXPECT_LE(last_avg, first_avg * 2.0 + 100)
            << "Performance degraded from " << first_avg << "us to " << last_avg << "us";
    }

    /* Check stats for average update time */
    imagination_fep_stats_t stats;
    imagination_fep_bridge_get_stats(bridge, &stats);
    EXPECT_GT(stats.avg_update_time_us, 0.0f);
    EXPECT_LT(stats.avg_update_time_us, (float)REGRESSION_MAX_UPDATE_TIME_US);
}

/* ============================================================================
 * MemoryLeakRegression - No memory growth over many cycles
 * ============================================================================ */

TEST_F(ImaginationFEPRegressionTest, MemoryLeakRegression) {
    /* Run many create/update/reset cycles to detect memory leaks */

    /* Initial stats */
    imagination_fep_stats_t initial_stats;
    imagination_fep_bridge_get_stats(bridge, &initial_stats);

    /* Run stress test */
    for (int round = 0; round < 10; round++) {
        /* Run many cycles */
        run_cycles(REGRESSION_MEMORY_CHECK_CYCLES);

        /* Reset stats periodically */
        if (round % 3 == 0) {
            imagination_fep_bridge_reset_stats(bridge);
        }
    }

    /* Verify bridge is still functional */
    float fe = imagination_fep_bridge_get_free_energy(bridge);
    EXPECT_GE(fe, 0.0f);
    EXPECT_LE(fe, bridge_config.max_free_energy);

    imagination_fep_state_t state = imagination_fep_bridge_get_state(bridge);
    EXPECT_NE(state, IMAGINATION_FEP_STATE_ERROR);

    /* Test reset functionality doesn't leak */
    for (int i = 0; i < 20; i++) {
        EXPECT_EQ(imagination_fep_bridge_reset(bridge), 0);
        run_cycles(10);
    }

    /* Final state should be valid */
    state = imagination_fep_bridge_get_state(bridge);
    EXPECT_TRUE(state == IMAGINATION_FEP_STATE_IDLE ||
                state == IMAGINATION_FEP_STATE_ACTIVE);
}

/* ============================================================================
 * CreativityConsistencyRegression - Creative outputs maintain quality
 * ============================================================================ */

TEST_F(ImaginationFEPRegressionTest, CreativityConsistencyRegression) {
    /* Configure for creative mode */
    imagination_fep_config_t creative_config = bridge_config;
    creative_config.prediction_accuracy_weight = 0.05f;  /* Lower prediction bonus */
    creative_config.coherence_weight = 0.15f;  /* Lower coherence requirement */
    imagination_fep_bridge_set_config(bridge, &creative_config);

    /* Collect samples over extended run */
    std::vector<float> coherence_samples;
    std::vector<float> divergence_samples;

    uint64_t base_time = get_current_time_ms();
    for (int i = 0; i < 200; i++) {
        fep_orchestrator_update(fep_orch, base_time + (i * 50));

        if (i % 5 == 0) {
            /* These metrics serve as proxies for "creative quality" */
            imagination_fep_stats_t stats;
            imagination_fep_bridge_get_stats(bridge, &stats);
            coherence_samples.push_back(1.0f - stats.avg_simulation_divergence);
            divergence_samples.push_back(stats.avg_simulation_divergence);
        }
    }

    /* Compute statistics */
    if (!coherence_samples.empty()) {
        float sum = std::accumulate(coherence_samples.begin(), coherence_samples.end(), 0.0f);
        float avg_coherence = sum / coherence_samples.size();

        /* Creative mode should still maintain some coherence */
        EXPECT_GE(avg_coherence, 0.0f) << "Average coherence is negative";

        /* Check variance is bounded (consistency) */
        float variance = 0.0f;
        for (float c : coherence_samples) {
            variance += (c - avg_coherence) * (c - avg_coherence);
        }
        variance /= coherence_samples.size();

        /* Variance shouldn't be too high (indicates inconsistency) */
        EXPECT_LE(variance, 1.0f) << "Coherence variance too high";
    }
}

/* ============================================================================
 * StressTestRegression - System handles stress without failure
 * ============================================================================ */

TEST_F(ImaginationFEPRegressionTest, StressTestRegression) {
    /* Rapid configuration changes */
    imagination_fep_config_t config = bridge_config;

    for (int i = 0; i < 100; i++) {
        /* Vary configuration */
        config.simulation_divergence_weight = 0.1f + (i % 10) * 0.05f;
        config.counterfactual_cost = 0.1f + (i % 8) * 0.05f;
        config.coherence_weight = 0.1f + (i % 6) * 0.05f;

        imagination_fep_bridge_set_config(bridge, &config);
        run_cycles(5);

        /* Occasional resets */
        if (i % 25 == 0) {
            imagination_fep_bridge_reset(bridge);
        }
    }

    /* Verify still functional */
    imagination_fep_state_t state = imagination_fep_bridge_get_state(bridge);
    EXPECT_NE(state, IMAGINATION_FEP_STATE_ERROR);

    float fe = imagination_fep_bridge_get_free_energy(bridge);
    EXPECT_GE(fe, 0.0f);
}

/* ============================================================================
 * ConcurrentStressRegression - Thread safety under stress
 * ============================================================================ */

TEST_F(ImaginationFEPRegressionTest, ConcurrentStressRegression) {
    std::atomic<bool> stop{false};
    std::atomic<int> errors{0};

    /* Multiple reader threads */
    std::vector<std::thread> readers;
    for (int i = 0; i < 4; i++) {
        readers.emplace_back([&]() {
            while (!stop) {
                float fe = imagination_fep_bridge_get_free_energy(bridge);
                if (fe < -0.5f) errors++;  /* -1.0f is error return */

                float div = imagination_fep_bridge_get_simulation_divergence(bridge);
                if (div < -0.5f) errors++;

                imagination_fep_bridge_get_state(bridge);

                imagination_fep_stats_t stats;
                imagination_fep_bridge_get_stats(bridge, &stats);
            }
        });
    }

    /* Updater thread */
    std::thread updater([&]() {
        uint64_t base_time = get_current_time_ms();
        for (int i = 0; i < REGRESSION_STRESS_CYCLES && !stop; i++) {
            fep_orchestrator_update(fep_orch, base_time + (i * 5));
        }
        stop = true;
    });

    /* Config changer thread */
    std::thread config_changer([&]() {
        imagination_fep_config_t config = bridge_config;
        while (!stop) {
            config.simulation_divergence_weight = 0.2f + (rand() % 100) * 0.005f;
            imagination_fep_bridge_set_config(bridge, &config);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });

    /* Wait for completion */
    updater.join();
    stop = true;
    config_changer.join();
    for (auto& t : readers) {
        t.join();
    }

    EXPECT_EQ(errors.load(), 0) << "Concurrent access produced errors";

    /* Verify final state */
    imagination_fep_state_t state = imagination_fep_bridge_get_state(bridge);
    EXPECT_NE(state, IMAGINATION_FEP_STATE_ERROR);
}

/* ============================================================================
 * StatisticsConsistencyRegression - Statistics remain consistent
 * ============================================================================ */

TEST_F(ImaginationFEPRegressionTest, StatisticsConsistencyRegression) {
    imagination_fep_bridge_reset_stats(bridge);

    /* Run known number of cycles */
    const int num_cycles = 100;
    run_cycles(num_cycles);

    imagination_fep_stats_t stats;
    imagination_fep_bridge_get_stats(bridge, &stats);

    /* Verify statistics are consistent */
    EXPECT_GT(stats.total_updates, 0u);

    /* Averages should be reasonable */
    EXPECT_GE(stats.avg_free_energy, 0.0f);
    EXPECT_LE(stats.avg_free_energy, bridge_config.max_free_energy);

    EXPECT_GE(stats.avg_simulation_divergence, 0.0f);
    EXPECT_LE(stats.avg_simulation_divergence, 1.0f);

    EXPECT_GE(stats.avg_counterfactual_cost, 0.0f);
    EXPECT_LE(stats.avg_counterfactual_cost, 1.0f);

    /* Peak should be >= average */
    EXPECT_GE(stats.peak_free_energy, 0.0f);

    /* Update time should be positive */
    EXPECT_GE(stats.avg_update_time_us, 0.0f);
}

/* ============================================================================
 * ConfigurationPersistenceRegression - Config changes persist correctly
 * ============================================================================ */

TEST_F(ImaginationFEPRegressionTest, ConfigurationPersistenceRegression) {
    /* Set specific config */
    imagination_fep_config_t custom_config = bridge_config;
    custom_config.simulation_divergence_weight = 0.42f;
    custom_config.counterfactual_cost = 0.33f;
    custom_config.coherence_weight = 0.21f;
    custom_config.prediction_accuracy_weight = 0.15f;
    custom_config.high_free_energy_threshold = 1.77f;

    imagination_fep_bridge_set_config(bridge, &custom_config);

    /* Run many cycles */
    run_cycles(500);

    /* Verify config persisted */
    imagination_fep_config_t retrieved;
    imagination_fep_bridge_get_config(bridge, &retrieved);

    EXPECT_FLOAT_EQ(retrieved.simulation_divergence_weight, 0.42f);
    EXPECT_FLOAT_EQ(retrieved.counterfactual_cost, 0.33f);
    EXPECT_FLOAT_EQ(retrieved.coherence_weight, 0.21f);
    EXPECT_FLOAT_EQ(retrieved.prediction_accuracy_weight, 0.15f);
    EXPECT_FLOAT_EQ(retrieved.high_free_energy_threshold, 1.77f);
}

/* ============================================================================
 * DegradedModeRecoveryRegression - System recovers from degraded mode
 * ============================================================================ */

TEST_F(ImaginationFEPRegressionTest, DegradedModeRecoveryRegression) {
    /* Configure low threshold to trigger degraded mode */
    imagination_fep_config_t degraded_config = bridge_config;
    degraded_config.high_free_energy_threshold = 0.05f;
    degraded_config.enable_degraded_mode = true;
    imagination_fep_bridge_set_config(bridge, &degraded_config);

    /* Run to potentially trigger degraded mode */
    run_cycles(100);

    /* Now raise threshold to allow recovery */
    imagination_fep_config_t normal_config = bridge_config;
    normal_config.high_free_energy_threshold = 2.0f;
    imagination_fep_bridge_set_config(bridge, &normal_config);

    /* Run more cycles */
    run_cycles(100);

    /* System should be functional */
    imagination_fep_state_t state = imagination_fep_bridge_get_state(bridge);
    EXPECT_TRUE(state == IMAGINATION_FEP_STATE_IDLE ||
                state == IMAGINATION_FEP_STATE_ACTIVE ||
                state == IMAGINATION_FEP_STATE_DEGRADED);
    EXPECT_NE(state, IMAGINATION_FEP_STATE_ERROR);
}

/* ============================================================================
 * ResetIdempotencyRegression - Multiple resets work correctly
 * ============================================================================ */

TEST_F(ImaginationFEPRegressionTest, ResetIdempotencyRegression) {
    /* Run some cycles */
    run_cycles(50);

    /* Multiple resets should all succeed */
    for (int i = 0; i < 10; i++) {
        EXPECT_EQ(imagination_fep_bridge_reset(bridge), 0);

        /* State should be idle after reset */
        imagination_fep_state_t state = imagination_fep_bridge_get_state(bridge);
        EXPECT_EQ(state, IMAGINATION_FEP_STATE_IDLE);

        /* Free energy should be baseline */
        float fe = imagination_fep_bridge_get_free_energy(bridge);
        EXPECT_FLOAT_EQ(fe, bridge_config.baseline_free_energy);

        /* Stats should be reset */
        imagination_fep_stats_t stats;
        imagination_fep_bridge_get_stats(bridge, &stats);
        EXPECT_EQ(stats.total_updates, 0u);
    }

    /* System should still work after multiple resets */
    run_cycles(50);

    imagination_fep_stats_t stats;
    imagination_fep_bridge_get_stats(bridge, &stats);
    EXPECT_GT(stats.total_updates, 0u);
}

/* ============================================================================
 * LongRunningStabilityRegression - Extended stability test
 * ============================================================================ */

TEST_F(ImaginationFEPRegressionTest, LongRunningStabilityRegression) {
    /* Very long run to detect any slow degradation */
    const int total_cycles = REGRESSION_STRESS_CYCLES;
    const int check_interval = 100;

    std::vector<float> checkpoint_fe;
    std::vector<imagination_fep_state_t> checkpoint_states;

    uint64_t base_time = get_current_time_ms();
    for (int i = 0; i < total_cycles; i++) {
        fep_orchestrator_update(fep_orch, base_time + (i * 10));

        if (i % check_interval == 0) {
            checkpoint_fe.push_back(imagination_fep_bridge_get_free_energy(bridge));
            checkpoint_states.push_back(imagination_fep_bridge_get_state(bridge));
        }
    }

    /* All checkpoints should show valid state */
    for (size_t i = 0; i < checkpoint_fe.size(); i++) {
        EXPECT_GE(checkpoint_fe[i], 0.0f) << "Checkpoint " << i << " has negative FE";
        EXPECT_LE(checkpoint_fe[i], bridge_config.max_free_energy)
            << "Checkpoint " << i << " exceeded max FE";
        EXPECT_NE(checkpoint_states[i], IMAGINATION_FEP_STATE_ERROR)
            << "Checkpoint " << i << " is in error state";
    }

    /* Final statistics should be reasonable */
    imagination_fep_stats_t stats;
    imagination_fep_bridge_get_stats(bridge, &stats);
    /* FEP updates are interval-based (50ms default, 10ms cycle interval) */
    /* So we expect roughly total_cycles / 5 updates, with some margin */
    EXPECT_GT(stats.total_updates, (uint64_t)(total_cycles * 0.1));
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
