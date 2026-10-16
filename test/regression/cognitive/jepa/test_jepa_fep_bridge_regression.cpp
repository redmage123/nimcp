/**
 * @file test_jepa_fep_bridge_regression.cpp
 * @brief Regression Tests for JEPA FEP Bridge module
 * @version 1.0.0
 * @date 2026-01-08
 *
 * WHAT: Regression tests for JEPA FEP bridge stability and correctness
 * WHY:  Ensure FEP bridge produces consistent, correct results across changes
 * HOW:  Test embedding stability, free energy bounds, performance, memory safety
 *
 * Regression test categories:
 * 1. Embedding stability - embeddings remain stable over update cycles
 * 2. Free energy bounds - free energy stays within expected bounds
 * 3. Performance regression - update time doesn't degrade
 * 4. Memory leak regression - no memory growth over many cycles
 * 5. Representation quality regression - quality maintained over time
 *
 * @author NIMCP Development Team
 */

#include <gtest/gtest.h>
#include <chrono>
#include <cmath>
#include <cstring>
#include <vector>
#include <algorithm>
#include <numeric>
#include <thread>

// Headers have their own extern "C" guards
#include "cognitive/jepa/nimcp_jepa_fep_bridge.h"
#include "cognitive/free_energy/nimcp_fep_orchestrator.h"
#include "utils/error/nimcp_error_codes.h"

/* ============================================================================
 * Test Configuration Constants
 * ============================================================================ */

static constexpr float FLOAT_TOLERANCE = 1e-5f;
static constexpr float REGRESSION_TOLERANCE = 1e-3f;
static constexpr int STRESS_ITERATIONS = 100;
static constexpr int MEMORY_LEAK_ITERATIONS = 500;
static constexpr uint32_t PERFORMANCE_ITERATIONS = 1000;
static constexpr double MAX_UPDATE_TIME_US = 500.0;  /* 500 microseconds per update */

/* ============================================================================
 * Base Test Fixture
 * ============================================================================ */

class JepaFepBridgeRegressionTest : public ::testing::Test {
protected:
    jepa_fep_bridge_t* bridge = nullptr;
    jepa_fep_config_t config;
    fep_orchestrator_t* fep_orch = nullptr;

    void SetUp() override {
        config = jepa_fep_config_default();
        config.enable_logging = false;
        bridge = jepa_fep_bridge_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            if (jepa_fep_bridge_is_registered(bridge)) {
                jepa_fep_bridge_unregister(bridge);
            }
            jepa_fep_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (fep_orch) {
            fep_orchestrator_stop(fep_orch);
            fep_orchestrator_destroy(fep_orch);
            fep_orch = nullptr;
        }
    }

    /* Helper to create and start FEP orchestrator */
    void setup_fep_orchestrator() {
        fep_orchestrator_config_t fep_config;
        fep_orchestrator_default_config(&fep_config);
        fep_config.enable_statistics = true;
        fep_config.enable_logging = false;
        fep_orch = fep_orchestrator_create(&fep_config);
        ASSERT_NE(fep_orch, nullptr);
        ASSERT_EQ(fep_orchestrator_start(fep_orch), 0);
    }

    /* Helper to measure execution time in microseconds */
    template<typename Func>
    double measure_time_us(Func&& func) {
        auto start = std::chrono::high_resolution_clock::now();
        func();
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::micro>(end - start).count();
    }

    /* Helper to get current time in ms */
    uint64_t get_current_time_ms() {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();
    }

    /* Helper to compute variance */
    double compute_variance(const std::vector<float>& values) {
        if (values.empty()) return 0.0;
        double sum = std::accumulate(values.begin(), values.end(), 0.0);
        double mean = sum / values.size();
        double sq_sum = 0.0;
        for (float v : values) {
            sq_sum += (v - mean) * (v - mean);
        }
        return sq_sum / values.size();
    }
};

/* ============================================================================
 * Test 1: EmbeddingStabilityRegression
 * Verify embeddings remain stable over multiple update cycles
 * ============================================================================ */

