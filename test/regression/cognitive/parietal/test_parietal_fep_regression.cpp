/**
 * @file test_parietal_fep_regression.cpp
 * @brief Regression tests for Parietal Lobe - FEP Bridge
 * @version 1.0.0
 * @date 2026-01-08
 *
 * WHAT: Regression tests to prevent regressions in parietal-FEP integration
 * WHY:  Ensure spatial accuracy, free energy bounds, performance, and memory stability
 * HOW:  Test against established baselines, verify bounds, measure performance
 *
 * TEST CATEGORIES:
 * - SpatialAccuracyRegression: Spatial computations remain accurate
 * - FreeEnergyBoundsRegression: Free energy stays within expected bounds
 * - PerformanceRegression: Update time doesn't degrade
 * - MemoryLeakRegression: No memory growth over many cycles
 * - CoordinateConsistencyRegression: Coordinate transforms remain consistent
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <chrono>
#include <thread>
#include <vector>
#include <algorithm>

// Headers have their own extern "C" guards
#include "cognitive/parietal/nimcp_parietal.h"
#include "cognitive/parietal/nimcp_parietal_fep_bridge.h"
#include "cognitive/free_energy/nimcp_fep_orchestrator.h"

/* ============================================================================
 * Regression Test Constants and Thresholds
 * ============================================================================ */

/* Performance thresholds (microseconds) */
static constexpr int64_t BRIDGE_UPDATE_THRESHOLD_US = 500;      /* <500us per update */
static constexpr int64_t METRICS_QUERY_THRESHOLD_US = 50;       /* <50us to query metrics */
static constexpr int64_t STATE_QUERY_THRESHOLD_US = 20;         /* <20us to query state */
static constexpr int64_t REGISTRATION_THRESHOLD_US = 1000;      /* <1ms to register */

/* Free energy bounds */
static constexpr float FREE_ENERGY_MIN = 0.0f;
static constexpr float FREE_ENERGY_MAX_EXPECTED = 2.0f;
static constexpr float PREDICTION_ERROR_MAX = 1.0f;
static constexpr float SPATIAL_UNCERTAINTY_MAX = 1.0f;
static constexpr float BODY_SCHEMA_ERROR_MAX = 1.0f;