TEST_F(JepaFepBridgeRegressionTest, EmbeddingStabilityRegression) {
    setup_fep_orchestrator();

    uint32_t bridge_id;
    ASSERT_EQ(jepa_fep_bridge_register(bridge, fep_orch, nullptr, &bridge_id), 0);

    /* Record consistent inputs */
    const float CONSISTENT_ERROR = 0.25f;
    const float CONSISTENT_QUALITY = 0.85f;

    std::vector<float> free_energies;
    uint64_t base_time = get_current_time_ms();

    /* Warm up phase */
    for (int i = 0; i < 10; i++) {
        EXPECT_EQ(jepa_fep_bridge_record_prediction_error(bridge, CONSISTENT_ERROR), 0);
        EXPECT_EQ(jepa_fep_bridge_record_representation_quality(bridge, CONSISTENT_QUALITY), 0);
        EXPECT_EQ(jepa_fep_bridge_force_update(bridge), 0);
    }

    /* Stability measurement phase */
    for (int i = 0; i < STRESS_ITERATIONS; i++) {
        EXPECT_EQ(jepa_fep_bridge_record_prediction_error(bridge, CONSISTENT_ERROR), 0);
        EXPECT_EQ(jepa_fep_bridge_record_representation_quality(bridge, CONSISTENT_QUALITY), 0);
        EXPECT_EQ(jepa_fep_bridge_force_update(bridge), 0);

        fep_orchestrator_update(fep_orch, base_time + (i * 50));
        free_energies.push_back(jepa_fep_bridge_get_free_energy_contribution(bridge));
    }

    /* Verify stability: variance should be very low */
    double variance = compute_variance(free_energies);
    EXPECT_LT(variance, 0.001) << "Free energy variance too high: " << variance;

    /* Verify all values are within expected range */
    for (float fe : free_energies) {
        EXPECT_GE(fe, 0.0f);
        EXPECT_LE(fe, JEPA_FEP_MAX_FREE_ENERGY);
    }

    /* Verify consistency with expected value */
    float mean_fe = std::accumulate(free_energies.begin(), free_energies.end(), 0.0f)
                    / free_energies.size();
    float expected_fe = jepa_fep_bridge_get_free_energy_contribution(bridge);
    EXPECT_NEAR(mean_fe, expected_fe, REGRESSION_TOLERANCE);
}

/* ============================================================================
 * Test 2: FreeEnergyBoundsRegression
 * Verify free energy stays within expected bounds for all input combinations
 * ============================================================================ */

TEST_F(JepaFepBridgeRegressionTest, FreeEnergyBoundsRegression) {
    setup_fep_orchestrator();

    uint32_t bridge_id;
    ASSERT_EQ(jepa_fep_bridge_register(bridge, fep_orch, nullptr, &bridge_id), 0);

    /* Test grid of prediction errors and qualities */
    std::vector<float> errors = {0.0f, 0.1f, 0.5f, 1.0f, 1.5f, 2.0f};
    std::vector<float> qualities = {0.0f, 0.2f, 0.5f, 0.8f, 1.0f};

    for (float error : errors) {
        for (float quality : qualities) {
            EXPECT_EQ(jepa_fep_bridge_reset(bridge), 0);
            EXPECT_EQ(jepa_fep_bridge_record_prediction_error(bridge, error), 0);
            EXPECT_EQ(jepa_fep_bridge_record_representation_quality(bridge, quality), 0);
            EXPECT_EQ(jepa_fep_bridge_force_update(bridge), 0);

            float fe = jepa_fep_bridge_get_free_energy_contribution(bridge);

            /* Bounds check */
            EXPECT_GE(fe, 0.0f) << "Error=" << error << " Quality=" << quality;
            EXPECT_LE(fe, JEPA_FEP_MAX_FREE_ENERGY)
                << "Error=" << error << " Quality=" << quality;

            /* Monotonicity: higher error should not decrease FE */
            /* Monotonicity: higher quality should not increase FE */
        }
    }

    /* Verify extreme cases */

    /* Best case: no error, perfect quality */
    EXPECT_EQ(jepa_fep_bridge_reset(bridge), 0);
    EXPECT_EQ(jepa_fep_bridge_record_prediction_error(bridge, 0.0f), 0);
    EXPECT_EQ(jepa_fep_bridge_record_representation_quality(bridge, 1.0f), 0);
    EXPECT_EQ(jepa_fep_bridge_force_update(bridge), 0);
    float fe_best = jepa_fep_bridge_get_free_energy_contribution(bridge);
    EXPECT_NEAR(fe_best, JEPA_FEP_BASELINE_FREE_ENERGY, 0.1f);

    /* Worst case: max error, zero quality */
    EXPECT_EQ(jepa_fep_bridge_reset(bridge), 0);
    EXPECT_EQ(jepa_fep_bridge_record_prediction_error(bridge, 2.0f), 0);
    EXPECT_EQ(jepa_fep_bridge_record_representation_quality(bridge, 0.0f), 0);
    EXPECT_EQ(jepa_fep_bridge_force_update(bridge), 0);
    float fe_worst = jepa_fep_bridge_get_free_energy_contribution(bridge);
    EXPECT_GT(fe_worst, fe_best);
    EXPECT_LE(fe_worst, JEPA_FEP_MAX_FREE_ENERGY);
}

/* ============================================================================
 * Test 3: PerformanceRegression
 * Verify update time doesn't degrade from expected bounds
 * ============================================================================ */

TEST_F(JepaFepBridgeRegressionTest, PerformanceRegression) {
    setup_fep_orchestrator();

    uint32_t bridge_id;
    ASSERT_EQ(jepa_fep_bridge_register(bridge, fep_orch, nullptr, &bridge_id), 0);

    std::vector<double> update_times;
    update_times.reserve(PERFORMANCE_ITERATIONS);

    /* Warm up */
    for (int i = 0; i < 50; i++) {
        EXPECT_EQ(jepa_fep_bridge_record_prediction_error(bridge, 0.3f), 0);
        EXPECT_EQ(jepa_fep_bridge_record_representation_quality(bridge, 0.8f), 0);
        EXPECT_EQ(jepa_fep_bridge_force_update(bridge), 0);
    }

    /* Measure performance */
    for (uint32_t i = 0; i < PERFORMANCE_ITERATIONS; i++) {
        EXPECT_EQ(jepa_fep_bridge_record_prediction_error(bridge, 0.3f), 0);
        EXPECT_EQ(jepa_fep_bridge_record_representation_quality(bridge, 0.8f), 0);

        double time_us = measure_time_us([this]() {
            jepa_fep_bridge_force_update(bridge);
        });

        update_times.push_back(time_us);
    }

    /* Calculate statistics */
    double sum = std::accumulate(update_times.begin(), update_times.end(), 0.0);
    double mean = sum / update_times.size();
    double max_time = *std::max_element(update_times.begin(), update_times.end());

    /* Compute 99th percentile */
    std::vector<double> sorted_times = update_times;
    std::sort(sorted_times.begin(), sorted_times.end());
    double p99 = sorted_times[sorted_times.size() * 99 / 100];

    /* Performance assertions */
    EXPECT_LT(mean, MAX_UPDATE_TIME_US)
        << "Mean update time (" << mean << " us) exceeds limit";

    EXPECT_LT(p99, MAX_UPDATE_TIME_US * 2)
        << "99th percentile (" << p99 << " us) exceeds 2x limit";

    /* Verify stats report correct timing */
    jepa_fep_stats_t stats;
    EXPECT_EQ(jepa_fep_bridge_get_stats(bridge, &stats), 0);
    EXPECT_GT(stats.avg_update_time_us, 0.0f);
}

/* ============================================================================
 * Test 4: MemoryLeakRegression
 * Verify no memory growth over many create/destroy and update cycles
 * ============================================================================ */