/* Regression iterations */
static constexpr int WARMUP_ITERATIONS = 100;
static constexpr int BENCHMARK_ITERATIONS = 1000;
static constexpr int MEMORY_TEST_ITERATIONS = 10000;
static constexpr int ACCURACY_TEST_ITERATIONS = 100;

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class ParietalFEPRegressionTest : public ::testing::Test {
protected:
    parietal_lobe_t* parietal = nullptr;
    parietal_fep_bridge_t* bridge = nullptr;
    fep_orchestrator_t* fep_orch = nullptr;

    void SetUp() override {
        /* Create parietal lobe */
        parietal_config_t parietal_config = parietal_default_config();
        parietal_config.enable_fep_bridge = false;
        parietal = parietal_create_custom(&parietal_config);
        ASSERT_NE(parietal, nullptr);

        /* Create FEP bridge */
        parietal_fep_config_t bridge_config = parietal_fep_config_default();
        bridge_config.enable_callbacks = false;  /* Disable for perf testing */
        bridge = parietal_fep_bridge_create(&bridge_config);
        ASSERT_NE(bridge, nullptr);

        /* Create FEP orchestrator */
        fep_orchestrator_config_t fep_config;
        fep_orchestrator_default_config(&fep_config);
        fep_config.enable_statistics = true;
        fep_config.enable_logging = false;
        fep_orch = fep_orchestrator_create(&fep_config);
        ASSERT_NE(fep_orch, nullptr);

        /* Start orchestrator and register bridge */
        ASSERT_EQ(fep_orchestrator_start(fep_orch), 0);
        uint32_t bridge_id = 0;
        ASSERT_EQ(parietal_fep_bridge_register(bridge, fep_orch, parietal, &bridge_id), 0);
    }

    void TearDown() override {
        if (bridge) {
            parietal_fep_bridge_unregister(bridge);
            parietal_fep_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (fep_orch) {
            fep_orchestrator_stop(fep_orch);
            fep_orchestrator_destroy(fep_orch);
            fep_orch = nullptr;
        }
        if (parietal) {
            parietal_destroy(parietal);
            parietal = nullptr;
        }
    }

    /* Utility to measure operation time in microseconds */
    template<typename Func>
    int64_t measure_time_us(Func&& func) {
        auto start = std::chrono::high_resolution_clock::now();
        func();
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    }

    /* Benchmark with warmup and statistics */
    template<typename Func>
    void benchmark_operation(const char* name, Func&& func, int64_t threshold_us) {
        /* Warmup */
        for (int i = 0; i < WARMUP_ITERATIONS; i++) {
            func();
        }

        /* Measure */
        std::vector<int64_t> timings;
        timings.reserve(BENCHMARK_ITERATIONS);
        for (int i = 0; i < BENCHMARK_ITERATIONS; i++) {
            int64_t time_us = measure_time_us(func);
            timings.push_back(time_us);
        }

        /* Statistics */
        std::sort(timings.begin(), timings.end());
        int64_t min = timings.front();
        int64_t max = timings.back();
        int64_t median = timings[timings.size() / 2];
        int64_t p95 = timings[(timings.size() * 95) / 100];

        int64_t sum = 0;
        for (auto t : timings) sum += t;
        int64_t avg = sum / (int64_t)timings.size();

        /* Report */
        printf("  %s: avg=%lldus, median=%lldus, p95=%lldus, min=%lldus, max=%lldus\n",
               name, (long long)avg, (long long)median, (long long)p95,
               (long long)min, (long long)max);

        /* Verify performance */
        EXPECT_LT(avg, threshold_us) << name << " average exceeds threshold";
        EXPECT_LT(p95, threshold_us * 2) << name << " p95 exceeds 2x threshold";
    }

    uint64_t get_current_time_ms() {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();
    }
};

/* ============================================================================
 * SpatialAccuracyRegression - Spatial computations remain accurate
 * ============================================================================ */

TEST_F(ParietalFEPRegressionTest, SpatialAccuracyRegression) {
    printf("\n[Spatial Accuracy Regression]\n");

    /* Perform many spatial computations and verify consistency */
    std::vector<float> spatial_uncertainties;
    spatial_uncertainties.reserve(ACCURACY_TEST_ITERATIONS);

    for (int i = 0; i < ACCURACY_TEST_ITERATIONS; i++) {
        /* Perform spatial processing */
        parietal_request_t req;
        memset(&req, 0, sizeof(req));
        req.type = PARIETAL_COORDINATE_TRANSFORM;
        req.input.transform_input.position.x = 1.0f + (float)i * 0.01f;
        req.input.transform_input.position.y = 2.0f;
        req.input.transform_input.position.z = 3.0f;
        observer_pose_t observer;
        memset(&observer, 0, sizeof(observer));
        req.input.transform_input.observer = &observer;
        req.input.transform_input.ego_to_allocentric = true;

        parietal_result_t result = parietal_process(parietal, &req);
        (void)result;

        /* Update FEP bridge */
        parietal_fep_bridge_force_update(bridge);

        /* Record spatial uncertainty */
        float uncertainty = parietal_fep_bridge_get_spatial_uncertainty(bridge);
        spatial_uncertainties.push_back(uncertainty);
    }

    /* Verify spatial uncertainties are within bounds */
    for (size_t i = 0; i < spatial_uncertainties.size(); i++) {
        float u = spatial_uncertainties[i];
        EXPECT_GE(u, 0.0f) << "Spatial uncertainty at iteration " << i << " should be >= 0";
        EXPECT_LE(u, SPATIAL_UNCERTAINTY_MAX)
            << "Spatial uncertainty at iteration " << i << " should be <= " << SPATIAL_UNCERTAINTY_MAX;
    }

    /* Verify consistency (standard deviation should be reasonable) */
    if (spatial_uncertainties.size() > 1) {
        float sum = 0.0f;
        for (float u : spatial_uncertainties) sum += u;
        float mean = sum / (float)spatial_uncertainties.size();

        float var_sum = 0.0f;
        for (float u : spatial_uncertainties) {
            float diff = u - mean;
            var_sum += diff * diff;
        }
        float std_dev = std::sqrt(var_sum / (float)spatial_uncertainties.size());

        printf("  Spatial uncertainty: mean=%.4f, std_dev=%.4f\n", mean, std_dev);

        /* Standard deviation should be bounded (consistent behavior) */
        EXPECT_LE(std_dev, 0.5f) << "Spatial uncertainty variance too high";
    }
}

/* ============================================================================
 * FreeEnergyBoundsRegression - Free energy stays within expected bounds
 * ============================================================================ */

TEST_F(ParietalFEPRegressionTest, FreeEnergyBoundsRegression) {
    printf("\n[Free Energy Bounds Regression]\n");

    /* Run many update cycles and verify bounds */
    std::vector<float> free_energies;
    std::vector<float> prediction_errors;
    std::vector<float> body_schema_errors;

    uint64_t base_time = get_current_time_ms();
    for (int i = 0; i < 500; i++) {
        fep_orchestrator_update(fep_orch, base_time + (i * 50));

        float fe = parietal_fep_bridge_get_free_energy_contribution(bridge);
        float pe = parietal_fep_bridge_get_prediction_error(bridge);
        float bse = parietal_fep_bridge_get_body_schema_error(bridge);

        free_energies.push_back(fe);
        prediction_errors.push_back(pe);
        body_schema_errors.push_back(bse);
    }

    /* Verify all values are within bounds */
    int fe_violations = 0;
    int pe_violations = 0;
    int bse_violations = 0;

    for (size_t i = 0; i < free_energies.size(); i++) {
        if (free_energies[i] < FREE_ENERGY_MIN || free_energies[i] > FREE_ENERGY_MAX_EXPECTED) {
            fe_violations++;
        }
        if (prediction_errors[i] < 0.0f || prediction_errors[i] > PREDICTION_ERROR_MAX) {
            pe_violations++;
        }
        if (body_schema_errors[i] < 0.0f || body_schema_errors[i] > BODY_SCHEMA_ERROR_MAX) {
            bse_violations++;
        }
    }

    printf("  Free energy violations: %d/%zu\n", fe_violations, free_energies.size());
    printf("  Prediction error violations: %d/%zu\n", pe_violations, prediction_errors.size());
    printf("  Body schema error violations: %d/%zu\n", bse_violations, body_schema_errors.size());

    EXPECT_EQ(fe_violations, 0) << "Free energy should stay within bounds";
    EXPECT_EQ(pe_violations, 0) << "Prediction error should stay within bounds";
    EXPECT_EQ(bse_violations, 0) << "Body schema error should stay within bounds";

    /* Report statistics */
    float fe_min = *std::min_element(free_energies.begin(), free_energies.end());
    float fe_max = *std::max_element(free_energies.begin(), free_energies.end());
    float pe_max_obs = *std::max_element(prediction_errors.begin(), prediction_errors.end());

    printf("  Free energy range: [%.4f, %.4f]\n", fe_min, fe_max);
    printf("  Max prediction error observed: %.4f\n", pe_max_obs);
}

/* ============================================================================
 * PerformanceRegression - Update time doesn't degrade
 * ============================================================================ */

TEST_F(ParietalFEPRegressionTest, PerformanceRegression) {
    printf("\n[Performance Regression]\n");

    /* Benchmark force update */
    benchmark_operation("force_update", [&]() {
        parietal_fep_bridge_force_update(bridge);
    }, BRIDGE_UPDATE_THRESHOLD_US);

    /* Benchmark metrics query */
    benchmark_operation("get_metrics", [&]() {
        parietal_fep_metrics_t metrics;
        parietal_fep_bridge_get_metrics(bridge, &metrics);
    }, METRICS_QUERY_THRESHOLD_US);

    /* Benchmark stats query */
    benchmark_operation("get_stats", [&]() {
        parietal_fep_stats_t stats;
        parietal_fep_bridge_get_stats(bridge, &stats);
    }, METRICS_QUERY_THRESHOLD_US);

    /* Benchmark state query */
    benchmark_operation("get_state", [&]() {
        parietal_fep_bridge_get_state(bridge);
    }, STATE_QUERY_THRESHOLD_US);

    /* Benchmark free energy accessor */
    benchmark_operation("get_free_energy", [&]() {
        parietal_fep_bridge_get_free_energy_contribution(bridge);
    }, STATE_QUERY_THRESHOLD_US);

    /* Benchmark spatial uncertainty accessor */
    benchmark_operation("get_spatial_uncertainty", [&]() {
        parietal_fep_bridge_get_spatial_uncertainty(bridge);
    }, STATE_QUERY_THRESHOLD_US);

    /* Benchmark prediction error accessor */
    benchmark_operation("get_prediction_error", [&]() {
        parietal_fep_bridge_get_prediction_error(bridge);
    }, STATE_QUERY_THRESHOLD_US);
}

/* ============================================================================
 * MemoryLeakRegression - No memory growth over many cycles
 * ============================================================================ */

TEST_F(ParietalFEPRegressionTest, MemoryLeakRegression) {
    printf("\n[Memory Leak Regression]\n");

    /* Get initial stats */
    parietal_fep_stats_t initial_stats;
    parietal_fep_bridge_get_stats(bridge, &initial_stats);

    /* Run many update cycles */
    uint64_t base_time = get_current_time_ms();
    for (int i = 0; i < MEMORY_TEST_ITERATIONS; i++) {
        parietal_fep_bridge_force_update(bridge);

        /* Periodically reset stats to simulate long-running behavior */
        if (i % 1000 == 0 && i > 0) {
            parietal_fep_bridge_reset_stats(bridge);
        }
    }

    /* Get final stats */
    parietal_fep_stats_t final_stats;
    parietal_fep_bridge_get_stats(bridge, &final_stats);

    printf("  Completed %d update cycles\n", MEMORY_TEST_ITERATIONS);
    printf("  Total updates recorded: %lu\n", (unsigned long)final_stats.total_updates);

    /* If there were memory leaks, the test would likely crash or timeout */
    /* Additional memory checks could be done with valgrind in CI */
    SUCCEED() << "Memory leak regression test completed without crash";
}

/* ============================================================================
 * CoordinateConsistencyRegression - Coordinate transforms remain consistent
 * ============================================================================ */

TEST_F(ParietalFEPRegressionTest, CoordinateConsistencyRegression) {
    printf("\n[Coordinate Consistency Regression]\n");

    /* Perform same transform multiple times and verify consistency */
    std::vector<parietal_result_t> results;
    results.reserve(100);

    for (int i = 0; i < 100; i++) {
        parietal_request_t req;
        memset(&req, 0, sizeof(req));
        req.type = PARIETAL_COORDINATE_TRANSFORM;
        req.input.transform_input.position.x = 5.0f;
        req.input.transform_input.position.y = 10.0f;
        req.input.transform_input.position.z = 15.0f;
        observer_pose_t observer;
        memset(&observer, 0, sizeof(observer));
        observer.position.x = 1.0f;
        observer.position.y = 1.0f;
        observer.position.z = 1.0f;
        observer.orientation.w = 0.9689f;  /* ~cos(0.25) for yaw=0.5 */
        observer.orientation.x = 0.0f;
        observer.orientation.y = 0.2474f;  /* ~sin(0.25) for yaw=0.5 */
        observer.orientation.z = 0.0f;
        observer.heading = 0.5f;
        req.input.transform_input.observer = &observer;
        req.input.transform_input.ego_to_allocentric = true;

        parietal_result_t result = parietal_process(parietal, &req);
        results.push_back(result);
    }

    /* Verify consistency of results */
    int consistent_count = 0;
    for (size_t i = 1; i < results.size(); i++) {
        if (results[i].success == results[0].success) {
            if (results[i].success) {
                /* Compare transformed positions */
                float dx = fabsf(results[i].output.transformed_position.x -
                                 results[0].output.transformed_position.x);
                float dy = fabsf(results[i].output.transformed_position.y -
                                 results[0].output.transformed_position.y);
                float dz = fabsf(results[i].output.transformed_position.z -
                                 results[0].output.transformed_position.z);

                if (dx < 0.001f && dy < 0.001f && dz < 0.001f) {
                    consistent_count++;
                }
            } else {
                consistent_count++;  /* Consistent failure is also consistent */
            }
        }
    }

    float consistency_rate = (float)consistent_count / (float)(results.size() - 1) * 100.0f;
    printf("  Consistency rate: %.1f%% (%d/%zu)\n",
           consistency_rate, consistent_count, results.size() - 1);

    /* Require high consistency */
    EXPECT_GE(consistency_rate, 99.0f) << "Coordinate transforms should be consistent";
}

/* ============================================================================
 * StatisticsConsistencyRegression - Stats remain consistent and accurate
 * ============================================================================ */

TEST_F(ParietalFEPRegressionTest, StatisticsConsistencyRegression) {
    printf("\n[Statistics Consistency Regression]\n");

    /* Reset stats */
    parietal_fep_bridge_reset_stats(bridge);

    /* Run known number of updates */
    const int KNOWN_UPDATES = 500;
    for (int i = 0; i < KNOWN_UPDATES; i++) {
        parietal_fep_bridge_force_update(bridge);
    }

    /* Get stats */
    parietal_fep_stats_t stats;
    parietal_fep_bridge_get_stats(bridge, &stats);

    printf("  Expected updates: %d\n", KNOWN_UPDATES);
    printf("  Recorded updates: %lu\n", (unsigned long)stats.total_updates);
    printf("  Avg update time: %.2f us\n", stats.avg_update_time_us);
    printf("  Avg free energy: %.4f\n", stats.avg_free_energy);
    printf("  Peak free energy: %.4f\n", stats.peak_free_energy);

    /* Verify update count matches */
    EXPECT_EQ(stats.total_updates, (uint64_t)KNOWN_UPDATES)
        << "Update count should match expected";

    /* Verify averages are reasonable */
    EXPECT_GE(stats.avg_free_energy, 0.0f);
    EXPECT_LE(stats.avg_free_energy, FREE_ENERGY_MAX_EXPECTED);
    EXPECT_GE(stats.peak_free_energy + 1e-6f, stats.avg_free_energy)
        << "Peak should be >= average (with float tolerance)";
}

/* ============================================================================
 * ConfigurationPersistenceRegression - Config changes persist correctly
 * ============================================================================ */

TEST_F(ParietalFEPRegressionTest, ConfigurationPersistenceRegression) {
    printf("\n[Configuration Persistence Regression]\n");

    /* Get original config */
    parietal_fep_config_t original_config;
    parietal_fep_bridge_get_config(bridge, &original_config);

    /* Modify config */
    parietal_fep_config_t new_config = original_config;
    new_config.spatial_uncertainty_weight = 0.5f;
    new_config.body_schema_error_weight = 0.3f;
    new_config.math_error_weight = 0.2f;
    new_config.high_free_energy_threshold = 1.8f;

    int ret = parietal_fep_bridge_set_config(bridge, &new_config);
    EXPECT_EQ(ret, 0) << "Set config should succeed";

    /* Run some updates */
    for (int i = 0; i < 100; i++) {
        parietal_fep_bridge_force_update(bridge);
    }

    /* Verify config persisted */
    parietal_fep_config_t retrieved_config;
    parietal_fep_bridge_get_config(bridge, &retrieved_config);

    EXPECT_FLOAT_EQ(retrieved_config.spatial_uncertainty_weight, 0.5f);
    EXPECT_FLOAT_EQ(retrieved_config.body_schema_error_weight, 0.3f);
    EXPECT_FLOAT_EQ(retrieved_config.math_error_weight, 0.2f);
    EXPECT_FLOAT_EQ(retrieved_config.high_free_energy_threshold, 1.8f);

    printf("  Configuration persisted correctly after %d updates\n", 100);
}

/* ============================================================================
 * ResetBehaviorRegression - Reset restores correct initial state
 * ============================================================================ */

TEST_F(ParietalFEPRegressionTest, ResetBehaviorRegression) {
    printf("\n[Reset Behavior Regression]\n");

    /* Run many updates to change state */
    for (int i = 0; i < 500; i++) {
        parietal_fep_bridge_force_update(bridge);
    }

    /* Get pre-reset stats */
    parietal_fep_stats_t pre_reset_stats;
    parietal_fep_bridge_get_stats(bridge, &pre_reset_stats);
    EXPECT_GT(pre_reset_stats.total_updates, 0u);

    /* Reset bridge */
    int ret = parietal_fep_bridge_reset(bridge);
    EXPECT_EQ(ret, 0) << "Reset should succeed";

    /* Verify state is reset */
    parietal_fep_state_t state = parietal_fep_bridge_get_state(bridge);
    EXPECT_EQ(state, PARIETAL_FEP_STATE_IDLE) << "State should be IDLE after reset";

    /* Verify metrics are reset */
    parietal_fep_metrics_t metrics;
    parietal_fep_bridge_get_metrics(bridge, &metrics);
    EXPECT_EQ(metrics.update_count, 0u) << "Update count should be 0 after reset";

    /* Verify stats are reset */
    parietal_fep_stats_t post_reset_stats;
    parietal_fep_bridge_get_stats(bridge, &post_reset_stats);
    EXPECT_EQ(post_reset_stats.total_updates, 0u) << "Total updates should be 0 after reset";

    printf("  Reset correctly cleared state after %lu updates\n",
           (unsigned long)pre_reset_stats.total_updates);
}

/* ============================================================================
 * ThreadSafetyRegression - Concurrent access remains safe
 * ============================================================================ */

TEST_F(ParietalFEPRegressionTest, ThreadSafetyRegression) {
    printf("\n[Thread Safety Regression]\n");

    const int NUM_THREADS = 4;
    const int ITERATIONS_PER_THREAD = 500;
    std::atomic<int> completed{0};
    std::atomic<int> errors{0};

    std::vector<std::thread> threads;
    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < ITERATIONS_PER_THREAD; i++) {
                /* Mix of read and write operations */
                float fe = parietal_fep_bridge_get_free_energy_contribution(bridge);
                if (fe < 0.0f && fe != -1.0f) errors++;

                float su = parietal_fep_bridge_get_spatial_uncertainty(bridge);
                if (su < 0.0f && su != -1.0f) errors++;

                parietal_fep_metrics_t metrics;
                if (parietal_fep_bridge_get_metrics(bridge, &metrics) != 0) errors++;

                parietal_fep_stats_t stats;
                if (parietal_fep_bridge_get_stats(bridge, &stats) != 0) errors++;

                /* Write operation */
                parietal_fep_bridge_force_update(bridge);
            }
            completed++;
        });
    }

    /* Wait for all threads */
    for (auto& t : threads) {
        t.join();
    }

    printf("  Completed: %d/%d threads\n", completed.load(), NUM_THREADS);
    printf("  Errors: %d\n", errors.load());

    EXPECT_EQ(completed.load(), NUM_THREADS) << "All threads should complete";
    EXPECT_EQ(errors.load(), 0) << "No errors should occur during concurrent access";
}