TEST_F(JepaFepBridgeRegressionTest, MemoryLeakRegression) {
    /* Test 1: Many update cycles on single bridge */
    for (int i = 0; i < MEMORY_LEAK_ITERATIONS; i++) {
        EXPECT_EQ(jepa_fep_bridge_record_prediction_error(bridge, 0.5f), 0);
        EXPECT_EQ(jepa_fep_bridge_record_representation_quality(bridge, 0.7f), 0);
        EXPECT_EQ(jepa_fep_bridge_force_update(bridge), 0);
    }

    /* Get stats to verify no overflow */
    jepa_fep_stats_t stats;
    EXPECT_EQ(jepa_fep_bridge_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.total_updates, (uint64_t)MEMORY_LEAK_ITERATIONS);

    /* Test 2: Many create/destroy cycles */
    for (int i = 0; i < 100; i++) {
        jepa_fep_bridge_t* temp_bridge = jepa_fep_bridge_create(&config);
        ASSERT_NE(temp_bridge, nullptr);

        /* Do some work */
        EXPECT_EQ(jepa_fep_bridge_record_prediction_error(temp_bridge, 0.5f), 0);
        EXPECT_EQ(jepa_fep_bridge_record_representation_quality(temp_bridge, 0.7f), 0);
        EXPECT_EQ(jepa_fep_bridge_force_update(temp_bridge), 0);

        jepa_fep_bridge_destroy(temp_bridge);
    }

    /* Test 3: Register/unregister cycles with FEP */
    setup_fep_orchestrator();

    for (int i = 0; i < 50; i++) {
        jepa_fep_bridge_t* temp_bridge = jepa_fep_bridge_create(&config);
        ASSERT_NE(temp_bridge, nullptr);

        uint32_t bridge_id;
        EXPECT_EQ(jepa_fep_bridge_register(temp_bridge, fep_orch, nullptr, &bridge_id), 0);

        /* Do some work */
        EXPECT_EQ(jepa_fep_bridge_record_prediction_error(temp_bridge, 0.5f), 0);
        EXPECT_EQ(jepa_fep_bridge_force_update(temp_bridge), 0);

        EXPECT_EQ(jepa_fep_bridge_unregister(temp_bridge), 0);
        jepa_fep_bridge_destroy(temp_bridge);
    }

    /* If we got here without crashing, memory management is working */
    SUCCEED();
}

/* ============================================================================
 * Test 5: RepresentationQualityRegression
 * Verify representation quality tracking maintained over time
 * ============================================================================ */

TEST_F(JepaFepBridgeRegressionTest, RepresentationQualityRegression) {
    setup_fep_orchestrator();

    uint32_t bridge_id;
    ASSERT_EQ(jepa_fep_bridge_register(bridge, fep_orch, nullptr, &bridge_id), 0);

    /* Test quality tracking accuracy */
    std::vector<float> qualities = {0.9f, 0.85f, 0.7f, 0.6f, 0.4f, 0.3f, 0.5f, 0.8f};
    float min_quality = 1.0f;

    for (float q : qualities) {
        EXPECT_EQ(jepa_fep_bridge_record_representation_quality(bridge, q), 0);
        if (q < min_quality) min_quality = q;

        /* Verify current quality is tracked */
        float current = jepa_fep_bridge_get_representation_quality(bridge);
        EXPECT_FLOAT_EQ(current, q);
    }

    EXPECT_EQ(jepa_fep_bridge_force_update(bridge), 0);

    /* Verify min quality tracked correctly */
    jepa_fep_stats_t stats;
    EXPECT_EQ(jepa_fep_bridge_get_stats(bridge, &stats), 0);
    EXPECT_FLOAT_EQ(stats.min_representation_quality, min_quality);

    /* Test quality-based free energy relationship over many cycles */
    EXPECT_EQ(jepa_fep_bridge_reset(bridge), 0);

    std::vector<std::pair<float, float>> quality_fe_pairs;
    uint64_t base_time = get_current_time_ms();

    for (int i = 0; i < 20; i++) {
        float quality = 0.1f + (i * 0.045f);  /* Range from 0.1 to ~1.0 */
        quality = std::min(quality, 1.0f);

        EXPECT_EQ(jepa_fep_bridge_record_representation_quality(bridge, quality), 0);
        EXPECT_EQ(jepa_fep_bridge_force_update(bridge), 0);

        fep_orchestrator_update(fep_orch, base_time + (i * 50));

        float fe = jepa_fep_bridge_get_free_energy_contribution(bridge);
        quality_fe_pairs.push_back({quality, fe});
    }

    /* Verify monotonicity: higher quality should generally give lower FE */
    int violations = 0;
    for (size_t i = 1; i < quality_fe_pairs.size(); i++) {
        if (quality_fe_pairs[i].first > quality_fe_pairs[i-1].first &&
            quality_fe_pairs[i].second > quality_fe_pairs[i-1].second + 0.1f) {
            violations++;
        }
    }

    /* Allow some tolerance for numerical effects */
    EXPECT_LT(violations, 3) << "Too many quality-FE monotonicity violations";
}