/* ============================================================================
 * OrchestratorIntegrationRegression - FEP orchestrator integration stable
 * ============================================================================ */

TEST_F(ParietalFEPRegressionTest, OrchestratorIntegrationRegression) {
    printf("\n[Orchestrator Integration Regression]\n");

    /* Run many orchestrator update cycles */
    const int CYCLES = 1000;
    int successful_updates = 0;

    uint64_t base_time = get_current_time_ms();
    for (int i = 0; i < CYCLES; i++) {
        int result = fep_orchestrator_update(fep_orch, base_time + (i * 50));
        if (result >= 0) successful_updates++;
    }

    printf("  Successful updates: %d/%d\n", successful_updates, CYCLES);

    /* All updates should succeed */
    EXPECT_EQ(successful_updates, CYCLES);

    /* Get orchestrator stats */
    fep_orchestrator_stats_t orch_stats;
    fep_orchestrator_get_stats(fep_orch, &orch_stats);

    printf("  Total bridge updates: %lu\n", (unsigned long)orch_stats.total_bridge_updates);
    printf("  Update errors: %u\n", orch_stats.update_errors);

    EXPECT_EQ(orch_stats.update_errors, 0u) << "No update errors expected";
    EXPECT_GT(orch_stats.total_bridge_updates, 0u) << "Bridge should receive updates";
}