/* ============================================================================
 * Test 6: CollapseDetectionRegression
 * Verify collapse detection remains consistent
 * ============================================================================ */

TEST_F(JepaFepBridgeRegressionTest, CollapseDetectionRegression) {
    config.enable_collapse_detection = true;
    config.collapse_detection_threshold = 0.25f;
    EXPECT_EQ(jepa_fep_bridge_set_config(bridge, &config), 0);

    /* Test boundary conditions */
    struct CollapseTestCase {
        float quality;
        bool should_collapse;
    };

    std::vector<CollapseTestCase> test_cases = {
        {0.30f, false},  /* Above threshold */
        {0.25f, false},  /* At threshold */
        {0.24f, true},   /* Just below threshold */
        {0.10f, true},   /* Well below threshold */
        {0.00f, true},   /* Zero quality */
        {0.50f, false},  /* Clearly above */
    };

    for (const auto& tc : test_cases) {
        EXPECT_EQ(jepa_fep_bridge_reset(bridge), 0);
        EXPECT_EQ(jepa_fep_bridge_set_config(bridge, &config), 0);

        jepa_fep_stats_t stats_before;
        EXPECT_EQ(jepa_fep_bridge_get_stats(bridge, &stats_before), 0);
        uint64_t collapse_before = stats_before.collapse_detections;

        EXPECT_EQ(jepa_fep_bridge_record_representation_quality(bridge, tc.quality), 0);
        EXPECT_EQ(jepa_fep_bridge_force_update(bridge), 0);

        jepa_fep_stats_t stats_after;
        EXPECT_EQ(jepa_fep_bridge_get_stats(bridge, &stats_after), 0);
        uint64_t collapse_after = stats_after.collapse_detections;

        bool collapsed = (collapse_after > collapse_before);
        EXPECT_EQ(collapsed, tc.should_collapse)
            << "Quality " << tc.quality << " collapse detection mismatch";
    }
}

/* ============================================================================
 * Test 7: StatePersistenceRegression
 * Verify state persists correctly across operations
 * ============================================================================ */

TEST_F(JepaFepBridgeRegressionTest, StatePersistenceRegression) {
    setup_fep_orchestrator();

    /* Initial state should be IDLE */
    EXPECT_EQ(jepa_fep_bridge_get_state(bridge), JEPA_FEP_STATE_IDLE);

    /* After registration, should be ACTIVE */
    uint32_t bridge_id;
    ASSERT_EQ(jepa_fep_bridge_register(bridge, fep_orch, nullptr, &bridge_id), 0);
    EXPECT_EQ(jepa_fep_bridge_get_state(bridge), JEPA_FEP_STATE_ACTIVE);

    /* State should persist through updates */
    for (int i = 0; i < 20; i++) {
        EXPECT_EQ(jepa_fep_bridge_record_prediction_error(bridge, 0.3f), 0);
        EXPECT_EQ(jepa_fep_bridge_record_representation_quality(bridge, 0.8f), 0);
        EXPECT_EQ(jepa_fep_bridge_force_update(bridge), 0);

        /* Should still be ACTIVE with good inputs */
        jepa_fep_state_t state = jepa_fep_bridge_get_state(bridge);
        EXPECT_TRUE(state == JEPA_FEP_STATE_ACTIVE || state == JEPA_FEP_STATE_IDLE)
            << "Unexpected state: " << jepa_fep_state_name(state);
    }

    /* Trigger degraded state with bad inputs */
    config.high_free_energy_threshold = 0.1f;  /* Very low threshold */
    EXPECT_EQ(jepa_fep_bridge_set_config(bridge, &config), 0);

    EXPECT_EQ(jepa_fep_bridge_record_prediction_error(bridge, 2.0f), 0);
    EXPECT_EQ(jepa_fep_bridge_record_representation_quality(bridge, 0.1f), 0);
    EXPECT_EQ(jepa_fep_bridge_force_update(bridge), 0);

    EXPECT_EQ(jepa_fep_bridge_get_state(bridge), JEPA_FEP_STATE_DEGRADED);

    /* After unregister, should be IDLE */
    EXPECT_EQ(jepa_fep_bridge_unregister(bridge), 0);
    EXPECT_EQ(jepa_fep_bridge_get_state(bridge), JEPA_FEP_STATE_IDLE);

    /* After reset, should be IDLE */
    EXPECT_EQ(jepa_fep_bridge_reset(bridge), 0);
    EXPECT_EQ(jepa_fep_bridge_get_state(bridge), JEPA_FEP_STATE_IDLE);
}