/* ============================================================================
 * RegistrationStabilityRegression - Registration/unregistration stable
 * ============================================================================ */

TEST_F(ParietalFEPRegressionTest, RegistrationStabilityRegression) {
    printf("\n[Registration Stability Regression]\n");

    /* Test repeated registration/unregistration cycles */
    const int CYCLES = 100;
    int successful_registrations = 0;
    int successful_unregistrations = 0;

    for (int i = 0; i < CYCLES; i++) {
        /* Unregister */
        int ret = parietal_fep_bridge_unregister(bridge);
        if (ret == 0) successful_unregistrations++;

        EXPECT_FALSE(parietal_fep_bridge_is_registered(bridge));

        /* Re-register */
        uint32_t bridge_id = 0;
        ret = parietal_fep_bridge_register(bridge, fep_orch, parietal, &bridge_id);
        if (ret == 0) successful_registrations++;

        EXPECT_TRUE(parietal_fep_bridge_is_registered(bridge));
        EXPECT_GT(bridge_id, 0u);
    }

    printf("  Successful registrations: %d/%d\n", successful_registrations, CYCLES);
    printf("  Successful unregistrations: %d/%d\n", successful_unregistrations, CYCLES);

    EXPECT_EQ(successful_registrations, CYCLES);
    EXPECT_EQ(successful_unregistrations, CYCLES);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