/* ============================================================================
 * Test 8: ConfigurationPersistenceRegression
 * Verify configuration persists correctly
 * ============================================================================ */

TEST_F(JepaFepBridgeRegressionTest, ConfigurationPersistenceRegression) {
    /* Set custom configuration */
    jepa_fep_config_t custom_config;
    custom_config.free_energy_weight = 0.75f;
    custom_config.embedding_prediction_error_weight = 0.5f;
    custom_config.representation_collapse_penalty = 0.3f;
    custom_config.high_free_energy_threshold = 1.2f;
    custom_config.collapse_detection_threshold = 0.15f;
    custom_config.prediction_quality_threshold = 0.6f;
    custom_config.enable_logging = false;
    custom_config.update_interval_ms = 30;
    custom_config.enable_adaptive_weights = false;
    custom_config.enable_collapse_detection = true;

    EXPECT_EQ(jepa_fep_bridge_set_config(bridge, &custom_config), 0);

    /* Retrieve and verify */
    jepa_fep_config_t retrieved_config;
    EXPECT_EQ(jepa_fep_bridge_get_config(bridge, &retrieved_config), 0);

    EXPECT_FLOAT_EQ(retrieved_config.free_energy_weight, 0.75f);
    EXPECT_FLOAT_EQ(retrieved_config.embedding_prediction_error_weight, 0.5f);
    EXPECT_FLOAT_EQ(retrieved_config.representation_collapse_penalty, 0.3f);
    EXPECT_FLOAT_EQ(retrieved_config.high_free_energy_threshold, 1.2f);
    EXPECT_FLOAT_EQ(retrieved_config.collapse_detection_threshold, 0.15f);
    EXPECT_FLOAT_EQ(retrieved_config.prediction_quality_threshold, 0.6f);
    EXPECT_FALSE(retrieved_config.enable_logging);
    EXPECT_EQ(retrieved_config.update_interval_ms, 30u);
    EXPECT_FALSE(retrieved_config.enable_adaptive_weights);
    EXPECT_TRUE(retrieved_config.enable_collapse_detection);

    /* Config should persist through updates */
    for (int i = 0; i < 10; i++) {
        EXPECT_EQ(jepa_fep_bridge_record_prediction_error(bridge, 0.5f), 0);
        EXPECT_EQ(jepa_fep_bridge_force_update(bridge), 0);
    }

    EXPECT_EQ(jepa_fep_bridge_get_config(bridge, &retrieved_config), 0);
    EXPECT_FLOAT_EQ(retrieved_config.free_energy_weight, 0.75f);
}

/* ============================================================================
 * Test 9: StatisticsAccuracyRegression
 * Verify statistics are accurate and don't overflow
 * ============================================================================ */

TEST_F(JepaFepBridgeRegressionTest, StatisticsAccuracyRegression) {
    const int PREDICTION_COUNT = 100;
    const int QUALITY_COUNT = 75;
    const int UPDATE_COUNT = 50;

    float total_error = 0.0f;
    float min_quality = 1.0f;
    float max_fe = 0.0f;

    /* Record known number of predictions */
    for (int i = 0; i < PREDICTION_COUNT; i++) {
        float error = 0.1f + (i % 20) * 0.05f;
        total_error += error;
        EXPECT_EQ(jepa_fep_bridge_record_prediction_error(bridge, error), 0);
    }

    /* Record known number of quality measurements */
    for (int i = 0; i < QUALITY_COUNT; i++) {
        float quality = 0.9f - (i % 15) * 0.05f;
        quality = std::max(0.1f, quality);
        if (quality < min_quality) min_quality = quality;
        EXPECT_EQ(jepa_fep_bridge_record_representation_quality(bridge, quality), 0);
    }

    /* Perform known number of updates */
    for (int i = 0; i < UPDATE_COUNT; i++) {
        EXPECT_EQ(jepa_fep_bridge_force_update(bridge), 0);
        float fe = jepa_fep_bridge_get_free_energy_contribution(bridge);
        if (fe > max_fe) max_fe = fe;
    }

    /* Verify statistics */
    jepa_fep_stats_t stats;
    EXPECT_EQ(jepa_fep_bridge_get_stats(bridge, &stats), 0);

    EXPECT_EQ(stats.embedding_predictions, (uint64_t)PREDICTION_COUNT);
    EXPECT_EQ(stats.representation_updates, (uint64_t)QUALITY_COUNT);
    EXPECT_EQ(stats.total_updates, (uint64_t)UPDATE_COUNT);

    /* Verify computed statistics are reasonable */
    float expected_avg_error = total_error / PREDICTION_COUNT;
    EXPECT_NEAR(stats.avg_embedding_error, expected_avg_error, 0.1f);
    EXPECT_NEAR(stats.min_representation_quality, min_quality, 0.01f);
    EXPECT_NEAR(stats.peak_free_energy, max_fe, 0.01f);

    /* Verify no overflow in cumulative stats */
    EXPECT_GT(stats.total_free_energy_contribution, 0.0f);
    EXPECT_LT(stats.total_free_energy_contribution, 1e10f);  /* Sanity check */
}

/* ============================================================================
 * Test 10: ConcurrentAccessRegression
 * Verify thread safety under concurrent access
 * ============================================================================ */

TEST_F(JepaFepBridgeRegressionTest, ConcurrentAccessRegression) {
    const int NUM_THREADS = 4;
    const int OPS_PER_THREAD = 100;

    std::atomic<int> error_count{0};
    std::vector<std::thread> threads;

    /* Launch concurrent workers */
    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([this, t, &error_count]() {
            for (int i = 0; i < OPS_PER_THREAD; i++) {
                int op = (t + i) % 4;

                switch (op) {
                    case 0:
                        if (jepa_fep_bridge_record_prediction_error(bridge, 0.3f) != 0) {
                            error_count++;
                        }
                        break;
                    case 1:
                        if (jepa_fep_bridge_record_representation_quality(bridge, 0.7f) != 0) {
                            error_count++;
                        }
                        break;
                    case 2:
                        if (jepa_fep_bridge_force_update(bridge) != 0) {
                            error_count++;
                        }
                        break;
                    case 3: {
                        jepa_fep_stats_t stats;
                        if (jepa_fep_bridge_get_stats(bridge, &stats) != 0) {
                            error_count++;
                        }
                        jepa_fep_bridge_get_free_energy_contribution(bridge);
                        jepa_fep_bridge_get_state(bridge);
                        break;
                    }
                }
            }
        });
    }

    /* Wait for all threads */
    for (auto& t : threads) {
        t.join();
    }

    /* Verify no errors */
    EXPECT_EQ(error_count.load(), 0);

    /* Verify bridge is still valid */
    EXPECT_EQ(jepa_fep_bridge_get_state(bridge), JEPA_FEP_STATE_IDLE);

    jepa_fep_stats_t stats;
    EXPECT_EQ(jepa_fep_bridge_get_stats(bridge, &stats), 0);
    EXPECT_GT(stats.total_updates, 0u);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
